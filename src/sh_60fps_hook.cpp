// Splatterhouse (2010) - parche de 60 FPS (equivalente a la patch de Xenia
// "60 FPS" del titulo 4E4D07F0, autor illusion).
//
// La patch de Xenia escribe tres sitios: dos floats (0x82F8FF58/5C: 1/15 y 1/30
// -> 1/60, aplicados en SplatterhouseApp::ApplyGuestMemoryPatches) y una
// instruccion en 0x8247A4AC (stb r30,202(r31) -> stb r11,202(r31)).
//
// Las funciones recompiladas se emiten como simbolos weak (DEFINE_REX_FUNC), de
// modo que este fichero sustituye sub_8247A368 por una version fuerte que llama
// al original (__imp__sub_8247A368) y luego replica el comportamiento parcheado
// del campo +202. Asi el parche sobrevive a una regeneracion de `generated/`.

#include <rex/hook.h>
#include <rex/ppc.h>
#include <rex/rex_app.h>

REX_HOOK_RAW(__imp__sub_8247A368);

REX_HOOK_RAW(sub_8247A368) {
  const uint32_t object = ctx.r3.u32;
  const uint8_t arg_byte = uint8_t(ctx.r7.u32);
  __imp__sub_8247A368(ctx, base);
  if (auto* memory = rex::Runtime::instance()->memory()) {
    memory->TranslateVirtual(object + 202)[0] = arg_byte;
  }
}
