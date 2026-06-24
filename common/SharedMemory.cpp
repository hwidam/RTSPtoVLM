#include "pch.h"
#include "SharedMemory.h"
#include <chrono>
#include <cstring>

// ── Shared-memory layout ──────────────────────────────────────────────────────
//
//   [ RingHeader ]
//   [ SlotHeader | data[slotSize] ]  ← slot 0
//   [ SlotHeader | data[slotSize] ]  ← slot 1
//   ...
//   [ SlotHeader | data[slotSize] ]  ← slot N-1
//
// All fields are plain integers — no pointers stored in shared memory.
// Each process adds its own m_view base address to reach any slot.

static constexpr uint32_t SHM_MAGIC = 0x52494E47; // 'RING'

#pragma pack(push, 1)
struct RingHeader {
    uint32_t magic;
    uint32_t headerSize;  // = sizeof(RingHeader), checked on Open
    uint32_t slotCount;
    uint32_t slotSize;    // max bytes of payload per slot
    uint32_t head;        // next slot to write (producer advances)
    uint32_t tail;        // next slot to read  (consumer advances)
    uint32_t count;       // number of occupied slots
};

struct SlotHeader {
    uint32_t type;
    uint64_t timestamp;   // ms since Unix epoch
    uint32_t size;        // actual payload bytes stored
};
#pragma pack(pop)

// ── Helpers ───────────────────────────────────────────────────────────────────

static uint64_t NowMs()
{
    using namespace std::chrono;
    return (uint64_t)duration_cast<milliseconds>(
        system_clock::now().time_since_epoch()).count();
}

static std::string MapName  (const std::string& n) { return "Local\\" + n + "_shm";   }
static std::string MutexName(const std::string& n) { return "Local\\" + n + "_mutex"; }
static std::string EventName(const std::string& n) { return "Local\\" + n + "_event"; }

static inline RingHeader* ringHdr(void* v)
{
    return reinterpret_cast<RingHeader*>(v);
}

// Returns pointer to the start of slot idx (i.e. its SlotHeader).
static inline uint8_t* slotPtr(void* v, uint32_t idx, uint32_t slotSize)
{
    return reinterpret_cast<uint8_t*>(v)
        + sizeof(RingHeader)
        + idx * (sizeof(SlotHeader) + slotSize);
}

// ── Close ─────────────────────────────────────────────────────────────────────

void SharedMemory::Close()
{
    if (m_view)   { UnmapViewOfFile(m_view);       m_view   = nullptr; }
    if (m_hMap)   { CloseHandle((HANDLE)m_hMap);   m_hMap   = nullptr; }
    if (m_hMutex) { CloseHandle((HANDLE)m_hMutex); m_hMutex = nullptr; }
    if (m_hEvent) { CloseHandle((HANDLE)m_hEvent); m_hEvent = nullptr; }
    m_readBuf.clear();
}

// ── Create ────────────────────────────────────────────────────────────────────

bool SharedMemory::Create(const std::string& name, uint32_t slotCount, uint32_t slotSize)
{
    Close();

    HANDLE hMutex = CreateMutexA(nullptr, FALSE, MutexName(name).c_str());
    if (!hMutex) return false;
    m_hMutex = hMutex;

    HANDLE hEvent = CreateEventA(nullptr, FALSE, FALSE, EventName(name).c_str());
    if (!hEvent) { Close(); return false; }
    m_hEvent = hEvent;

    const size_t total = sizeof(RingHeader)
                       + (size_t)slotCount * (sizeof(SlotHeader) + slotSize);

    HANDLE hMap = CreateFileMappingA(INVALID_HANDLE_VALUE, nullptr,
                                     PAGE_READWRITE,
                                     (DWORD)(total >> 32),
                                     (DWORD)(total & 0xFFFFFFFF),
                                     MapName(name).c_str());
    if (!hMap) { Close(); return false; }
    m_hMap = hMap;

    m_view = MapViewOfFile(hMap, FILE_MAP_ALL_ACCESS, 0, 0, total);
    if (!m_view) { Close(); return false; }

    RingHeader* h = ringHdr(m_view);
    h->magic      = SHM_MAGIC;
    h->headerSize = sizeof(RingHeader);
    h->slotCount  = slotCount;
    h->slotSize   = slotSize;
    h->head       = 0;
    h->tail       = 0;
    h->count      = 0;

    return true;
}

