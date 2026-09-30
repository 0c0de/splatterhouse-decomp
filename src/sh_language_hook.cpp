// Splatterhouse (2010) - localizacion del TEXTO (audio siempre English).
//
// El juego (Gamebryo) obtiene el nombre del idioma con sub_827B65E0(this) =
// tabla[this+120], y con el construye "data/localization/%s.xml". Esa funcion
// se llama desde DOS sitios:
//   - sub_82E786E8 (arranque temprano; el audio/setup): si se le devuelve un
//     idioma no-ingles, el juego crashea (strlen sobre puntero malo).
//   - sub_8277CFC8 (LocalizationManager::LoadLanguage): es el que queremos.
// Asi que solo cambiamos el idioma cuando la llamada viene de la localizacion,
// dejando el arranque/audio en el idioma original (English). El nombre se
// devuelve en minusculas porque los ficheros son "spanish.xml" etc.

#include <cstring>
#include <string>

#if defined(_WIN32)
#include <windows.h>
#endif

#include <rex/cvar.h>
#include <rex/hook.h>
#include <rex/ppc.h>
#include <rex/rex_app.h>

// Funciones recompiladas (para comparar el return address del host).
REX_HOOK_RAW(sub_8277CFC8);
REX_HOOK_RAW(sub_82E786E8);

namespace {

constexpr uint32_t kLanguageTable = 0x82F90110;

struct LanguageEntry {
  const char* name;
  int index;
};

constexpr LanguageEntry kLanguages[] = {
    {"english", 0}, {"french", 1}, {"italian", 2},
    {"german", 3},  {"spanish", 4}, {"japanese", 5},
};

int HostLanguageIndex() {
#if defined(_WIN32)
  switch (PRIMARYLANGID(GetUserDefaultUILanguage())) {
    case LANG_FRENCH:
      return 1;
    case LANG_ITALIAN:
      return 2;
    case LANG_GERMAN:
      return 3;
    case LANG_SPANISH:
      return 4;
    case LANG_JAPANESE:
      return 5;
    default:
      return 0;
  }
#else
  return 0;
#endif
}

int ResolveLanguageIndex() {
  const std::string setting = rex::cvar::Query<std::string>("sh_language");
  for (const LanguageEntry& language : kLanguages) {
    if (setting == language.name) {
      return language.index;
    }
  }
  return HostLanguageIndex();
}

// True si el return address del host cae dentro de sub_8277CFC8 (localizacion)
// y no de sub_82E786E8 (arranque/audio).
bool CalledFromLocalization(void* return_address) {
  const uintptr_t ra = reinterpret_cast<uintptr_t>(return_address);
  const uintptr_t localization = reinterpret_cast<uintptr_t>(&sub_8277CFC8);
  const uintptr_t startup = reinterpret_cast<uintptr_t>(&sub_82E786E8);
  const uintptr_t d_localization =
      ra >= localization ? ra - localization : localization - ra;
  const uintptr_t d_startup = ra >= startup ? ra - startup : startup - ra;
  return d_localization < d_startup;
}

// Nombre del idioma en minusculas, en un buffer guest asignado una sola vez.
uint32_t GuestLowercaseName(int index) {
  static uint32_t address = 0;
  static int cached_index = -1;
  auto* memory = rex::Runtime::instance()->memory();
  if (!memory) {
    return 0;
  }
  if (!address) {
    address = memory->SystemHeapAlloc(32);
    if (!address) {
      return 0;
    }
    cached_index = -1;
  }
  if (cached_index != index) {
    const char* name = (index >= 0 && index < 6) ? kLanguages[index].name : "english";
    char* destination = memory->TranslateVirtual<char*>(address);
    std::strncpy(destination, name, 31);
    destination[31] = '\0';
    cached_index = index;
  }
  return address;
}

}  // namespace

REX_HOOK_RAW(__imp__sub_827B65E0);

REX_HOOK_RAW(sub_827B65E0) {
  void* return_address = __builtin_return_address(0);
  __imp__sub_827B65E0(ctx, base);
  if (rex::cvar::Query<std::string>("sh_language") == "default") {
    return;
  }
  if (!CalledFromLocalization(return_address)) {
    return;
  }
  const int index = ResolveLanguageIndex();
  if (const uint32_t name = GuestLowercaseName(index)) {
    ctx.r3.u64 = name;
  }
}
