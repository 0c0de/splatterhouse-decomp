#include "splatterhouse/hle_kernel.h"
#include "splatterhouse/memory.h"
#include <spdlog/spdlog.h>

namespace splatterhouse::hle {

// ── xam.xex stubs (user-mode kernel: Xam* / XUser* / XContent* etc) ──
// Estos se van implementando a medida que el juego los necesite.
// Usar xenia como referencia: https://github.com/xenia-project/xenia

static void Xam_Stub_XamGetSystemVersion() { UnimplementedStub("xam", "XamGetSystemVersion"); }
static void Xam_Stub_XamLoaderGetDvdTrayState() { UnimplementedStub("xam", "XamLoaderGetDvdTrayState"); }
static void Xam_Stub_XUserGetSigninState() { UnimplementedStub("xam", "XUserGetSigninState"); }
static void Xam_Stub_XUserGetSigninInfo() { UnimplementedStub("xam", "XUserGetSigninInfo"); }
static void Xam_Stub_XContentCreateEnumerator() { UnimplementedStub("xam", "XContentCreateEnumerator"); }
static void Xam_Stub_NetDll_XNetStartup() { UnimplementedStub("xam", "NetDll_XNetStartup"); }
static void Xam_Stub_XamInputGetState() { UnimplementedStub("xam", "XamInputGetState"); }

void RegisterXamExports() {
    auto reg = [](const char* name, uint32_t ord, HleHandler h){ Register("xam.xex", name, ord, h); };
    // Ordinales reales varían por dash; usamos 0 hasta tener el dump del XEX de Splatterhouse
    reg("XamGetSystemVersion", 0, Xam_Stub_XamGetSystemVersion);
    reg("XamLoaderGetDvdTrayState", 0, Xam_Stub_XamLoaderGetDvdTrayState);
    reg("XUserGetSigninState", 0, Xam_Stub_XUserGetSigninState);
    reg("XUserGetSigninInfo", 0, Xam_Stub_XUserGetSigninInfo);
    reg("XContentCreateEnumerator", 0, Xam_Stub_XContentCreateEnumerator);
    reg("NetDll_XNetStartup", 0, Xam_Stub_NetDll_XNetStartup);
    reg("XamInputGetState", 0, Xam_Stub_XamInputGetState);
    spdlog::debug("[HLE] xam.xex exports registrados");
}

} // namespace splatterhouse::hle

// Simbolos que XenonRecomp espera enlazar: __imp__xam_<func>
extern "C" {
    void __imp__xam_XamGetSystemVersion() { splatterhouse::hle::UnimplementedStub("xam","XamGetSystemVersion"); }
    void __imp__xam_XamLoaderGetDvdTrayState() { splatterhouse::hle::UnimplementedStub("xam","XamLoaderGetDvdTrayState"); }
    void __imp__xam_XUserGetSigninState() { splatterhouse::hle::UnimplementedStub("xam","XUserGetSigninState"); }
    void __imp__xam_XUserGetSigninInfo() { splatterhouse::hle::UnimplementedStub("xam","XUserGetSigninInfo"); }
}
