// Splatterhouse (2010) port - in-game graphics options screen.
//
// Full screen in the game's style (dark background, red/cream), NOT a floating
// window. Opened/closed with the "bind_graphics" keybind (F5 by default,
// rebindable from the runtime settings overlay). Writes the port's cvars and
// persists them to splatterhouse.toml with "Save".

#pragma once

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <functional>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <imgui.h>
#include <rex/cvar.h>
#include <rex/ui/imgui_dialog.h>
#include <rex/ui/virtual_key.h>

class GraphicsMenuDialog;

// The dialog registers itself with the ImGuiDrawer when constructed and deletes
// itself when Close() is called. This global pointer lets the keybind open/close it.
inline GraphicsMenuDialog* g_graphics_menu = nullptr;

class GraphicsMenuDialog : public rex::ui::ImGuiDialog {
 public:
  GraphicsMenuDialog(rex::ui::ImGuiDrawer* drawer, std::filesystem::path config_path,
                     std::function<void()> quit = nullptr)
      : rex::ui::ImGuiDialog(drawer),
        config_path_(std::move(config_path)),
        quit_(std::move(quit)) {
    g_graphics_menu = this;
    BuildRows();
    // The displayed preset is derived from the current configuration (it used to
    // always show "Ultra" even when the toml said otherwise).
    preset_index_ = DetectPresetIndex();
    // The first row is a section title: start on something selectable.
    selected_ = NextSelectable(0, +1);
  }
  ~GraphicsMenuDialog() override {
    if (g_graphics_menu == this) {
      g_graphics_menu = nullptr;
    }
  }

  void RequestClose() { Close(); }

  // The keyboard is not read from ImGui: keys are received by our own
  // WindowInputListener (z-order 128, the same one that already catches F5 before
  // the ImGuiDrawer), which queues them here and the menu consumes them when it
  // draws. That way the menu doesn't depend on ImGui having focus or on
  // io.WantCaptureKeyboard.
  void PushKey(rex::ui::VirtualKey key) {
    // System autorepeat resends the keypress; only the first one matters.
    if (keys_down_.insert(key).second) {
      pending_keys_.push_back(key);
    }
  }
  void ReleaseKey(rex::ui::VirtualKey key) { keys_down_.erase(key); }

 protected:
  void OnDraw(ImGuiIO& io) override {
    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.05f, 0.02f, 0.02f, 0.95f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    const ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoSavedSettings;
    ImGui::Begin("##sh_graphics_menu", nullptr, flags);

    const float scale = std::max(1.0f, io.DisplaySize.x / 1280.0f);
    const float panel_w = std::min(io.DisplaySize.x * 0.74f, 940.0f * scale);
    const float panel_x = (io.DisplaySize.x - panel_w) * 0.5f;

    HandleInput(io, io.MousePos.x >= panel_x && io.MousePos.x <= panel_x + panel_w &&
                       io.MousePos.y >= 60.0f * scale);

    // Larger text: the font of the whole menu is scaled.
    const float font_scale = 1.7f;

    ImGui::SetCursorPos(ImVec2(panel_x, 70.0f * scale));
    ImGui::SetWindowFontScale(2.6f);
    ImGui::TextColored(kAccent, "%s", L("OPTIONS"));
    ImGui::SetWindowFontScale(font_scale);
    ImGui::SetCursorPosX(panel_x);
    ImGui::TextColored(kDim, "%s", L("Splatterhouse - port options"));

    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 24.0f * scale);

    // With ~30 rows they don't all fit: only the window that fits is drawn and
    // the selection is kept in view.
    const float row_h = ImGui::GetTextLineHeight() + 12.0f * scale;
    const float list_top = ImGui::GetCursorPosY();
    const float list_bottom = io.DisplaySize.y - 116.0f * scale;
    const int count = static_cast<int>(rows_.size());
    visible_rows_ = std::max(1, static_cast<int>((list_bottom - list_top) / row_h));
    if (count > visible_rows_) {
      if (selected_ < first_row_) {
        first_row_ = selected_;
      } else if (selected_ >= first_row_ + visible_rows_) {
        first_row_ = selected_ - visible_rows_ + 1;
      }
      first_row_ = std::clamp(first_row_, 0, count - visible_rows_);
    } else {
      first_row_ = 0;
    }

    hovered_ = -1;
    for (int i = first_row_; i < count && i < first_row_ + visible_rows_; ++i) {
      const bool selected = (i == selected_);
      const Row& row = rows_[i];

      if (row.header) {
        ImGui::SetCursorPosX(panel_x);
        ImGui::TextColored(kAccent, "-- %s", L(row.label.c_str()));
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 12.0f * scale);
        continue;
      }

