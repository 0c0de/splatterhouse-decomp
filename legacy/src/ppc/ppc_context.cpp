#include "splatterhouse/common.h"
#include <spdlog/spdlog.h>
#include <cstring>

// PPC Context wrapper. Cuando XenonUtils esté disponible, incluirá:
//   #include <ppc/Context.h>
//   #include <ppc/MMU.h>
// Por ahora stub para que el proyecto compile sin dependencias.

namespace splatterhouse::ppc {

struct PPCContext {
    // GPRs 0-31, FPRs, CR, LR, CTR, XER, etc.
    uint64_t gpr[32]{};
    double   fpr[32]{};
    uint32_t cr = 0;
    uint32_t lr = 0;
    uint32_t ctr = 0;
    uint32_t xer = 0;
    uint32_t pc = 0;
    // MSR, etc.
};

void InitContext(PPCContext& ctx) {
    memset(&ctx, 0, sizeof(ctx));
    spdlog::debug("[ppc] Context init");
}

} // namespace splatterhouse::ppc
