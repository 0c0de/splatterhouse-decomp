#include "splatterhouse/common.h"
#include <spdlog/spdlog.h>

// Stub D3D12 backend - se implementará cuando se tenga el trace del Xenos.
// Ver: docs/gpu.md y extern/XenonUtils GPU helpers.

namespace splatterhouse::gpu::d3d12 {

bool CreateDevice(void* hwnd) {
    (void)hwnd;
    spdlog::info("[gpu::d3d12] CreateDevice stub");
    SH_TODO("Crear ID3D12Device, swapchain, command queue");
    return false;
}

} // namespace splatterhouse::gpu::d3d12