      ImGui::SetCursorPosX(panel_x);
      ImGui::TextColored(selected ? kAccent : kDim, selected ? ">" : " ");

      ImGui::SameLine(panel_x + 34.0f * scale * font_scale);
      ImGui::TextColored(selected ? (row.danger ? kAccent : kText) : (row.danger ? kDim : kTextDim),
                         "%s", L(row.label.c_str()));

      ImGui::SameLine(panel_x + panel_w * 0.58f);
      const std::string value = row.get();
      ImGui::TextColored(selected ? kAccent : kText, "%s %s %s", selected ? "<" : " ",
                         value.c_str(), selected ? ">" : " ");
      if (row.restart) {
        ImGui::SameLine();
        ImGui::TextColored(kDim, "%s", L(" [restart]"));
      }

      if (ImGui::IsMouseHoveringRect(ImVec2(panel_x, ImGui::GetItemRectMin().y - 3.0f * scale),
                                     ImVec2(panel_x + panel_w,
                                            ImGui::GetItemRectMax().y + 3.0f * scale))) {
        hovered_ = i;
      }
      ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 12.0f * scale);
    }

    // Hover ONLY moves the selection if the mouse has actually moved. If it were
    // reassigned every frame, the row under the cursor would stay pinned and the
    // keyboard and wheel would do nothing (which is how it used to be).
    const bool mouse_moved = io.MousePos.x != last_mouse_.x || io.MousePos.y != last_mouse_.y;
    last_mouse_ = io.MousePos;
    if (mouse_moved && hovered_ >= 0 && hovered_ != selected_) {
      selected_ = hovered_;
    }
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && hovered_ >= 0) {
      selected_ = hovered_;
      ChangeSelected(+1);
      MarkDirty();
    }

    DrawFooter(io, panel_x, panel_w, scale, font_scale);

    ImGui::SetWindowFontScale(1.0f);
    ImGui::End();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();
  }

 private:
  struct Row {
    std::string label;
    std::function<std::string()> get;
    std::function<void(int)> change;
    bool restart = false;
    bool danger = false;
    bool header = false;
  };

  // A row is only drawn if its cvar exists: some options depend on how the
  // runtime was compiled (e.g. FSR quality only exists with FidelityFX) or on the
  // GPU plugin being loaded. GetFlagByName returns "" if it isn't registered, so
  // without this filter the menu would offer values that are then rejected when
  // written.
  static bool Available(const char* name) {
    return rex::cvar::GetFlagInfo(name) != nullptr;
  }
  void AddHeader(const char* label) {
    rows_.push_back({label, {}, {}, false, false, true});
  }
  bool IsSelectable(int index) const {
    return index >= 0 && index < static_cast<int>(rows_.size()) && !rows_[index].header;
  }
  // Navigation that skips section titles.
  int NextSelectable(int from, int delta) const {
    const int count = static_cast<int>(rows_.size());
    for (int step = 1; step <= count; ++step) {
      int index = ((from + delta * step) % count + count) % count;
      if (IsSelectable(index)) {
        return index;
      }
    }
    return from;
  }

  void HandleInput(ImGuiIO& io, bool mouse_in_panel) {
    if (rows_.empty()) {
      return;
    }
    // Keys queued by the window listener (reliable source: doesn't depend on
    // ImGui focus).
    for (const rex::ui::VirtualKey key : pending_keys_) {
      switch (key) {
        case rex::ui::VirtualKey::kEscape:
          if (dirty_) {
            rex::cvar::SaveConfig(config_path_);
          }
          Close();
          return;
        case rex::ui::VirtualKey::kUp:
          selected_ = NextSelectable(selected_, -1);
          break;
        case rex::ui::VirtualKey::kDown:
          selected_ = NextSelectable(selected_, +1);
          break;
        case rex::ui::VirtualKey::kLeft:
          ChangeSelected(-1);
          MarkDirty();
          break;
        case rex::ui::VirtualKey::kRight:
        case rex::ui::VirtualKey::kSpace:
        case rex::ui::VirtualKey::kReturn:
          ChangeSelected(+1);
          MarkDirty();
          break;
        case rex::ui::VirtualKey::kS:
          rex::cvar::SaveConfig(config_path_);
          dirty_ = false;
          saved_flash_ = 2.0f;
          break;
        default:
          break;
      }
    }
    pending_keys_.clear();

    // Wheel: moves the selection. It used to be ignored except over the last
    // visible row because hover reassigned selected_ every frame.
    if (mouse_in_panel && io.MouseWheel != 0.0f) {
      selected_ = NextSelectable(selected_, io.MouseWheel < 0.0f ? +1 : -1);
    }

    if (saved_flash_ > 0.0f) {
      saved_flash_ = std::max(0.0f, saved_flash_ - io.DeltaTime);
    }
  }

  // Changing the selected row isn't an "unsaved change" when the row is the quit
  // row (ChangeQuit already saves whatever is pending).
  void MarkDirty() {
    if (selected_ != quit_row_) {
      dirty_ = true;
    }
  }

  void DrawFooter(ImGuiIO& io, float panel_x, float panel_w, float scale, float font_scale) {
    ImGui::SetWindowFontScale(font_scale);
    ImGui::SetCursorPos(ImVec2(panel_x, io.DisplaySize.y - 92.0f * scale));
    const int count = static_cast<int>(rows_.size());
    std::string hint = L("Arrows: navigate/change   S: save   Esc/F5: close");
    if (dirty_) {
      hint += L("   (unsaved changes)");
    }
    if (count > visible_rows_) {
      char buffer[64];
      std::snprintf(buffer, sizeof(buffer), L("   [%d-%d of %d, wheel to scroll]"),
                    first_row_ + 1, std::min(count, first_row_ + visible_rows_), count);
      hint += buffer;
    }
    ImGui::TextColored(kDim, "%s", hint.c_str());
    if (saved_flash_ > 0.0f) {
      ImGui::SetCursorPosX(panel_x);
      ImGui::TextColored(kAccent, L("Saved to %s"), config_path_.filename().string().c_str());
    }
    const std::vector<std::string> pending = rex::cvar::GetPendingRestartFlags();
    if (!pending.empty()) {
      std::string names;
      for (size_t i = 0; i < pending.size(); ++i) {
        if (i) {
          names += ", ";
        }
        names += pending[i];
      }
      ImGui::SetCursorPosX(panel_x);
      ImGui::TextColored(kDim, L("Restart to apply: %s"), names.c_str());
    }
    if (quit_armed_) {
      ImGui::SetCursorPosX(panel_x);
      ImGui::TextColored(kAccent, "%s", L("? Quit game: Yes = Right/Enter/click, No = Left"));
    }
  }

  // Menu text is written in English and translated at draw time when
  // sh_menu_language is "spanish", so switching takes effect immediately.
  static const char* L(const char* english) {
    if (!Available("sh_menu_language") || QueryString("sh_menu_language") != "spanish") {
      return english;
    }
    static const std::unordered_map<std::string_view, const char*> kSpanish = {
        {"OPTIONS", "OPCIONES"},
        {"Splatterhouse - port options", "Splatterhouse - opciones del port"},
        {" [restart]", " [reinicio]"},
        {"Arrows: navigate/change   S: save   Esc/F5: close",
         "Flechas: navegar/cambiar   S: guardar   Esc/F5: cerrar"},
        {"   (unsaved changes)", "   (cambios sin guardar)"},
        {"   [%d-%d of %d, wheel to scroll]", "   [%d-%d de %d, rueda para scroll]"},
        {"Saved to %s", "Guardado en %s"},
        {"Restart to apply: %s", "Reiniciar para aplicar: %s"},
        {"? Quit game: Yes = Right/Enter/click, No = Left",
         "? Salir del juego: Si = Derecha/Enter/click, No = Izquierda"},
        {"Interface", "Interfaz"},
        {"Menu language", "Idioma del menu"},
        {"Image", "Imagen"},
        {"Internal resolution", "Resolucion interna"},
        {"Internal aspect", "Aspecto interno"},
        {"Draw scale", "Escala de dibujo"},
        {"Render path", "Ruta de render"},
        {"Upscaling and presentation", "Reescalado y presentacion"},
        {"Presentation", "Presentacion"},
        {"FSR quality", "Calidad FSR"},
        {"Sharpness (FSR)", "Nitidez (FSR)"},
        {"Presentation antialiasing", "Antialias presentacion"},
        {"Display", "Pantalla"},
        {"Fullscreen", "Pantalla completa"},
        {"Window size", "Tamano ventana"},
        {"Refresh rate", "Frecuencia"},
        {"FPS limit", "Limite de FPS"},
        {"Quality", "Calidad"},
        {"Texture cache (MB)", "Cache de texturas (MB)"},
        {"Native 2x MSAA", "MSAA 2x nativo"},
        {"Anisotropic filtering", "Filtro anisotropico"},
        {"Performance", "Rendimiento"},
        {"Background shaders", "Shaders en segundo plano"},
        {"Non-blocking PSO", "PSO sin bloqueos"},
        {"PSO creation threads", "Hilos para crear PSO"},
        {"Game", "Juego"},
        {"60 FPS patch", "Parche 60 FPS"},
        {"Unlock levels", "Desbloquear niveles"},
        {"Max level", "Nivel maximo"},
        {"Game language", "Idioma del juego"},
        {"Input and audio", "Entrada y audio"},
        {"Input backend", "Backend de input"},
        {"Max rumble (ms)", "Vibracion maxima (ms)"},
        {"Mute", "Silenciar"},
        {"Development", "Desarrollo"},
        {"Level warp (dev)", "Warp nivel (dev)"},
        {"Quit game", "Salir del juego"},
        {"Low", "Bajo"},
        {"Medium", "Medio"},
        {"High", "Alto"},
        {"Custom", "Personalizado"},
        {"on", "activado"},
        {"off", "desactivado"},
        {"automatic", "automatico"},
        {"always (VRR)", "siempre (VRR)"},
        {"auto (only below 60)", "auto (solo si baja de 60)"},
        {"off (60/30)", "desactivado (60/30)"},
        {"unavailable", "no disponible"},
        {"confirm (Yes/No)", "confirmar (Si/No)"},
        {"press to quit", "pulsar para salir"},
    };
    const auto it = kSpanish.find(english);
    return it != kSpanish.end() ? it->second : english;
  }

  static std::string QueryString(const char* name) {
    return rex::cvar::Query<std::string>(name);
  }
  static void SetString(const char* name, const std::string& value) {
    rex::cvar::SetFlagByName(name, value);
  }
  static void CycleString(const char* name, const std::vector<std::string>& values, int delta) {
    const std::string current = QueryString(name);
    int index = 0;
    for (int i = 0; i < static_cast<int>(values.size()); ++i) {
      if (values[i] == current) {
        index = i;
      }
    }
    const int n = static_cast<int>(values.size());
    index = ((index + delta) % n + n) % n;
    SetString(name, values[index]);
  }
  static std::string FormatValue(const char* name, const char* suffix) {
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%.1f%s", rex::cvar::Query<double>(name), suffix);
    return buffer;
  }
  static void CycleDouble(const char* name, double step, double lo, double hi, int delta) {
    double value = rex::cvar::Query<double>(name) + step * delta;
    value = std::clamp(value, lo, hi);
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%.3f", value);
    SetString(name, buffer);
  }
  // Cycle over integer values written as text (int32 cvars are stored the same
  // way in the toml).
  static void CycleInt(const char* name, const std::vector<int32_t>& values, int delta) {
    const int32_t current = rex::cvar::Query<int32_t>(name);
    int index = 0;
    for (int i = 0; i < static_cast<int>(values.size()); ++i) {
      if (values[i] == current) {
        index = i;
      }
    }
    const int n = static_cast<int>(values.size());
    index = ((index + delta) % n + n) % n;
    SetString(name, std::to_string(values[index]));
  }
  static std::string FormatInt(const char* name) {
    return std::to_string(rex::cvar::Query<int32_t>(name));
  }
  static std::string OnOff(const char* name) {
    return std::string(rex::cvar::Query<bool>(name) ? L("on") : L("off"));
  }
  static std::string FormatPair(const char* name_w, const char* name_h, const char* suffix) {
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%d x %d%s",
                  rex::cvar::Query<int32_t>(name_w), rex::cvar::Query<int32_t>(name_h), suffix);
    return std::string(buffer);
  }
  static void CyclePair(const char* name_w, const char* name_h,
                        const std::vector<std::array<int32_t, 2>>& values, int delta) {
    const int32_t w = rex::cvar::Query<int32_t>(name_w);
    const int32_t h = rex::cvar::Query<int32_t>(name_h);
    int index = 0;
    for (int i = 0; i < static_cast<int>(values.size()); ++i) {
      if (values[i][0] == w && values[i][1] == h) {
        index = i;
      }
    }
    const int n = static_cast<int>(values.size());
    index = ((index + delta) % n + n) % n;
    SetString(name_w, std::to_string(values[index][0]));
    SetString(name_h, std::to_string(values[index][1]));
  }
  // Quick presets for GPUs of different power. They mainly change the internal
  // resolution (GPU cost) and the texture cache limit.
  // The current aspect (16:9, 21:9...) is kept and only the vertical resolution
  // changes, so an ultrawide setup isn't broken.
  static const char* PresetName(int index) {
    static const char* names[] = {"Low", "Medium", "High", "Ultra", "Custom"};
    return L(names[index]);
  }
  // Preset heights/cache limits (the width depends on the aspect).
  static const int* PresetHeights() {
    static const int heights[4] = {540, 720, 900, 1080};
    return heights;
  }
  static const int* PresetCaches() {
    static const int caches[4] = {256, 512, 768, 1024};
    return caches;
  }
  // Preset matching the current configuration (4 = Custom).
  static int DetectPresetIndex() {
    if (!Available("video_mode_height") || !Available("texture_cache_memory_limit_soft")) {
      return 4;
    }
    const int32_t height = rex::cvar::Query<int32_t>("video_mode_height");
    const int32_t cache = rex::cvar::Query<int32_t>("texture_cache_memory_limit_soft");
    const int* heights = PresetHeights();
    const int* caches = PresetCaches();
    for (int i = 0; i < 4; ++i) {
      if (height == heights[i] && cache == caches[i]) {
        return i;
      }
    }
    return 4;
  }
  static void ApplyPresetValues(int index) {
    const int* heights = PresetHeights();
    const int* caches = PresetCaches();
    const int32_t current_width = std::max(1, rex::cvar::Query<int32_t>("video_mode_width"));
    const int32_t current_height = std::max(1, rex::cvar::Query<int32_t>("video_mode_height"));
    const double aspect = double(current_width) / double(current_height);
    const int32_t height = heights[index];
    // Multiple of 8 (guest RT alignment) and within the guest video mode limit
    // (0x0FFF wide).
    int32_t width = int32_t(std::lround(double(height) * aspect / 8.0) * 8.0);
    width = std::clamp(width, 640, 4095);
    SetString("video_mode_width", std::to_string(width));
    SetString("video_mode_height", std::to_string(height));
    SetString("texture_cache_memory_limit_soft", std::to_string(caches[index]));
    SetString("present_effect", "fsr");
  }
  void CyclePreset(int delta) {
    preset_index_ = (preset_index_ + delta + 5) % 5;
    if (preset_index_ < 4) {
      ApplyPresetValues(preset_index_);
    }
  }
  void ChangeSelected(int delta) {
    if (!IsSelectable(selected_) || !rows_[selected_].change) {
      return;
    }
    rows_[selected_].change(delta);
    if (selected_ != preset_row_ && selected_ != quit_row_ && selected_ != menu_language_row_) {
      preset_index_ = 4;
    }
  }

  // Quit row: two steps so the game isn't closed by mistake. The first change
  // arms it ("confirm"), the second (right/Enter/click) quits, and left disarms
  // it.
  void ChangeQuit(int delta) {
    if (!quit_) {
      return;
    }
    if (!quit_armed_) {
      quit_armed_ = true;
      return;
    }
    if (delta < 0) {
      quit_armed_ = false;
      return;
    }
    quit_armed_ = false;
    if (dirty_) {
      rex::cvar::SaveConfig(config_path_);
      dirty_ = false;
    }
    // Close() deletes the dialog, so the callback is copied first.
    std::function<void()> quit = quit_;
    Close();
    if (quit) {
      quit();
    }
  }
  static void CycleBool(const char* name, int delta) {
    bool value = rex::cvar::Query<bool>(name);
    if (delta != 0) {
      value = !value;
    }
    SetString(name, value ? "true" : "false");
  }

  void BuildRows() {
    if (Available("sh_menu_language")) {
      AddHeader("Interface");
      menu_language_row_ = static_cast<int>(rows_.size());
      rows_.push_back(
          {"Menu language",
           [] {
             return std::string(QueryString("sh_menu_language") == "spanish" ? "Espanol"
                                                                             : "English");
           },
           [](int d) {
             static const std::vector<std::string> values = {"english", "spanish"};
             CycleString("sh_menu_language", values, d);
           },
           false});
    }

    AddHeader("Image");

    preset_row_ = static_cast<int>(rows_.size());
    rows_.push_back(
        {"Preset",
         [this] { return std::string(PresetName(preset_index_)); },
         [this](int d) { CyclePreset(d); },
         true});

    rows_.push_back(
        {"Internal resolution",
         [] { return FormatPair("video_mode_width", "video_mode_height", ""); },
         [](int d) {
           static const std::vector<std::array<int32_t, 2>> presets = {
               {640, 360},   {960, 540},   {1280, 720},  {1600, 900},
               {1920, 1080}, {2560, 1080}, {2560, 1440}, {3440, 1440}};
           CyclePair("video_mode_width", "video_mode_height", presets, d);
           // Manual choice: keep the automatic aspect adjustment from changing it.
           SetString("sh_aspect", "keep");
         },
         true});

    if (Available("sh_aspect")) {
      rows_.push_back(
          {"Internal aspect",
           [] {
             const std::string mode = QueryString("sh_aspect");
             return mode == "keep" ? std::string("manual") : mode;
           },
           [](int d) {
             static const std::vector<std::string> values = {"auto", "16:9", "21:9", "32:9"};
             if (QueryString("sh_aspect") == "keep") {
               SetString("sh_aspect", "auto");
               return;
             }
             CycleString("sh_aspect", values, d);
           },
           true});
    }

    rows_.push_back(
        {"Draw scale",
         [] { return FormatInt("resolution_scale"); },
         [](int d) {
           // Limit the scale so the render width (video_mode_width * scale)
           // doesn't exceed the safe limit with the ROV path. At 1080p, 2x is
           // already 3840 px and saturated the GPU (Application Hang). The cap is 2560.
           const int32_t width = std::max(1, rex::cvar::Query<int32_t>("video_mode_width"));
           const int max_scale = std::max(1, std::min(4, 2560 / width));
           std::vector<int32_t> values;
           for (int32_t i = 1; i <= max_scale; ++i) {
             values.push_back(i);
           }
           CycleInt("resolution_scale", values, d);
         },
         true});

    rows_.push_back(
        {"Render path",
         [] {
           const std::string path = QueryString("render_target_path_d3d12");
           return path.empty() ? std::string("auto") : path;
         },
         [](int d) {
           static const std::vector<std::string> values = {"", "rtv", "rov"};
           CycleString("render_target_path_d3d12", values, d);
         },
         true});

    AddHeader("Upscaling and presentation");

    rows_.push_back(
        {"Presentation",
         [] { return QueryString("present_effect"); },
         [](int d) {
           static const std::vector<std::string> values = {"bilinear", "cas", "fsr", "fsr2", "fsr3"};
           CycleString("present_effect", values, d);
         },
         true});

    if (Available("present_fsr_quality_mode")) {
      rows_.push_back(
          {"FSR quality",
           [] { return QueryString("present_fsr_quality_mode"); },
           [](int d) {
             static const std::vector<std::string> values = {
                 "auto", "nativeaa", "quality", "balanced", "performance", "ultra_performance"};
             CycleString("present_fsr_quality_mode", values, d);
           },
           true});
    }

    rows_.push_back(
        {"Sharpness (FSR)",
         [] { return FormatValue("present_fsr_sharpness_reduction", ""); },
         [](int d) { CycleDouble("present_fsr_sharpness_reduction", 0.2, 0.0, 2.0, d); },
         true});

    if (Available("swap_post_effect")) {
      rows_.push_back(
          {"Presentation antialiasing",
           [] { return QueryString("swap_post_effect"); },
           [](int d) {
             static const std::vector<std::string> values = {"none", "fxaa", "fxaa_extreme"};
             CycleString("swap_post_effect", values, d);
           },
           true});
    }

    rows_.push_back(
        {"Dither", [] { return OnOff("present_dither"); },
         [](int d) { CycleBool("present_dither", d); }, true});

    if (Available("present_letterbox")) {
      rows_.push_back({"Letterbox", [] { return OnOff("present_letterbox"); },
                       [](int d) { CycleBool("present_letterbox", d); }, true});
    }

    AddHeader("Display");

    rows_.push_back(
        {"Fullscreen",
         [] { return OnOff("fullscreen"); },
         [](int d) { CycleBool("fullscreen", d); },
         true});

    if (Available("window_width") && Available("window_height")) {
      rows_.push_back(
          {"Window size",
           [] {
             const int32_t w = rex::cvar::Query<int32_t>("window_width");
             const int32_t h = rex::cvar::Query<int32_t>("window_height");
             return w > 0 && h > 0 ? FormatPair("window_width", "window_height", "")
                                   : std::string(L("automatic"));
           },
           [](int d) {
             static const std::vector<std::array<int32_t, 2>> sizes = {
                 {0, 0},       {1280, 720},  {1600, 900},  {1920, 1080},
                 {2560, 1080}, {2560, 1440}, {3440, 1440}, {3840, 1080}};
             CyclePair("window_width", "window_height", sizes, d);
           },
           true});
    }

    rows_.push_back(
        {"Refresh rate",
         [] {
           char buffer[32];
           std::snprintf(buffer, sizeof(buffer), "%.0f Hz",
                         rex::cvar::Query<double>("video_mode_refresh_rate"));
           return std::string(buffer);
         },
         [](int d) {
           static const std::vector<std::string> values = {"30",  "60",  "72", "90",
                                                           "120", "144", "165", "240"};
           CycleString("video_mode_refresh_rate", values, d);
         },
         true});

    rows_.push_back(
        {"VSync", [] { return OnOff("vsync"); }, [](int d) { CycleBool("vsync", d); }, true});

    rows_.push_back(
        {"FPS limit",
         [] {
           if (!rex::cvar::Query<bool>("d3d12_present_frame_limiter")) {
             return std::string(L("off"));
           }
           char buffer[32];
           std::snprintf(buffer, sizeof(buffer), "%d FPS",
                         static_cast<int>(rex::cvar::Query<double>(
                             "d3d12_present_frame_limiter_fps")));
           return std::string(buffer);
         },
         [](int d) {
           static const int values[] = {0, 30, 60, 120, 144};
           const int count = 5;
           int index = 0;
           if (rex::cvar::Query<bool>("d3d12_present_frame_limiter")) {
             const int fps = static_cast<int>(
                 rex::cvar::Query<double>("d3d12_present_frame_limiter_fps"));
             for (int i = 0; i < count; ++i) {
               if (values[i] == fps) {
                 index = i;
               }
             }
           }
           index = ((index + d) % count + count) % count;
           if (index == 0) {
             SetString("d3d12_present_frame_limiter", "false");
           } else {
             SetString("d3d12_present_frame_limiter", "true");
             SetString("d3d12_present_frame_limiter_fps", std::to_string(values[index]));
           }
         },
         false});

    if (Available("d3d12_present_adaptive_tearing")) {
      rows_.push_back(
          {"Tearing / VRR",
           [] {
             if (rex::cvar::Query<bool>("d3d12_allow_variable_refresh_rate_and_tearing")) {
               return std::string(L("always (VRR)"));
             }
             return std::string(rex::cvar::Query<bool>("d3d12_present_adaptive_tearing")
                                    ? L("auto (only below 60)")
                                    : L("off (60/30)"));
           },
           [](int d) {
             const bool always =
                 rex::cvar::Query<bool>("d3d12_allow_variable_refresh_rate_and_tearing");
             const bool adaptive = rex::cvar::Query<bool>("d3d12_present_adaptive_tearing");
             int state = always ? 2 : (adaptive ? 1 : 0);
             state = ((state + d) % 3 + 3) % 3;
             SetString("d3d12_allow_variable_refresh_rate_and_tearing",
                       state == 2 ? "true" : "false");
             SetString("d3d12_present_adaptive_tearing", state == 0 ? "false" : "true");
           },
           false});
    } else {
      rows_.push_back(
          {"Tearing / VRR",
           [] { return OnOff("d3d12_allow_variable_refresh_rate_and_tearing"); },
           [](int d) { CycleBool("d3d12_allow_variable_refresh_rate_and_tearing", d); },
           true});
    }

    AddHeader("Quality");

    rows_.push_back(
        {"Texture cache (MB)",
         [] { return FormatInt("texture_cache_memory_limit_soft"); },
         [](int d) {
           static const std::vector<int32_t> values = {128, 256, 384, 512, 768, 1024, 1536, 2048};
           CycleInt("texture_cache_memory_limit_soft", values, d);
         },
         true});

    if (Available("native_2x_msaa")) {
      rows_.push_back({"Native 2x MSAA", [] { return OnOff("native_2x_msaa"); },
                       [](int d) { CycleBool("native_2x_msaa", d); }, true});
    }

    if (Available("anisotropic_override")) {
      rows_.push_back(
          {"Anisotropic filtering",
           [] {
             const int32_t value = rex::cvar::Query<int32_t>("anisotropic_override");
             if (value < 0) {
               return std::string(L("automatic"));
             }
             if (value == 0) {
               return std::string(L("off"));
             }
             return std::to_string(value) + "x";
           },
           [](int d) {
             static const std::vector<int32_t> values = {-1, 0, 2, 4, 8, 16};
             CycleInt("anisotropic_override", values, d);
           },
           true});
    }

    AddHeader("Performance");

    if (Available("async_shader_compilation")) {
      rows_.push_back({"Background shaders", [] { return OnOff("async_shader_compilation"); },
                       [](int d) { CycleBool("async_shader_compilation", d); }, false});
    }

    if (Available("sh_pso_async")) {
      rows_.push_back({"Non-blocking PSO", [] { return OnOff("sh_pso_async"); },
                       [](int d) { CycleBool("sh_pso_async", d); }, false});
    }

    if (Available("d3d12_pipeline_creation_threads")) {
      rows_.push_back(
          {"PSO creation threads",
           [] {
             const int32_t value = rex::cvar::Query<int32_t>("d3d12_pipeline_creation_threads");
             return value < 0 ? std::string(L("automatic")) : std::to_string(value);
           },
           [](int d) {
             static const std::vector<int32_t> values = {-1, 2, 4, 8, 16};
             CycleInt("d3d12_pipeline_creation_threads", values, d);
           },
           true});
    }

    AddHeader("Game");

    if (Available("sh_60fps")) {
      rows_.push_back({"60 FPS patch", [] { return OnOff("sh_60fps"); },
                       [](int d) { CycleBool("sh_60fps", d); }, false});
    }

    rows_.push_back(
        {"Unlock levels", [] { return OnOff("sh_unlock_levels"); },
         [](int d) { CycleBool("sh_unlock_levels", d); }, false});

    if (Available("sh_unlock_levels_max")) {
      rows_.push_back(
          {"Max level",
           [] { return FormatInt("sh_unlock_levels_max"); },
           [](int d) {
             static const std::vector<int32_t> values = {1, 2, 3, 4, 5, 6, 7, 8};
             CycleInt("sh_unlock_levels_max", values, d);
           },
           false});
    }

    rows_.push_back(
        {"Game language",
         [] { return QueryString("sh_language"); },
         [](int d) {
           static const std::vector<std::string> values = {
               "auto", "english", "french", "italian", "german", "spanish", "japanese"};
           CycleString("sh_language", values, d);
         },
         true});

    AddHeader("Input and audio");

    if (Available("input_backend")) {
      rows_.push_back(
          {"Input backend",
           [] { return QueryString("input_backend"); },
           [](int d) {
             static const std::vector<std::string> values = {"sdl", "xinput"};
             CycleString("input_backend", values, d);
           },
           true});
    }

    if (Available("sh_rumble_timeout_ms")) {
      rows_.push_back(
          {"Max rumble (ms)",
           [] { return FormatInt("sh_rumble_timeout_ms"); },
           [](int d) {
             static const std::vector<int32_t> values = {0, 2000, 5000, 10000, 30000};
             CycleInt("sh_rumble_timeout_ms", values, d);
           },
           false});
    }

    if (Available("audio_mute")) {
      rows_.push_back({"Mute", [] { return OnOff("audio_mute"); },
                       [](int d) { CycleBool("audio_mute", d); }, false});
    }

    AddHeader("Development");

    rows_.push_back(
        {"Level warp (dev)",
         [this] {
           static const char* levels[] = {"lvl1_manor_interior", "lvl2_shanty",
                                           "lvl3_manor_catacombs", "lvl4_carnival",
                                           "lvl5_manor_grounds", "lvl7_manor_chapel",
                                           "survival_arena", "frontend"};
           return std::string(levels[warp_index_]);
         },
         [this](int d) {
           static const char* levels[] = {"lvl1_manor_interior", "lvl2_shanty",
                                           "lvl3_manor_catacombs", "lvl4_carnival",
                                           "lvl5_manor_grounds", "lvl7_manor_chapel",
                                           "survival_arena", "frontend"};
           warp_index_ = (warp_index_ + d + 8) % 8;
           SetString("sh_warp", levels[warp_index_]);
         },
         false});

    quit_row_ = static_cast<int>(rows_.size());
    rows_.push_back(
        {"Quit game",
         [this] {
           if (!quit_) {
             return std::string(L("unavailable"));
           }
           return quit_armed_ ? std::string(L("confirm (Yes/No)"))
                             : std::string(L("press to quit"));
         },
         [this](int d) { ChangeQuit(d); },
         false,
         true});
  }


  std::filesystem::path config_path_;
  std::function<void()> quit_;
  std::vector<Row> rows_;
  std::vector<rex::ui::VirtualKey> pending_keys_;
  std::set<rex::ui::VirtualKey> keys_down_;
  int selected_ = 0;
  int first_row_ = 0;
  int visible_rows_ = 16;
  int hovered_ = -1;
  ImVec2 last_mouse_ = ImVec2(-FLT_MAX, -FLT_MAX);
  int preset_index_ = 3;
  int preset_row_ = 1;
  int quit_row_ = 0;
  int menu_language_row_ = -1;
  int warp_index_ = 0;
  bool dirty_ = false;
  bool quit_armed_ = false;
  float saved_flash_ = 0.0f;

  static constexpr ImVec4 kAccent = ImVec4(0.85f, 0.11f, 0.11f, 1.0f);
  static constexpr ImVec4 kText = ImVec4(0.93f, 0.91f, 0.86f, 1.0f);
  static constexpr ImVec4 kTextDim = ImVec4(0.62f, 0.58f, 0.54f, 1.0f);
  static constexpr ImVec4 kDim = ImVec4(0.48f, 0.44f, 0.42f, 1.0f);
};
