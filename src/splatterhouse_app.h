// splatterhouse - ReXGlue Recompiled Project
//
// Customize your app by overriding virtual hooks from rex::ReXApp.

#pragma once

#if defined(_WIN32)
#include <cstdio>
#include <crtdbg.h>
#include <Windows.h>
#endif
#include <fstream>
#include <vector>

#include <rex/cvar.h>
#include <rex/rex_app.h>
#include <rex/logging.h>
#include <rex/memory.h>
#include <rex/filesystem.h>
#include <rex/system/kernel_state.h>
#include <rex/system/xam/content_manager.h>
#include <rex/system/xmemory.h>
#include <rex/ui/keybinds.h>

#include <fmt/format.h>

#include "graphics_menu.h"

REXCVAR_DECLARE(bool, dump_guest);
REXCVAR_DECLARE(bool, sh_60fps);
REXCVAR_DECLARE(bool, sh_graphics_menu);

class SplatterhouseApp : public rex::ReXApp {
 public:
  using rex::ReXApp::ReXApp;

  static std::unique_ptr<rex::ui::WindowedApp> Create(
      rex::ui::WindowedAppContext& ctx) {
    return std::unique_ptr<SplatterhouseApp>(new SplatterhouseApp(ctx, "splatterhouse",
        PPCImageConfig));
  }

  // Consola de Windows adjunta al lanzar la app: muestra el log en vivo.
  void OnPostInitLogging() override {
#if defined(_WIN32)
    if (AllocConsole()) {
      FILE* f = nullptr;
      if (freopen_s(&f, "CONOUT$", "w", stdout) != 0) f = nullptr;
      f = nullptr;
      freopen_s(&f, "CONOUT$", "w", stderr);
      f = nullptr;
      freopen_s(&f, "CONIN$", "r", stdin);
      SetConsoleTitleA("Splatterhouse - Log");
      // Con stderr sin buffer, el log aparece en tiempo real.
      setvbuf(stdout, nullptr, _IONBF, 0);
      setvbuf(stderr, nullptr, _IONBF, 0);
      // CRT asserts: redirigidos a un fichero (no modal MessageBox).
      // Sin esto un assert aborta en modal y cuelga la carga del XEX.
      _set_abort_behavior(0, _CALL_REPORTFAULT);
      FILE* crt = nullptr;
      freopen_s(&crt, "crt_report.txt", "w", stderr);
      if (crt) {
        _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
        _CrtSetReportFile(_CRT_ASSERT, crt);
        _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
        _CrtSetReportFile(_CRT_ERROR, crt);
        _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
        _CrtSetReportFile(_CRT_WARN, crt);
      }
      // Con stderr sin buffer, el log aparece en tiempo real.
      setvbuf(stdout, nullptr, _IONBF, 0);
      setvbuf(stderr, nullptr, _IONBF, 0);
    }
#endif
    REXLOG_INFO("Consola de log adjuntada");
  }

  // Defaults seguros si el usuario no los ha puesto en splatterhouse.toml.
  // Sin gpu_plugin no hay GPU (pantalla negra); sin "rov" la iluminacion sale
  // mal (la ruta RTV aproxima la eDRAM del X360).
  void OnPreSetup(rex::RuntimeConfig& config) override {
    if (config.gpu_plugin.empty()) {
      config.gpu_plugin = "xenos";
    }
    if (!rex::cvar::HasNonDefaultValue("render_target_path_d3d12")) {
      rex::cvar::SetFlagByName("render_target_path_d3d12", "rov");
    }
    if (!rex::cvar::HasNonDefaultValue("present_effect")) {
      rex::cvar::SetFlagByName("present_effect", "fsr");
    }
    if (!rex::cvar::HasNonDefaultValue("video_mode_refresh_rate")) {
      rex::cvar::SetFlagByName("video_mode_refresh_rate", "60");
    }
  }

