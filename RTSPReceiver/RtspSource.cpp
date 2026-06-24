#include "RtspSource.h"
#include "../common/Logger.h"

extern "C" {
#include <libavutil/dict.h>
}

bool RtspSource::Open(const std::string& url, const std::string& transport)
{
    Close();

    AVDictionary* opts = nullptr;
    av_dict_set(&opts, "rtsp_transport",    transport.c_str(), 0);
    av_dict_set(&opts, "max_delay",         "500000",          0); // 500 ms
    av_dict_set(&opts, "stimeout",          "5000000",         0); // 5 s connect timeout
    av_dict_set(&opts, "analyzeduration",   "1000000",         0); // 1 s
    av_dict_set(&opts, "probesize",         "1000000",         0);

    int ret = avformat_open_input(&m_fmtCtx, url.c_str(), nullptr, &opts);
    av_dict_free(&opts);
    if (ret < 0) {
        char err[128]; av_strerror(ret, err, sizeof(err));
        Logger::Error("RtspSource: cannot open '{}': {}", url, err);
        return false;
    }

    ret = avformat_find_stream_info(m_fmtCtx, nullptr);
    if (ret < 0) {
        Logger::Error("RtspSource: avformat_find_stream_info failed");
        Close();
        return false;
    }

    m_videoStreamIdx = av_find_best_stream(m_fmtCtx, AVMEDIA_TYPE_VIDEO,
                                           -1, -1, nullptr, 0);
    if (m_videoStreamIdx < 0) {
        Logger::Error("RtspSource: no video stream found in '{}'", url);
        Close();
        return false;
    }

    Logger::Info("RtspSource: opened '{}' (video stream {})", url, m_videoStreamIdx);
    return true;
}

bool RtspSource::ReadPacket(AVPacket* pkt)
{
    if (!m_fmtCtx) return false;

    for (;;) {
        int ret = av_read_frame(m_fmtCtx, pkt);
        if (ret == AVERROR_EOF)      return false;
        if (ret == AVERROR(EAGAIN))  continue;
        if (ret < 0) {
            char err[128]; av_strerror(ret, err, sizeof(err));
            Logger::Warn("RtspSource: av_read_frame error: {}", err);
            return false;
        }
        if (pkt->stream_index == m_videoStreamIdx)
            return true;
        av_packet_unref(pkt); // skip non-video packets
    }
}

AVCodecParameters* RtspSource::GetCodecParameters() const
{
    if (!m_fmtCtx || m_videoStreamIdx < 0) return nullptr;
    return m_fmtCtx->streams[m_videoStreamIdx]->codecpar;
}

AVRational RtspSource::GetTimeBase() const
{
    if (!m_fmtCtx || m_videoStreamIdx < 0) return {1, 1};
    return m_fmtCtx->streams[m_videoStreamIdx]->time_base;
}

void RtspSource::Close()
{
    if (m_fmtCtx) {
        avformat_close_input(&m_fmtCtx);
        m_fmtCtx = nullptr;
    }
    m_videoStreamIdx = -1;
}
