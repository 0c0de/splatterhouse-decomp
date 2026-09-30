#include "splatterhouse/memory.h"
#include <spdlog/spdlog.h>
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace splatterhouse::mem {

static uint8_t* g_base = nullptr;
static size_t   g_size = GUEST_MEM_SIZE;
static uint32_t g_bump = HEAP_BASE;

bool Initialize() {
#ifdef _WIN32
    g_base = (uint8_t*)VirtualAlloc(nullptr, g_size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (!g_base) {
        spdlog::error("VirtualAlloc {} MB fallo: {}", g_size / (1024*1024), GetLastError());
        return false;
    }
#else
    g_base = (uint8_t*)mmap(nullptr, g_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (g_base == MAP_FAILED) { g_base = nullptr; return false; }
#endif
    spdlog::info("[mem] Guest memory {} MB mapeada en host {:p} (guest 0x{:08X}-0x{:08X})",
        g_size/(1024*1024), (void*)g_base, GUEST_BASE_ADDR, GUEST_BASE_ADDR + (uint32_t)g_size);
    // Zero-inicializar
    memset(g_base, 0, g_size);
    g_bump = HEAP_BASE;
    return true;
}

void Shutdown() {
    if (!g_base) return;
#ifdef _WIN32
    VirtualFree(g_base, 0, MEM_RELEASE);
#else
    munmap(g_base, g_size);
#endif
    g_base = nullptr;
    spdlog::info("[mem] Guest memory liberada");
}

void* Translate(u32 guestAddr) {
    if (!g_base) return nullptr;
    // g_base mapea la dirección guest GUEST_BASE_ADDR (0x82000000)
    if (guestAddr < GUEST_BASE_ADDR) return nullptr;
    u32 offset = guestAddr - GUEST_BASE_ADDR;
    if (offset >= g_size) return nullptr;
    return g_base + offset;
}
const void* TranslateConst(u32 guestAddr) {
    if (!g_base) return nullptr;
    if (guestAddr < GUEST_BASE_ADDR) return nullptr;
    u32 offset = guestAddr - GUEST_BASE_ADDR;
    if (offset >= g_size) return nullptr;
    return g_base + offset;
}

uint8_t* GetBase() { return g_base; }

u32 ReadBE32(u32 guestAddr) {
    auto* p = (uint32_t*)Translate(guestAddr);
    if (!p) return 0;
    return bswap32(*p);
}
void WriteBE32(u32 guestAddr, u32 value) {
    auto* p = (uint32_t*)Translate(guestAddr);
    if (!p) return;
    *p = bswap32(value);
}
u64 ReadBE64(u32 guestAddr) { auto* p = (uint64_t*)Translate(guestAddr); return p ? bswap64(*p) : 0; }
void WriteBE64(u32 guestAddr, u64 value) { auto* p = (uint64_t*)Translate(guestAddr); if(p) *p = bswap64(value); }

u32 Alloc(u32 size, u32 align) {
    // Alineamiento power-of-two
    g_bump = (g_bump + (align - 1)) & ~(align - 1);
    u32 addr = g_bump;
    g_bump += size;
    // Check contra el final de la región guest mapeada (GUEST_BASE_ADDR + g_size)
    if (g_bump >= GUEST_BASE_ADDR + g_size || addr < GUEST_BASE_ADDR) {
        spdlog::error("[mem] Out of guest memory! bump=0x{:08X} size={}", addr, size);
        g_bump = addr; // revertir
        return 0;
    }
    memset(Translate(addr), 0, size);
    return addr;
}
void Free(u32) {
    // TODO: free list real. Por ahora bump allocator sin free (suficiente para bootstrap).
}

std::string ReadString(u32 guestAddr) {
    std::string out;
    for (u32 i = 0; i < 4096; ++i) {
        auto* p = (char*)Translate(guestAddr + i);
        if (!p || *p == '\0') break;
        out.push_back(*p);
    }
    return out;
}
std::string ReadString(u32 guestAddr, size_t maxLen) {
    std::string out; out.reserve(maxLen);
    for (size_t i = 0; i < maxLen; ++i) {
        auto* p = (char*)Translate(guestAddr + (u32)i);
        if (!p || *p == '\0') break;
        out.push_back(*p);
    }
    return out;
}

} // namespace splatterhouse::mem
