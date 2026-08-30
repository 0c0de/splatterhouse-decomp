# GPU — Xenos -> Host

Xbox 360 GPU: Xenos (ATI), D3D9-like con command buffer ring + shaders microcode.

## Estrategia para este port

Splatterhouse (UE3) usa `d3d9.dll` del XDK. Eso es buena noticia: no hay que emular todo el Xenos, basta con HLEar D3D9:

- Interceptar `D3D9Create`, `CreateDevice`, `DrawIndexedPrimitive`, `SetTexture`, `SetVertexShader`, etc.
- Re-emitir a D3D12/Vulkan con wrapper (similar a `D3D9On12` o `dxvk`).

Alternativa si se quiere fidelidad: usar el command processor de Xenia (`xenia/gpu/`).

## Roadmap GPU

1. **Bootstrap**: ventana SDL3 vacía (hecho).
2. **D3D9 HLE stubs**: loggear calls y retornar dummy device.
3. **D3D12 backend**: crear device/swapchain (`src/gpu/d3d12_backend.cpp`).
4. **Shader translator**: microcode Xenos -> DXIL/SPIR-V (ver `XenonUtils`/`xenia` shader translator).
5. **Texture/format swizzle**: Xenos tiling -> linear.

## Referencias

- Xenia `xenia/gpu/xenos/` y `xenia/ui/d3d12/`
- hedge-dev `XenonUtils` GPU helpers (si existen)
- RenderDoc para capturar frames del juego en Xenia y comparar.
