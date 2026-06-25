#include "FrameDecoder.h"
#include "../common/Logger.h"
#include <chrono>

extern "C" {
#include <libavutil/imgutils.h>
#include <libavutil/time.h>
}

static uint64_t NowMs()
{
    using namespace std::chrono;
    return (uint64_t)duration_cast<milliseconds>(
        system_clock::now().time_since_epoch()).count();
}

bool FrameDecoder::Init(AVCodecParameters* params, AVRational timeBase)
{
    Close();

    const AVCodec* codec = avcodec_find_decoder(params->codec_id);
    if (!codec) {
        Logger::Error("FrameDecoder: no decoder for codec_id {}", (int)params->codec_id);
        return false;
    }

    m_codecCtx = avcodec_alloc_context3(codec);
    if (!m_codecCtx) return false;

    int ret = avcodec_parameters_to_context(m_codecCtx, params);
    if (ret < 0) {
        Logger::Error("FrameDecoder: avcodec_parameters_to_context failed");
        Close();
        return false;
    }

    // Request multi-threaded decoding when available
    m_codecCtx->thread_count = 0; // auto-detect core count
    m_codecCtx->thread_type  = FF_THREAD_FRAME | FF_THREAD_SLICE;

    ret = avcodec_open2(m_codecCtx, codec, nullptr);
    if (ret < 0) {
        char err[128]; av_strerror(ret, err, sizeof(err));
        Logger::Error("FrameDecoder: avcodec_open2 failed: {}", err);
        Close();
        return false;
    }

    m_avFrame  = av_frame_alloc();
    m_bgrFrame = av_frame_alloc();
    if (!m_avFrame || !m_bgrFrame) { Close(); return false; }

    m_timeBase = timeBase;
    Logger::Info("FrameDecoder: decoder '{}' opened", codec->name);
    return true;
}

void FrameDecoder::BuildSwsContext()
{
    if (m_swsCtx) { sws_freeContext(m_swsCtx); m_swsCtx = nullptr; }

    m_width  = m_codecCtx->width;
    m_height = m_codecCtx->height;

    m_swsCtx = sws_getContext(
        m_width, m_height, m_codecCtx->pix_fmt,
        m_width, m_height, AV_PIX_FMT_BGR24,
        SWS_BILINEAR, nullptr, nullptr, nullptr);
}

bool FrameDecoder::ReceiveFrame(DecodedFrame& out)
{
    int ret = avcodec_receive_frame(m_codecCtx, m_avFrame);
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) return false;
    if (ret < 0) {
        char err[128]; av_strerror(ret, err, sizeof(err));
        Logger::Warn("FrameDecoder: avcodec_receive_frame: {}", err);
        return false;
    }

    // Build (or rebuild) the SwsContext when resolution becomes known or changes
    if (!m_swsCtx
        || m_codecCtx->width  != m_width
        || m_codecCtx->height != m_height)
    {
        BuildSwsContext();
    }

    const int stride   = m_width * 3; // BGR24: 3 bytes/pixel
    const int pixBytes = stride * m_height;

    out.data.resize(pixBytes);
    out.width     = m_width;
    out.height    = m_height;
    out.stride    = stride;
    out.format    = FRAME_FORMAT_BGR24;
    out.pts       = m_avFrame->pts;
    out.timestamp = NowMs();

    // sws_scale writes into out.data directly; provide aligned pointers
    uint8_t* dstData[1]  = { out.data.data() };
    int      dstStride[1]= { stride };
    sws_scale(m_swsCtx,
              m_avFrame->data, m_avFrame->linesize, 0, m_height,
              dstData, dstStride);

    av_frame_unref(m_avFrame);
    return true;
}

bool FrameDecoder::Decode(AVPacket* pkt, DecodedFrame& out)
{
    int ret = avcodec_send_packet(m_codecCtx, pkt);
    if (ret < 0 && ret != AVERROR(EAGAIN)) {
        char err[128]; av_strerror(ret, err, sizeof(err));
        Logger::Warn("FrameDecoder: avcodec_send_packet: {}", err);
        return false;
    }
    return ReceiveFrame(out);
}

bool FrameDecoder::Flush(DecodedFrame& out)
{
    // Send null packet to signal EOF to decoder
    avcodec_send_packet(m_codecCtx, nullptr);
    return ReceiveFrame(out);
}

void FrameDecoder::Close()
{
    if (m_swsCtx)   { sws_freeContext(m_swsCtx);           m_swsCtx   = nullptr; }
    if (m_bgrFrame) { av_frame_free(&m_bgrFrame);           m_bgrFrame = nullptr; }
    if (m_avFrame)  { av_frame_free(&m_avFrame);            m_avFrame  = nullptr; }
    if (m_codecCtx) { avcodec_free_context(&m_codecCtx);    m_codecCtx = nullptr; }
    m_width = m_height = 0;
}