  // `vsync` y `d3d12_present_frame_limiter*` los registra el plugin GPU, que se
  // carga DESPUES de OnPreSetup, asi que alli no existen todavia. OnPostSetup
  // corre cuando el plugin ya esta cargado (y el worker de vblank lee `vsync`
  // en vivo), asi que aqui si aplican.
  //
  // VSync off + present frame limiter a 60: un frame que excede el presupuesto
  // ya no hace caer la presentacion a mitad de vblank (60 -> 30), que se percibe
  // como "parones"; el limiter acota el ritmo a 60 sin bloquear en vblank.
  void OnPostSetup() override {
    if (!rex::cvar::HasNonDefaultValue("vsync")) {
      rex::cvar::SetFlagByName("vsync", "false");
    }
    if (!rex::cvar::HasNonDefaultValue("d3d12_present_frame_limiter")) {
      rex::cvar::SetFlagByName("d3d12_present_frame_limiter", "true");
    }
    if (!rex::cvar::HasNonDefaultValue("d3d12_present_frame_limiter_fps")) {
      rex::cvar::SetFlagByName("d3d12_present_frame_limiter_fps", "60");
    }
    // Sin tearing (ALLOW_TEARING off): con vsync=false + limiter, el flag de
    // tearing permitia desgarro en frames que no caian dentro del vblank. Con
    // esto desactivado se elimina sin volver al bajon a 30 del vsync clasico.
    if (!rex::cvar::HasNonDefaultValue("d3d12_allow_variable_refresh_rate_and_tearing")) {
      rex::cvar::SetFlagByName("d3d12_allow_variable_refresh_rate_and_tearing", "false");
    }
    // Red de seguridad: si el toml trae un resolution_scale que dispara el
    // ancho de render por encima del limite seguro con ROV (p.ej. 2x a 1080p =
    // 3840 px), la GPU se satura y Windows lo marca como "no responde". Lo
    // reducimos aqui para no colgar al arrancar.
    {
      const int32_t width = std::max(1, rex::cvar::Query<int32_t>("video_mode_width"));
      const int32_t scale = rex::cvar::Query<int32_t>("resolution_scale");
      const int32_t max_scale = std::max(1, std::min(4, 2560 / width));
      if (scale > max_scale) {
        REXLOG_WARN("[sh-perf] resolution_scale={} con ancho {} excede el limite ({}x{}); se ajusta a {}",
                    scale, width, width, scale, max_scale);
        rex::cvar::SetFlagByName("resolution_scale", std::to_string(max_scale));
      }
    }
    REXLOG_INFO("[sh-perf] vsync={} limiter={} fps={} tearing={} scale={}",
                rex::cvar::Query<bool>("vsync"),
                rex::cvar::Query<bool>("d3d12_present_frame_limiter"),
                rex::cvar::Query<double>("d3d12_present_frame_limiter_fps"),
                rex::cvar::Query<bool>("d3d12_allow_variable_refresh_rate_and_tearing"),
                rex::cvar::Query<int32_t>("resolution_scale"));
  }

  // Ruta de datos del juego: assets/ junto al exe (o override con
  // --game_data_root / REXCVAR_game_data_root).
  void OnConfigurePaths(rex::PathConfig& paths) override {
    if (paths.game_data_root.empty()) {
      std::filesystem::path candidate =
          rex::filesystem::GetExecutableFolder() / "assets";
      if (std::filesystem::is_directory(candidate)) {
        paths.game_data_root = candidate;
      }
    }
  }

  // Dump de la imagen guest (XEX cargado) para analisis estatico:
  // habilitar con REXCVAR_dump_guest=1 (tools/recompile.py / scripts)
  void OnLoadXexImage(std::string& xex_image) override {
    // El instalador genera el XEX desencriptado con xextool (default.dec.xex);
    // si esta presente, usarlo en vez del retail.
    std::filesystem::path decrypted =
        rex::filesystem::GetExecutableFolder() / "assets" / "default.dec.xex";
    if (std::filesystem::is_regular_file(decrypted)) {
      xex_image = "game:\\default.dec.xex";
    }
    REXLOG_INFO("[sh-trace] pre LoadXexImage: {}", xex_image);
  }

  // Parches de datos en memoria guest (equivalentes a los patches de Xenia).
  void ApplyGuestMemoryPatches() {
    auto* memory = rex::Runtime::instance()->memory();
    auto store_be32 = [&](u32 addr, u32 value) {
      u32 heap_size = 0;
      auto* heap = memory->LookupHeap(addr);
      if (!heap || !heap->QuerySize(addr, &heap_size) || heap_size < 4) {
        REXLOG_WARN("[sh-60] sin memoria committed en 0x{:08X}", addr);
        return;
      }
      u8* p = memory->TranslateVirtual(addr);
      p[0] = u8(value >> 24);
      p[1] = u8(value >> 16);
      p[2] = u8(value >> 8);
      p[3] = u8(value);
    };

    if (REXCVAR_GET(sh_60fps)) {
      // Xenia "60 FPS" (4E4D07F0, illusion): el clamp del delta-time pasa de
      // 1/30..1/15 a 1/60..1/60 (0x3C888889).
      store_be32(0x82F8FF58, 0x3C888889);
      store_be32(0x82F8FF5C, 0x3C888889);
      REXLOG_INFO("[sh-60] patch de datos 60 FPS aplicado (0x82F8FF58/5C)");
    }
  }

  void ToggleGraphicsMenu() {
    if (g_graphics_menu) {
      g_graphics_menu->RequestClose();
    } else if (graphics_drawer_) {
      new GraphicsMenuDialog(graphics_drawer_, graphics_config_path_);
    }
  }

  // Recibe F5 antes que ImGui (z-order alto). LaunchImpl no deja sobrescribir
  // ReXApp::OnKeyDown (es privado), asi que usamos un listener propio.
  struct GraphicsKeyListener : public rex::ui::WindowInputListener {
    std::function<void()> toggle;
    void OnKeyDown(rex::ui::KeyEvent& e) override {
      if (e.virtual_key() == rex::ui::VirtualKey::kF5) {
        if (toggle) {
          toggle();
        }
        e.set_handled(true);
      }
    }
  };
  GraphicsKeyListener graphics_key_listener_;

