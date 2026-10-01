// Splatterhouse (2010) - fix defensivo del crash "Call to invalid or
// unregistered function at 0x00000000" (transicion al capitulo 6).
//
// Cadena resuelta con llvm-symbolizer --relative-address:
//   sub_82917918 -> sub_82366CC0
// En sub_82366CC0:
//   lwz r11,48(r4) ; mtctr r11 ; bctrl      (llamada virtual por [obj+48])
//
// El objeto llega CORRECTAMENTE construido (vtable valida 0x82058AE8) pero con
// [obj+48] == 0. Ese slot lo inicializa a 0 el constructor sub_82347100
// (recomp.113.cpp) y el valor "esperado" seria 0x822BE680, un metodo virtual
// base que es no-op (blr). En este camino el callback nunca se asigna, asi que
// la llamada indirecta acaba saltando a 0x00000000.
//
// En el original ese flujo no se alcanza con el objeto a medio inicializar (hay
// un LevelState/LevelManager que todavia no existe). Este override replica el
// comportamiento defensivo del resto del motor: si el slot es 0, no se llama.

#include <rex/cvar.h>
#include <rex/hook.h>
#include <rex/logging.h>
#include <rex/memory.h>
#include <rex/ppc.h>
#include <rex/rex_app.h>
#include <rex/system/xmemory.h>

REX_HOOK_RAW(__imp__sub_82366CC0);

namespace {

uint32_t GuestLoadU32Offset(rex::memory::Memory* memory, uint32_t addr, uint32_t off) {
  if (!memory || !addr) {
    return 0;
  }
  const uint8_t* p = memory->TranslateVirtual(addr + off);
  if (!p) {
    return 0;
  }
  return __builtin_bswap32(*reinterpret_cast<const uint32_t*>(p));
}

}  // namespace

REX_HOOK_RAW(sub_82366CC0) {
  auto* memory = rex::Runtime::instance()->memory();
  const uint32_t obj = ctx.r4.u32;
  if (!GuestLoadU32Offset(memory, obj, 48)) {
    // Slot virtual sin inicializar: saltar la llamada en vez de bctrl a 0.
    static bool logged = false;
    if (!logged) {
      logged = true;
      REXLOG_WARN(
          "[sh-uf] sub_82366CC0: [obj+48]==0 (obj=0x{:08X}); se omite la llamada virtual", obj);
    }
    ctx.r3.u64 = 0;
    return;
  }
  __imp__sub_82366CC0(ctx, base);
}
