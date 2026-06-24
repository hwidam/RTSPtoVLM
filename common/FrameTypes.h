#pragma once
#include <vector>
#include <cstdint>

// ── Pixel format tags ─────────────────────────────────────────────────────────
static constexpr uint32_t FRAME_FORMAT_BGR24 = 0;

// ── Decoded frame (process-local, never stored in shared memory) ──────────────
struct DecodedFrame {
    std::vector<uint8_t> data;      // BGR24 pixels, row-major
    int      width     = 0;
    int      height    = 0;
    int      stride    = 0;         // bytes per row (width * 3 for BGR24)
    uint32_t format    = FRAME_FORMAT_BGR24;
    int64_t  pts       = 0;         // presentation timestamp (stream timebase units)
    uint64_t timestamp = 0;         // ms since Unix epoch, set at decode time
};

// ── Shared memory frame header ────────────────────────────────────────────────
// Written by RTSPReceiver (FramePublisher) as the first bytes of each ShmPacket.
// Read by VA to reconstruct a cv::Mat:
//   auto* h = pkt.As<ShmFrameHeader>();
//   cv::Mat frame(h->height, h->width, CV_8UC3, pkt.data + sizeof(ShmFrameHeader));
#pragma pack(push, 1)
struct ShmFrameHeader {
    int32_t  width;
    int32_t  height;
    int32_t  stride;
    uint32_t format;
    int64_t  pts;
    uint64_t timestamp;
};
#pragma pack(pop)