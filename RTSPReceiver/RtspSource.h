#pragma once
#include <string>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}

// Opens an RTSP (or any avformat) URL and exposes the video stream.
// Outputs raw AVPackets for the video stream; the caller must call
// av_packet_unref() after processing each packet.
class RtspSource {
public:
    RtspSource()  = default;
    ~RtspSource() { Close(); }

    RtspSource(const RtspSource&)            = delete;
    RtspSource& operator=(const RtspSource&) = delete;

    // url       — rtsp://... or any avformat-supported URL
    // transport — "tcp" (default, more reliable) or "udp"
    bool Open(const std::string& url, const std::string& transport = "tcp");

    // Read the next packet for the video stream.
    // Returns false on EOF or unrecoverable error.
    bool ReadPacket(AVPacket* pkt);

    AVCodecParameters* GetCodecParameters() const;
    AVRational         GetTimeBase()        const;
    int                GetVideoStreamIdx()  const { return m_videoStreamIdx; }

    void Close();
    bool IsOpen() const { return m_fmtCtx != nullptr; }

private:
    AVFormatContext* m_fmtCtx          = nullptr;
    int              m_videoStreamIdx  = -1;
};
