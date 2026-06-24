#include "../common/Logger.h"
#include "RtspSource.h"
#include "FrameDecoder.h"
#include "FramePublisher.h"

#include <atomic>
#include <csignal>
#include <iostream>

extern "C" {
#include <libavformat/avformat.h>
}

static std::atomic<bool> g_running{ true };

static void OnSignal(int) { g_running = false; }

int main(int argc, char* argv[])
{
    Logger::Init("RTSPReceiver");

    const char* url = (argc > 1) ? argv[1] : "rtsp://admin:admin@221.165.29.188:554/test.mp4";
    const char* shmName = (argc > 2) ? argv[2] : "RtspFrame";

    Logger::Info("RTSPReceiver starting: url={} shm={}", url, shmName);

    std::signal(SIGINT,  OnSignal);
    std::signal(SIGTERM, OnSignal);

    // ── Open source ───────────────────────────────────────────────────────────
    RtspSource source;
    if (!source.Open(url)) {
        Logger::Error("Failed to open RTSP source");
        return 1;
    }

    // ── Init decoder ──────────────────────────────────────────────────────────
    FrameDecoder decoder;
    if (!decoder.Init(source.GetCodecParameters(), source.GetTimeBase())) {
        Logger::Error("Failed to init decoder");
        return 1;
    }

    // ── Init publisher ────────────────────────────────────────────────────────
    // Resolve actual frame dimensions from the codec context.
    // If not yet known (0x0), fall back to a safe 1920x1080 upper bound.
    int width  = (decoder.Width()  > 0) ? decoder.Width()  : 1920;
    int height = (decoder.Height() > 0) ? decoder.Height() : 1080;

    FramePublisher publisher;
    if (!publisher.Init(shmName, width, height, 4)) {
        Logger::Error("Failed to init frame publisher");
        return 1;
    }

    // ── Main loop ─────────────────────────────────────────────────────────────
    AVPacket* pkt = av_packet_alloc();
    DecodedFrame frame;

    Logger::Info("RTSPReceiver: running (Ctrl+C to stop)");

    while (g_running) {
        if (!source.ReadPacket(pkt)) {
            Logger::Warn("RTSPReceiver: stream ended or read error");
            break;
        }

        if (decoder.Decode(pkt, frame))
            publisher.Publish(frame);

        av_packet_unref(pkt);
    }

    // ── Flush remaining buffered frames ───────────────────────────────────────
    while (decoder.Flush(frame))
        publisher.Publish(frame);

    av_packet_free(&pkt);
    Logger::Info("RTSPReceiver: stopped");
    return 0;
}