// ── Open ──────────────────────────────────────────────────────────────────────

bool SharedMemory::Open(const std::string& name)
{
    Close();

    HANDLE hMutex = OpenMutexA(SYNCHRONIZE, FALSE, MutexName(name).c_str());
    if (!hMutex) return false;
    m_hMutex = hMutex;

    HANDLE hEvent = OpenEventA(SYNCHRONIZE | EVENT_MODIFY_STATE, FALSE,
                               EventName(name).c_str());
    if (!hEvent) { Close(); return false; }
    m_hEvent = hEvent;

    HANDLE hMap = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, MapName(name).c_str());
    if (!hMap) { Close(); return false; }
    m_hMap = hMap;

    m_view = MapViewOfFile(hMap, FILE_MAP_ALL_ACCESS, 0, 0, 0);
    if (!m_view) { Close(); return false; }

    const RingHeader* h = ringHdr(m_view);
    if (h->magic != SHM_MAGIC || h->headerSize != sizeof(RingHeader)) {
        Close();
        return false;
    }

    return true;
}

// ── Push ──────────────────────────────────────────────────────────────────────

bool SharedMemory::Push(uint32_t type, const uint8_t* data, uint32_t size)
{
    if (!m_view || !data || size == 0) return false;

    RingHeader* h = ringHdr(m_view);
    if (size > h->slotSize) return false;

    WaitForSingleObject((HANDLE)m_hMutex, INFINITE);

    if (h->count == h->slotCount) {
        // Ring full — discard oldest to make room
        h->tail  = (h->tail + 1) % h->slotCount;
        h->count--;
    }

    uint8_t* slot         = slotPtr(m_view, h->head, h->slotSize);
    SlotHeader* sh        = reinterpret_cast<SlotHeader*>(slot);
    sh->type              = type;
    sh->timestamp         = NowMs();
    sh->size              = size;
    std::memcpy(slot + sizeof(SlotHeader), data, size);

    h->head  = (h->head + 1) % h->slotCount;
    h->count++;

    ReleaseMutex((HANDLE)m_hMutex);
    SetEvent((HANDLE)m_hEvent);
    return true;
}

// ── Pop (non-blocking) ────────────────────────────────────────────────────────

bool SharedMemory::Pop(ShmPacket& out)
{
    if (!m_view) return false;

    WaitForSingleObject((HANDLE)m_hMutex, INFINITE);

    RingHeader* h = ringHdr(m_view);
    bool ok = (h->count > 0);
    if (ok) {
        uint8_t* slot  = slotPtr(m_view, h->tail, h->slotSize);
        SlotHeader* sh = reinterpret_cast<SlotHeader*>(slot);

        m_readBuf.resize(sh->size);
        std::memcpy(m_readBuf.data(), slot + sizeof(SlotHeader), sh->size);

        out.type      = sh->type;
        out.timestamp = sh->timestamp;
        out.size      = sh->size;
        out.data      = m_readBuf.data();  // process-local pointer, safe

        h->tail  = (h->tail + 1) % h->slotCount;
        h->count--;
    }

    ReleaseMutex((HANDLE)m_hMutex);
    return ok;
}

// ── WaitAndPop ────────────────────────────────────────────────────────────────

bool SharedMemory::WaitAndPop(ShmPacket& out, uint32_t timeoutMs)
{
    if (!m_view) return false;
    if (Pop(out)) return true;  // already have data
    DWORD res = WaitForSingleObject((HANDLE)m_hEvent, timeoutMs);
    if (res == WAIT_OBJECT_0)
        return Pop(out);
    return false;
}

// ── IsEmpty / IsFull ──────────────────────────────────────────────────────────

bool SharedMemory::IsEmpty() const
{
    if (!m_view) return true;
    WaitForSingleObject((HANDLE)m_hMutex, INFINITE);
    bool empty = (ringHdr(m_view)->count == 0);
    ReleaseMutex((HANDLE)m_hMutex);
    return empty;
}

bool SharedMemory::IsFull() const
{
    if (!m_view) return false;
    WaitForSingleObject((HANDLE)m_hMutex, INFINITE);
    const RingHeader* h = ringHdr(m_view);
    bool full = (h->count == h->slotCount);
    ReleaseMutex((HANDLE)m_hMutex);
    return full;
}
