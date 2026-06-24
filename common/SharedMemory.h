#pragma once
#include <cstdint>
#include <string>
#include <vector>

#ifdef COMMON_EXPORTS
#define COMMON_API __declspec(dllexport)
#else
#define COMMON_API __declspec(dllimport)
#endif

// ── Packet ────────────────────────────────────────────────────────────────────
// Returned by Pop(). data is a process-local pointer into an internal copy
// buffer — valid until the next Pop() or Close() on the same instance.
// (No pointers are stored inside shared memory — data is always copied out.)
struct ShmPacket {
    uint32_t  type;       // user-defined type ID
    uint64_t  timestamp;  // milliseconds since Unix epoch
    uint32_t  size;       // bytes in data
    uint8_t*  data;       // process-local pointer to payload copy

    bool IsValid() const { return data != nullptr && size > 0; }

    template<typename T>       T* As()       { return reinterpret_cast<T*>(data); }
    template<typename T> const T* As() const { return reinterpret_cast<const T*>(data); }
};

// ── SharedMemory ──────────────────────────────────────────────────────────────
// Ring buffer over a Windows named shared memory segment.
// No pointers are stored inside shared memory — only fixed-size integers
// (indices, sizes, type tags) so both processes can interpret the layout
// regardless of where each maps the segment in its virtual address space.
//
// PRODUCER:
//   SharedMemory shm;
//   shm.Create("MyStream", 4 /*slots*/, 1920*1080*3 /*bytes per slot*/);
//   shm.Push(1, data, size);       // overwrites oldest slot if ring is full
//
// CONSUMER:
//   SharedMemory shm;
//   shm.Open("MyStream");
//   ShmPacket pkt{};
//   if (shm.WaitAndPop(pkt, 33))   // block up to 33 ms (~30 fps)
//       auto* hdr = pkt.As<MyHeader>();

class COMMON_API SharedMemory {
public:
    SharedMemory()  = default;
    ~SharedMemory() { Close(); }

    SharedMemory(const SharedMemory&)            = delete;
    SharedMemory& operator=(const SharedMemory&) = delete;

    // ── Producer ──────────────────────────────────────────────────────────────

    // Create ring buffer: slotCount slots × slotSize bytes each.
    bool Create(const std::string& name, uint32_t slotCount, uint32_t slotSize);

    // Push one packet. Overwrites the oldest slot when the ring is full.
    // timestamp is set automatically to the current time.
    bool Push(uint32_t type, const uint8_t* data, uint32_t size);

    // ── Consumer ──────────────────────────────────────────────────────────────

    // Open an existing ring created by Create().
    bool Open(const std::string& name);

    // Non-blocking read. Returns false if the ring is empty.
    bool Pop(ShmPacket& out);

    // Block until a packet is available or timeoutMs elapses.
    bool WaitAndPop(ShmPacket& out, uint32_t timeoutMs = 1000);

    bool IsEmpty() const;
    bool IsFull()  const;

    // ── Common ────────────────────────────────────────────────────────────────

    void Close();
    bool IsOpen() const { return m_view != nullptr; }

private:
    void*  m_view   = nullptr;
    void*  m_hMap   = nullptr;
    void*  m_hMutex = nullptr;  // named kernel mutex (cross-process safe)
    void*  m_hEvent = nullptr;  // auto-reset event, signalled on each Push

    std::vector<uint8_t> m_readBuf;  // process-local copy buffer for Pop()
};