  // Pantalla de opciones propia del port (tecla F5 por defecto, tambien
  // reasignable via el keybind "bind_graphics").
  void OnCreateDialogs(rex::ui::ImGuiDrawer* drawer) override {
    graphics_drawer_ = drawer;
    graphics_config_path_ = rex::filesystem::GetExecutableFolder() / "splatterhouse.toml";
    rex::ui::RegisterBind("bind_graphics", "F5", "Toggle options menu",
                          [this]() { ToggleGraphicsMenu(); });
    graphics_key_listener_.toggle = [this]() { ToggleGraphicsMenu(); };
    if (window()) {
      window()->AddInputListener(&graphics_key_listener_, 128);
    }
    if (REXCVAR_GET(sh_graphics_menu)) {
      new GraphicsMenuDialog(drawer, graphics_config_path_);
    }
  }

  void OnShutdown() override {
    rex::ui::UnregisterBind("bind_graphics");
    if (window()) {
      window()->RemoveInputListener(&graphics_key_listener_);
    }
  }

  rex::ui::ImGuiDrawer* graphics_drawer_ = nullptr;
  std::filesystem::path graphics_config_path_;

  // Instala paquetes DLC (STFS: CON/LIVE/PIRS) que el usuario haya dejado en la
  // carpeta de datos del usuario, para que el engine los vea via XamContent.
  // Se buscan en varias ubicaciones (la del xuid del perfil, el xuid comun
  // 0000000000000000, y la raiz).
  void InstallDlcPackages() {
    auto* kernel_state = rex::runtime::current_kernel_state();
    if (!kernel_state || !kernel_state->content_manager()) {
      return;
    }
    const uint32_t title_id = kernel_state->title_id();
    const std::string title_str = fmt::format("{:08X}", title_id);
    auto user_root = rex::filesystem::GetUserFolder() / "splatterhouse";
    std::vector<std::filesystem::path> candidates = {
        user_root / title_str / "00000002",
        user_root / "0000000000000000" / title_str / "00000002",
    };
    // Cualquier subcarpeta de primer nivel (xuid de perfil) con <title>/00000002.
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(user_root, ec)) {
      if (ec || !entry.is_directory(ec)) {
        continue;
      }
      candidates.push_back(entry.path() / title_str / "00000002");
    }
    uint32_t total = 0;
    for (const auto& dir : candidates) {
      total += kernel_state->content_manager()->InstallContentFromDirectory(dir);
    }
    if (total) {
      REXLOG_INFO("[sh-dlc] Instalados {} paquete(s) DLC desde la carpeta de usuario", total);
    }
  }

  void OnPostLoadXexImage() override {
    ApplyGuestMemoryPatches();
    InstallDlcPackages();
    if (!REXCVAR_GET(dump_guest)) {
      return;
    }
    auto* memory = rex::Runtime::instance()->memory();
    std::filesystem::path out_path =
        rex::filesystem::GetExecutableFolder() / "guest_image.bin";
    std::ofstream f(out_path, std::ios::binary | std::ios::trunc);
    if (!f) {
      REXLOG_ERROR("No se pudo crear guest_image.bin en {}",
                   out_path.string());
      return;
    }
    u32 start = PPCImageConfig.image_base;
    // Solo DATA (antes del modulo): data es donde viven las vtables que
    // referencia el juego; el analisis busca punteros ahi.
    u32 end = PPCImageConfig.code_base;
    for (u32 off = start; off < end; off += 0x1000) {
      // Pad con ceros las regiones no commiteadas para mantener el layout
      // (direccion -> offset en el fichero).
      auto* heap = memory->LookupHeap(off);
      u32 size_tmp2 = 0;
      if (!heap || !heap->QuerySize(off, &size_tmp2) || size_tmp2 == 0) {
        static const char zeros[0x1000] = {0};
        f.write(zeros, 0x1000);
        continue;
      }
      f.write(reinterpret_cast<const char*>(
                  memory->TranslateVirtual<uint8_t>(off)),
              0x1000);
    }
    f.flush();
    f.close();
    REXLOG_INFO("Imagen guest volcada a {} ({} MB)", out_path.string(),
                (end - start) / (1024 * 1024));
  }

  // Override virtual hooks for customization:
  // void OnPostInitLogging() override {}
  // void OnPreSetup(rex::RuntimeConfig& config) override {}
  // void OnLoadXexImage(std::string& xex_image) override {}
  // void OnPostLoadXexImage() override {}
  // void OnPostSetup() override {}
  // void OnCreateDialogs(rex::ui::ImGuiDrawer* drawer) override {}
  // std::unique_ptr<rex::ui::ImGuiDialog> CreateAchievementsOverlay() override;
  // std::unique_ptr<rex::ui::AchievementNotificationDialog>
  // CreateAchievementNotificationDialog() override;
  // void OnShutdown() override {}
};
