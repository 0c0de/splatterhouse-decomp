// Splatterhouse (2010) - desbloqueo de niveles (historia).
//
// El frontend (Scaleform) decide que capitulos estan desbloqueados consultando
// la funcion ActionScript GetHighestLevelCompleted, que se resuelve en
// sub_82E5C540(this, name) devolviendo [statsObj+392] como string. Ahi acaba el
// selector de capitulos del menu principal.
//
// Reemplazamos sub_82E5C540 (weak override) para que, si la cvar
// sh_unlock_levels esta activa, devuelva el nivel maximo en lugar del guardado.
// Tambien reemplazamos sub_82EF0FE0 (GetHighestReachedLevel), que algunos menus
// usan como fuente alternativa de "nivel alcanzado".
//
// sh_unlock_debug=true loguea (one-shot) las consultas para diagnostico.

#include <algorithm>
#include <cstdio>
#include <string>

#include <rex/cvar.h>
#include <rex/hook.h>
#include <rex/logging.h>
#include <rex/memory.h>
#include <rex/ppc.h>
#include <rex/rex_app.h>
#include <rex/system/xmemory.h>

REXCVAR_DECLARE(bool, sh_unlock_levels);
REXCVAR_DECLARE(int32_t, sh_unlock_levels_max);
REXCVAR_DECLARE(bool, sh_unlock_debug);

REX_HOOK_RAW(__imp__sub_82EF0FE0);
REX_HOOK_RAW(__imp__sub_82E5C540);

namespace {

uint32_t UnlockedLevelTarget() {
  const int32_t value = REXCVAR_GET(sh_unlock_levels_max);
  return value > 0 ? static_cast<uint32_t>(value) : 0;
}

void LogOnce(const std::string& message) {
  if (!REXCVAR_GET(sh_unlock_debug)) {
    return;
  }
  static std::string seen;
  if (seen.find(message) != std::string::npos) {
    return;
  }
  seen += message;
  seen += '\n';
  REXLOG_WARN("[sh-unlock] {}", message);
}

std::string GuestCString(uint32_t addr) {
  auto* memory = rex::Runtime::instance()->memory();
  if (!memory || !addr) {
    return {};
  }
  const char* p = memory->TranslateVirtual<const char*>(addr);
  return p ? std::string(p) : std::string();
}

// Buffer guest estable para devolver el nivel como string.
uint32_t GuestLevelString(uint32_t value) {
  static uint32_t address = 0;
  static uint32_t cached = 0xFFFFFFFFu;
  auto* memory = rex::Runtime::instance()->memory();
  if (!memory) {
    return 0;
  }
  if (!address) {
    address = memory->SystemHeapAlloc(16);
    if (!address) {
      return 0;
    }
  }
  if (cached != value) {
    char* dst = memory->TranslateVirtual<char*>(address);
    std::snprintf(dst, 15, "%u", value);
    cached = value;
  }
  return address;
}

}  // namespace

// GetHighestReachedLevel: maximo nivel alcanzado (fuente alternativa).
REX_HOOK_RAW(sub_82EF0FE0) {
  __imp__sub_82EF0FE0(ctx, base);
  if (!REXCVAR_GET(sh_unlock_levels)) {
    return;
  }
  const uint32_t target = UnlockedLevelTarget();
  if (target > ctx.r3.u32) {
    LogOnce("GetHighestReachedLevel " + std::to_string(ctx.r3.u32) + " -> " +
            std::to_string(target));
    ctx.r3.u64 = target;
  }
}

// GetStat(name): el selector de capitulos pregunta "GetHighestLevelCompleted".
REX_HOOK_RAW(sub_82E5C540) {
  const std::string name = GuestCString(ctx.r4.u32);
  __imp__sub_82E5C540(ctx, base);
  if (name != "GetHighestLevelCompleted") {
    return;
  }
  LogOnce("GetHighestLevelCompleted original=" + GuestCString(ctx.r3.u32));
  if (!REXCVAR_GET(sh_unlock_levels)) {
    return;
  }
  if (const uint32_t s = GuestLevelString(UnlockedLevelTarget())) {
    LogOnce("GetHighestLevelCompleted -> " +
            std::to_string(UnlockedLevelTarget()));
    ctx.r3.u64 = s;
  }
}
