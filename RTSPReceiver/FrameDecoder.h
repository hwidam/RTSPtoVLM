#pragma once
#include "../common/FrameTypes.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
}

// Decodes AVPackets to BGR24 DecodedFrames using the software decoder.
// One FrameDecoder instance manages one video stream.
class FrameDecoder {
public:
    FrameDecoder()  = default;
    ~FrameDecoder() { Close(); }

    FrameDecoder(const FrameDecoder&)            = delete;
    FrameDecoder& operator=(const FrameDecoder&) = delete;

    // params    — from RtspSource::GetCodecParameters()
    // timeBase  — from RtspSource::GetTimeBase()
    bool Init(AVCodecParameters* params, AVRational timeBase);

    // Decode one packet. Returns true and fills |out| when a frame is ready.
    // May return false for the first few packets (B-frame buffering); that is
    // normal — just keep calling with subsequent packets.
    bool Decode(AVPacket* pkt, DecodedFrame& out);

    // Drain remaining buffered frames after the last packet.
    // Call in a loop until it returns false.
    bool Flush(DecodedFrame& out);

    void Close();

    int Width()  const { return m_width;  }
    int Height() const { return m_height; }

private:
    bool ReceiveFrame(DecodedFrame& out);
    void BuildSwsContext();

    AVCodecContext* m_codecCtx = nullptr;
    AVFrame*        m_avFrame  = nullptr;
    AVFrame*        m_bgrFrame = nullptr;
    SwsContext*     m_swsCtx   = nullptr;
    AVRational      m_timeBase = {1, 1};
    int             m_width    = 0;
    int             m_height   = 0;
};
