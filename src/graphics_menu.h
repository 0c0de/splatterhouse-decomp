// Splatterhouse (2010) port - pantalla de opciones graficas dentro del juego.
//
// Pantalla completa con el estilo del juego (fondo oscuro, rojo/crema), NO una
// ventana flotante. Se abre/cierra con el keybind "bind_graphics" (F5 por
// defecto, reasignable desde el overlay de ajustes del runtime). Escribe las
// cvars del port y las persiste en splatterhouse.toml con "Guardar".

#pragma once

#include <algorithm>
#include <array>
#include <cfloat>
#include <cstdio>
#include <filesystem>
#include <functional>
#include <set>
#include <string>
#include <vector>

#include <imgui.h>
#include <rex/cvar.h>
#include <rex/ui/imgui_dialog.h>
#include <rex/ui/virtual_key.h>

class GraphicsMenuDialog;

// El dialogo se registra en el ImGuiDrawer al construirse y se auto-borra al
// llamar Close(). Este puntero global permite al keybind abrirlo/cerrarlo.
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
    // La primera fila es un titulo de seccion: empezar en algo seleccionable.
    selected_ = NextSelectable(0, +1);
  }
  ~GraphicsMenuDialog() override {
    if (g_graphics_menu == this) {
      g_graphics_menu = nullptr;
    }
  }

  void RequestClose() { Close(); }

  // El teclado no se lee de ImGui: las teclas las recibe nuestro propio
  // WindowInputListener (z-order 128, el mismo que ya captura F5 antes que el
  // ImGuiDrawer), que las encola aqui y el menu consume al dibujarse. Asi el
  // menu no depende de que ImGui tenga el foco ni de io.WantCaptureKeyboard.
  void PushKey(rex::ui::VirtualKey key) {
    // El autorepeat del sistema reenvia la pulsacion; solo interested la primera.
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

    // Texto mas grande: se escala la fuente de todo el menu.
    const float font_scale = 1.7f;

    ImGui::SetCursorPos(ImVec2(panel_x, 70.0f * scale));
    ImGui::SetWindowFontScale(2.6f);
    ImGui::TextColored(kAccent, "OPCIONES");
    ImGui::SetWindowFontScale(font_scale);
    ImGui::SetCursorPosX(panel_x);
    ImGui::TextColored(kDim, "Splatterhouse - opciones del port");

    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 24.0f * scale);

    // Con ~30 filas no caben todas: se dibuja solo la ventana que cabe y se
    // mantiene la seleccion a la vista.
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
        ImGui::TextColored(kAccent, "-- %s", row.label.c_str());
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 12.0f * scale);
        continue;
      }

      ImGui::SetCursorPosX(panel_x);
      ImGui::TextColored(selected ? kAccent : kDim, selected ? ">" : " ");

      ImGui::SameLine(panel_x + 34.0f * scale * font_scale);
      ImGui::TextColored(selected ? (row.danger ? kAccent : kText) : (row.danger ? kDim : kTextDim),
                         "%s", row.label.c_str());

      ImGui::SameLine(panel_x + panel_w * 0.58f);
      const std::string value = row.get();
      ImGui::TextColored(selected ? kAccent : kText, "%s %s %s", selected ? "<" : " ",
                         value.c_str(), selected ? ">" : " ");
      if (row.restart) {
        ImGui::SameLine();
        ImGui::TextColored(kDim, " [reinicio]");
      }

      if (ImGui::IsMouseHoveringRect(ImVec2(panel_x, ImGui::GetItemRectMin().y - 3.0f * scale),
                                     ImVec2(panel_x + panel_w,
                                            ImGui::GetItemRectMax().y + 3.0f * scale))) {
        hovered_ = i;
      }
      ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 12.0f * scale);
    }

    // El hover SOLO mueve la seleccion si el raton se ha movido de verdad. Si se
    // reasignara cada frame, dejaria fijada la fila bajo el cursor y el teclado
    // y la rueda no harian nada (que es como estaba antes).
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

  // Una fila solo se pinta si su cvar existe: algunas opciones dependen de como
  // se compilo el runtime (p.ej. la calidad FSR solo existe con FidelityFX) o de
  // que el plugin de GPU este cargado. GetFlagByName devuelve "" si no esta
  // registrado, asi que sin este filtro el menu ofreceria valores que luego se
  // rechazan al escribirlos.
  static bool Available(const char* name) {
    return rex::cvar::GetFlagInfo(name) != nullptr;
  }
  void AddHeader(const char* label) {
    rows_.push_back({label, {}, {}, false, false, true});
  }
  bool IsSelectable(int index) const {
    return index >= 0 && index < static_cast<int>(rows_.size()) && !rows_[index].header;
  }
  // Navegacion que salta los titulos de seccion.
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
    // Teclas encoladas por el listener de la ventana (fuente fiable: no depende
    // del foco de ImGui).
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

    // Rueda: mueve la seleccion. Antes se ignoraba salvo sobre la ultima fila
    // visible porque el hover reasignaba selected_ cada frame.
    if (mouse_in_panel && io.MouseWheel != 0.0f) {
      selected_ = NextSelectable(selected_, io.MouseWheel < 0.0f ? +1 : -1);
    }

    if (saved_flash_ > 0.0f) {
      saved_flash_ = std::max(0.0f, saved_flash_ - io.DeltaTime);
    }
  }

  // Cambiar la fila seleccionada no es "cambio sin guardar" cuando la fila es la
  // de salida del juego (ChangeQuit ya guarda lo que haya).
  void MarkDirty() {
    if (selected_ != quit_row_) {
      dirty_ = true;
    }
  }

  void DrawFooter(ImGuiIO& io, float panel_x, float panel_w, float scale, float font_scale) {
    ImGui::SetWindowFontScale(font_scale);
    ImGui::SetCursorPos(ImVec2(panel_x, io.DisplaySize.y - 92.0f * scale));
    const int count = static_cast<int>(rows_.size());
    std::string hint = "Flechas: navegar/cambiar   S: guardar   Esc/F5: cerrar";
    if (dirty_) {
      hint += "   (cambios sin guardar)";
    }
    if (count > visible_rows_) {
      char buffer[64];
      std::snprintf(buffer, sizeof(buffer), "   [%d-%d de %d, rueda para_scroll]",
                    first_row_ + 1, std::min(count, first_row_ + visible_rows_), count);
      hint += buffer;
    }
    ImGui::TextColored(kDim, "%s", hint.c_str());
    if (saved_flash_ > 0.0f) {
      ImGui::SetCursorPosX(panel_x);
      ImGui::TextColored(kAccent, "Guardado en %s", config_path_.filename().string().c_str());
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
      ImGui::TextColored(kDim, "Reiniciar para aplicar: %s", names.c_str());
    }
    if (quit_armed_) {
      ImGui::SetCursorPosX(panel_x);
      ImGui::TextColored(kAccent, "? Salir del juego: Si = Derecha/Enter/click, No = Izquierda");
    }
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
  // Ciclo sobre valores enteros escritos como texto (los cvars int32 se guardan
  // igual en el toml).
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
    return std::string(rex::cvar::Query<bool>(name) ? "activado" : "desactivado");
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
  // Presets rapidos para GPUs de distinta potencia. Cambian sobre todo la
  // resolucion interna (coste GPU) y el limite de la cache de texturas.
  static const char* PresetName(int index) {
    static const char* names[] = {"Bajo", "Medio", "Alto", "Ultra", "Personalizado"};
    return names[index];
  }
  static void ApplyPresetValues(int index) {
    static const int presets[4][3] = {
        {960, 540, 256}, {1280, 720, 512}, {1600, 900, 768}, {1920, 1080, 1024}};
    SetString("video_mode_width", std::to_string(presets[index][0]));
    SetString("video_mode_height", std::to_string(presets[index][1]));
    SetString("texture_cache_memory_limit_soft", std::to_string(presets[index][2]));
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
    if (selected_ != preset_row_ && selected_ != quit_row_) {
      preset_index_ = 4;
    }
  }

  // Fila de salida: dos pasos para no cerrar el juego por error. El primer
  // cambio la arma ("confirmar"), el segundo (derecha/Enter/click) sale, y
  // izquierda la desarma.
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
    // Close() borra el dialogo, asi que la llamada se copia antes.
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
    AddHeader("Imagen");

    preset_row_ = static_cast<int>(rows_.size());
    rows_.push_back(
        {"Preset",
         [this] { return std::string(PresetName(preset_index_)); },
         [this](int d) { CyclePreset(d); },
         true});

    rows_.push_back(
        {"Resolucion interna",
         [] { return FormatPair("video_mode_width", "video_mode_height", ""); },
         [](int d) {
           static const std::vector<std::array<int32_t, 2>> presets = {
               {640, 360}, {960, 540}, {1280, 720}, {1600, 900}, {1920, 1080}, {2560, 1440}};
           CyclePair("video_mode_width", "video_mode_height", presets, d);
         },
         true});

    rows_.push_back(
        {"Escala de dibujo",
         [] { return FormatInt("resolution_scale"); },
         [](int d) {
           // Limitar la escala para que el ancho de render (video_mode_width *
           // escala) no supere el limite seguro con la ruta ROV. A 1080p, 2x ya
           // es 3840 px y saturaba la GPU (Application Hang). El tope es 2560.
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
        {"Ruta de render",
         [] {
           const std::string path = QueryString("render_target_path_d3d12");
           return path.empty() ? std::string("auto") : path;
         },
         [](int d) {
           static const std::vector<std::string> values = {"", "rtv", "rov"};
           CycleString("render_target_path_d3d12", values, d);
         },
         true});

    AddHeader("Reescalado y presentacion");

    rows_.push_back(
        {"Presentacion",
         [] { return QueryString("present_effect"); },
         [](int d) {
           static const std::vector<std::string> values = {"bilinear", "cas", "fsr", "fsr2", "fsr3"};
           CycleString("present_effect", values, d);
         },
         true});

    if (Available("present_fsr_quality_mode")) {
      rows_.push_back(
          {"Calidad FSR",
           [] { return QueryString("present_fsr_quality_mode"); },
           [](int d) {
             static const std::vector<std::string> values = {
                 "auto", "nativeaa", "quality", "balanced", "performance", "ultra_performance"};
             CycleString("present_fsr_quality_mode", values, d);
           },
           true});
    }

    rows_.push_back(
        {"Nitidez (FSR)",
         [] { return FormatValue("present_fsr_sharpness_reduction", ""); },
         [](int d) { CycleDouble("present_fsr_sharpness_reduction", 0.2, 0.0, 2.0, d); },
         true});

    if (Available("swap_post_effect")) {
      rows_.push_back(
          {"Antialias presentacion",
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

    AddHeader("Pantalla");

    rows_.push_back(
        {"Pantalla completa",
         [] { return OnOff("fullscreen"); },
         [](int d) { CycleBool("fullscreen", d); },
         true});

    if (Available("window_width") && Available("window_height")) {
      rows_.push_back(
          {"Tamano ventana",
           [] {
             const int32_t w = rex::cvar::Query<int32_t>("window_width");
             const int32_t h = rex::cvar::Query<int32_t>("window_height");
             return w > 0 && h > 0 ? FormatPair("window_width", "window_height", "")
                                   : std::string("automatico");
           },
           [](int d) {
             static const std::vector<std::array<int32_t, 2>> sizes = {
                 {0, 0}, {1280, 720}, {1600, 900}, {1920, 1080}};
             CyclePair("window_width", "window_height", sizes, d);
           },
           true});
    }

    rows_.push_back(
        {"Frecuencia",
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
        {"Limite de FPS",
         [] {
           if (!rex::cvar::Query<bool>("d3d12_present_frame_limiter")) {
             return std::string("desactivado");
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

    rows_.push_back(
        {"Tearing / VRR",
         [] { return OnOff("d3d12_allow_variable_refresh_rate_and_tearing"); },
         [](int d) { CycleBool("d3d12_allow_variable_refresh_rate_and_tearing", d); },
         true});

    AddHeader("Calidad");

    rows_.push_back(
        {"Cache de texturas (MB)",
         [] { return FormatInt("texture_cache_memory_limit_soft"); },
         [](int d) {
           static const std::vector<int32_t> values = {128, 256, 384, 512, 768, 1024, 1536, 2048};
           CycleInt("texture_cache_memory_limit_soft", values, d);
         },
         true});

    if (Available("native_2x_msaa")) {
      rows_.push_back({"MSAA 2x nativo", [] { return OnOff("native_2x_msaa"); },
                       [](int d) { CycleBool("native_2x_msaa", d); }, true});
    }

    if (Available("anisotropic_override")) {
      rows_.push_back(
          {"Filtro anisotropico",
           [] {
             const int32_t value = rex::cvar::Query<int32_t>("anisotropic_override");
             if (value < 0) {
               return std::string("automatico");
             }
             if (value == 0) {
               return std::string("desactivado");
             }
             return std::to_string(value) + "x";
           },
           [](int d) {
             static const std::vector<int32_t> values = {-1, 0, 2, 4, 8, 16};
             CycleInt("anisotropic_override", values, d);
           },
           true});
    }

    AddHeader("Rendimiento");

    if (Available("async_shader_compilation")) {
      rows_.push_back({"Shaders en segundo plano", [] { return OnOff("async_shader_compilation"); },
                       [](int d) { CycleBool("async_shader_compilation", d); }, false});
    }

    if (Available("sh_pso_async")) {
      rows_.push_back({"PSO sin bloqueos", [] { return OnOff("sh_pso_async"); },
                       [](int d) { CycleBool("sh_pso_async", d); }, false});
    }

    if (Available("d3d12_pipeline_creation_threads")) {
      rows_.push_back(
          {"Hilos para crear PSO",
           [] {
             const int32_t value = rex::cvar::Query<int32_t>("d3d12_pipeline_creation_threads");
             return value < 0 ? std::string("automatico") : std::to_string(value);
           },
           [](int d) {
             static const std::vector<int32_t> values = {-1, 2, 4, 8, 16};
             CycleInt("d3d12_pipeline_creation_threads", values, d);
           },
           true});
    }

    AddHeader("Juego");

    if (Available("sh_60fps")) {
      rows_.push_back({"Parche 60 FPS", [] { return OnOff("sh_60fps"); },
                       [](int d) { CycleBool("sh_60fps", d); }, false});
    }

    rows_.push_back(
        {"Desbloquear niveles", [] { return OnOff("sh_unlock_levels"); },
         [](int d) { CycleBool("sh_unlock_levels", d); }, false});

    if (Available("sh_unlock_levels_max")) {
      rows_.push_back(
          {"Nivel maximo",
           [] { return FormatInt("sh_unlock_levels_max"); },
           [](int d) {
             static const std::vector<int32_t> values = {1, 2, 3, 4, 5, 6, 7, 8};
             CycleInt("sh_unlock_levels_max", values, d);
           },
           false});
    }

    rows_.push_back(
        {"Idioma",
         [] { return QueryString("sh_language"); },
         [](int d) {
           static const std::vector<std::string> values = {
               "auto", "english", "french", "italian", "german", "spanish", "japanese"};
           CycleString("sh_language", values, d);
         },
         true});

    AddHeader("Entrada y audio");

    if (Available("input_backend")) {
      rows_.push_back(
          {"Backend de input",
           [] { return QueryString("input_backend"); },
           [](int d) {
             static const std::vector<std::string> values = {"sdl", "xinput"};
             CycleString("input_backend", values, d);
           },
           true});
    }

    if (Available("sh_rumble_timeout_ms")) {
      rows_.push_back(
          {"Vibracion maxima (ms)",
           [] { return FormatInt("sh_rumble_timeout_ms"); },
           [](int d) {
             static const std::vector<int32_t> values = {0, 2000, 5000, 10000, 30000};
             CycleInt("sh_rumble_timeout_ms", values, d);
           },
           false});
    }

    if (Available("audio_mute")) {
      rows_.push_back({"Silenciar", [] { return OnOff("audio_mute"); },
                       [](int d) { CycleBool("audio_mute", d); }, false});
    }

    AddHeader("Desarrollo");

    rows_.push_back(
        {"Warp nivel (dev)",
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
        {"Salir del juego",
         [this] {
           if (!quit_) {
             return std::string("no disponible");
           }
           return quit_armed_ ? std::string("confirmar (Si/No)") : std::string("pulsar para salir");
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
  int warp_index_ = 0;
  bool dirty_ = false;
  bool quit_armed_ = false;
  float saved_flash_ = 0.0f;

  static constexpr ImVec4 kAccent = ImVec4(0.85f, 0.11f, 0.11f, 1.0f);
  static constexpr ImVec4 kText = ImVec4(0.93f, 0.91f, 0.86f, 1.0f);
  static constexpr ImVec4 kTextDim = ImVec4(0.62f, 0.58f, 0.54f, 1.0f);
  static constexpr ImVec4 kDim = ImVec4(0.48f, 0.44f, 0.42f, 1.0f);
};
