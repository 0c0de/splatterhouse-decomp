#include "splatterhouse/common.h"
#include <spdlog/spdlog.h>

// MMIO handlers para direcciones especiales del guest (GPU ring, etc.)
// El recompilado llama a `ppc::MMIORead32(addr)` cuando accede a rangos no mapeados como RAM.

namespace splatterhouse::ppc::mmio {

uint32_t Read32(uint32_t guestAddr) {
    spdlog::trace("[mmio] Read32 0x{:08X}", guestAddr);
    // Rangos Xenos: 0x20000000+ etc.
    SH_TODO("MMIO Read32");
    return 0;
}
void Write32(uint32_t guestAddr, uint32_t value) {
    spdlog::trace("[mmio] Write32 0x{:08X} = 0x{:08X}", guestAddr, value);
    SH_TODO("MMIO Write32");
}

uint64_t Read64(uint32_t guestAddr) { return ((uint64_t)Read32(guestAddr) << 32) | Read32(guestAddr+4); }
void Write64(uint32_t guestAddr, uint64_t value) { Write32(guestAddr, (uint32_t)(value>>32)); Write32(guestAddr+4, (uint32_t)value); }

} // namespace splatterhouse::ppc::mmio
