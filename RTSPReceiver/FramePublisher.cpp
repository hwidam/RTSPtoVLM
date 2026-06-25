#include "FramePublisher.h"
#include "../common/Logger.h"
#include <cstring>

static constexpr uint32_t SHM_TYPE_FRAME = 1;

bool FramePublisher::Init(const std::string& name, int width, int height, int slotCount)
{
    m_maxWidth  = width;
    m_maxHeight = height;

    const uint32_t slotSize = (uint32_t)(sizeof(ShmFrameHeader) + width * height * 3);
    m_sendBuf.resize(slotSize);

    if (!m_shm.Create(name, (uint32_t)slotCount, slotSize)) {
        Logger::Error("FramePublisher: failed to create shared memory '{}'", name);
        return false;
    }
    Logger::Info("FramePublisher: created '{}' ({} slots × {} bytes)", name, slotCount, slotSize);
    return true;
}

bool FramePublisher::Publish(const DecodedFrame& frame)
{
    if (!m_shm.IsOpen()) return false;

    const uint32_t pixBytes = (uint32_t)(frame.stride * frame.height);
    const uint32_t total    = (uint32_t)sizeof(ShmFrameHeader) + pixBytes;

    if (frame.width  > m_maxWidth ||
        frame.height > m_maxHeight ||
        total        > (uint32_t)m_sendBuf.size())
    {
        Logger::Warn("FramePublisher: frame {}x{} exceeds slot capacity {}x{}",
                     frame.width, frame.height, m_maxWidth, m_maxHeight);
        return false;
    }

    ShmFrameHeader* hdr = reinterpret_cast<ShmFrameHeader*>(m_sendBuf.data());
    hdr->width     = frame.width;
    hdr->height    = frame.height;
    hdr->stride    = frame.stride;
    hdr->format    = frame.format;
    hdr->pts       = frame.pts;
    hdr->timestamp = frame.timestamp;

    std::memcpy(m_sendBuf.data() + sizeof(ShmFrameHeader),
                frame.data.data(), pixBytes);

    return m_shm.Push(SHM_TYPE_FRAME, m_sendBuf.data(), total);
}
