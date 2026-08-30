#include "splatterhouse/common.h"
#include <spdlog/spdlog.h>
#include <SDL3/SDL.h>

namespace splatterhouse::gpu {

// Abstracción GPU. El Xenos (Xbox 360) usa un command buffer ring + shaders microcode.
// Dos estrategias posibles:
//  1) Traducir draws del guest a D3D12/Vulkan en HLE (como Xenia) - complejo.
//  2) Si el juego usa D3D9-like (XDK D3D), interceptar D3D calls HLE y re-emitir en host.
//
// Para Splatterhouse (Unreal Engine 3), usa D3D9 XDK -> interceptamos d3d9.dll HLE.

static SDL_Window* s_window = nullptr;
static bool s_inited = false;

bool Init(void* windowHandle, int w, int h) {
    s_window = (SDL_Window*)windowHandle;
    s_inited = true;
    spdlog::info("[gpu] Backend init {}x{} window={:p}", w, h, windowHandle);
    SH_TODO("Implementar D3D12/Vulkan backend real + Xenos command processor");
    return true;
}
void Shutdown() {
    if (!s_inited) return;
    spdlog::info("[gpu] Shutdown");
    s_inited = false;
}
void Present() {
    // Por ahora solo clear para validar loop
    if (!s_inited || !s_window) return;
    // TODO: swapchain present
}
void Resize(int w, int h) {
    spdlog::info("[gpu] Resize {}x{}", w, h);
}

} // namespace splatterhouse::gpu
