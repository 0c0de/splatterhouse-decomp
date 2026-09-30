#include "splatterhouse/common.h"
#include <spdlog/spdlog.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

// Scheduler cooperativo para threads PPC recompilados.
// XenonRecomp + XenonUtils exponen su propio threading (fiber based).
// Este fichero es un placeholder HLE hasta integrar XenonUtils correctamente.

namespace splatterhouse::threads {

struct GuestThread {
    uint32_t id = 0;
    uint32_t entry = 0; // guest addr
    uint32_t stackBase = 0;
    uint32_t stackSize = 0;
    int priority = 0;
    bool running = false;
};

static uint32_t s_nextId = 1;

uint32_t CreateThread(uint32_t entry, uint32_t stackSize, int priority) {
    spdlog::info("[threads] CreateThread entry=0x{:08X} stack=0x{:X} prio={}", entry, stackSize, priority);
    // TODO: alloc stack via mem::Alloc, crear fiber / std::jthread que ejecute recompiled func
    SH_TODO("Implementar threads guest con XenonUtils fibers");
    return s_nextId++;
}

void ExitThread(uint32_t id) {
    spdlog::info("[threads] ExitThread id={}", id);
}

void Sleep(uint32_t ms) {
#ifdef _WIN32
    ::Sleep(ms);
#else
    usleep(ms * 1000);
#endif
}

} // namespace splatterhouse::threads
