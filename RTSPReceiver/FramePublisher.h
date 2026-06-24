#pragma once
#include "../common/FrameTypes.h"
#include "../common/SharedMemory.h"
#include <string>
#include <vector>

// Serialises DecodedFrames into SharedMemory as:
//   [ ShmFrameHeader ][ BGR24 pixels ]
// VA reads them back with:
//   auto* h = pkt.As<ShmFrameHeader>();
//   cv::Mat frame(h->height, h->width, CV_8UC3, pkt.data + sizeof(ShmFrameHeader));
class FramePublisher {
public:
    FramePublisher()  = default;
    ~FramePublisher() { Close(); }

    FramePublisher(const FramePublisher&)            = delete;
    FramePublisher& operator=(const FramePublisher&) = delete;

    // name      — shared memory segment name (same string VA must Open)
    // width/height — expected frame resolution; determines slot size
    // slotCount — ring buffer depth (4 slots is usually sufficient)
    bool Init(const std::string& name, int width, int height, int slotCount = 4);

    // Push one frame. Returns false if frame dimensions exceed slot capacity.
    bool Publish(const DecodedFrame& frame);

    void Close() { m_shm.Close(); }
    bool IsOpen() const { return m_shm.IsOpen(); }

private:
    SharedMemory          m_shm;
    int                   m_maxWidth  = 0;
    int                   m_maxHeight = 0;
    std::vector<uint8_t>  m_sendBuf;  // scratch buffer: header + pixels
};
