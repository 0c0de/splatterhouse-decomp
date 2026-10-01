// splatterhouse - ReXGlue Recompiled Project
//
// Splatterhouse (2010) Xbox 360 Static Recompilation Port
// Runtime: ReXGlue SDK (https://github.com/rexglue/rexglue-sdk)

// Codigo generado por `rexglue codegen` (requiere el XEX de Splatterhouse).
// Sin el XEX el proyecto compila en modo bootstrap (sin codigo guest).
#if defined(__has_include) && __has_include("generated/splatterhouse_init.h")
#include "generated/splatterhouse_init.h"
#define SH_HAS_GENERATED 1
#else
// Bootstrap sin codigo generado: imagen guest vacia.
#include <rex/image_info.h>
static const rex::PPCImageInfo PPCImageConfig{};
#endif

#include "splatterhouse_app.h"
#include <rex/cvar.h>

// Definicion del cvar: dump guest_image.bin tras cargar el XEX
// (usar con REXCVAR_dump_guest=1 o --dump_guest en la linea de comandos)
REXCVAR_DEFINE_BOOL(dump_guest, false, "Debug",
    "Dump guest image after XEX load (analysis)");

// 60 FPS data patch (Xenia "60 FPS", title 4E4D07F0, author illusion):
// rewrites the delta-time clamp floats to 1/60 -> 1/60 in guest memory.
REXCVAR_DEFINE_BOOL(sh_60fps, true, "Graphics",
    "Apply the Xenia 60 FPS data patch (Splatterhouse 4E4D07F0)");

// Test: abre la pantalla de graficos al arrancar (para capturas).
REXCVAR_DEFINE_BOOL(sh_graphics_menu, false, "Graphics",
    "Open the graphics menu on startup (testing)");

// Dev: carga directa de nivel (para calentar la cache de shaders por
// capitulos). La consume el runtime (VdSwap) y se limpia sola.
REXCVAR_DEFINE_STRING(sh_warp, "", "System",
    "Dev: request loading a level by name (e.g. lvl2_shanty), then cleared");

// Idioma del juego (Gamebryo LanguageDef). auto = locale del host.
REXCVAR_DEFINE_STRING(sh_language, "auto", "System",
    "Game language: auto, english, french, italian, german, spanish, japanese")
    .lifecycle(rex::cvar::Lifecycle::kRequiresRestart);

// Desbloqueo de niveles de la historia: GetHighestReachedLevel devuelve el
// maximo, asi el selector de capitulos del frontend los muestra todos.
REXCVAR_DEFINE_BOOL(sh_unlock_levels, true, "System",
    "Unlock all story levels (chapter select)");

// Nivel maximo que se reporta al desbloquear (los niveles son lvl1..lvl7).
REXCVAR_DEFINE_INT32(sh_unlock_levels_max, 7, "System",
    "Highest level index reported when sh_unlock_levels is on");

// Diagnostico del desbloqueo de niveles (loguea que getters se consultan).
REXCVAR_DEFINE_BOOL(sh_unlock_debug, false, "System",
    "Log level-unlock getter queries (diagnostics)");

REX_DEFINE_APP(splatterhouse, SplatterhouseApp::Create)
