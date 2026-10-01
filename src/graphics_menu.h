// Splatterhouse (2010) port - pantalla de opciones graficas dentro del juego.
//
// Pantalla completa con el estilo del juego (fondo oscuro, rojo/crema), NO una
// ventana flotante. Se abre/cierra con el keybind "bind_graphics" (F5 por
// defecto, reasignable desde el overlay de ajustes del runtime). Escribe las
// cvars del port y las persiste en splatterhouse.toml con "Guardar".

#pragma once

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

#include <imgui.h>
#include <rex/cvar.h>
#include <rex/ui/imgui_dialog.h>

class GraphicsMenuDialog;

// El dialogo se registra en el ImGuiDrawer al construirse y se auto-borra al
// llamar Close(). Este puntero global permite al keybind abrirlo/cerrarlo.
static GraphicsMenuDialog* g_graphics_menu = nullptr;

class GraphicsMenuDialog : public rex::ui::ImGuiDialog {
 public:
  GraphicsMenuDialog(rex::ui::ImGuiDrawer* drawer, std::filesystem::path config_path)
      : rex::ui::ImGuiDialog(drawer), config_path_(std::move(config_path)) {
    g_graphics_menu = this;
    BuildRows();
  }
  ~GraphicsMenuDialog() override {
    if (g_graphics_menu == this) {
      g_graphics_menu = nullptr;
    }
  }

  void RequestClose() { Close(); }

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

    HandleInput(io);

    const float scale = std::max(1.0f, io.DisplaySize.x / 1280.0f);
    const float panel_w = std::min(io.DisplaySize.x * 0.74f, 940.0f * scale);
    const float panel_x = (io.DisplaySize.x - panel_w) * 0.5f;

    // Texto mas grande: se escala la fuente de todo el menu.
    const float font_scale = 1.7f;

    ImGui::SetCursorPos(ImVec2(panel_x, 70.0f * scale));
    ImGui::SetWindowFontScale(2.6f);
    ImGui::TextColored(kAccent, "OPCIONES");
    ImGui::SetWindowFontScale(font_scale);
    ImGui::SetCursorPosX(panel_x);
    ImGui::TextColored(kDim, "Splatterhouse - opciones del port");

    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 24.0f * scale);

    for (int i = 0; i < static_cast<int>(rows_.size()); ++i) {
      const bool selected = (i == selected_);
      const Row& row = rows_[i];

      ImGui::SetCursorPosX(panel_x);
      ImGui::TextColored(selected ? kAccent : kDim, selected ? ">" : " ");

      ImGui::SameLine(panel_x + 34.0f * scale * font_scale);
      ImGui::TextColored(selected ? kText : kTextDim, "%s", row.label.c_str());

      ImGui::SameLine(panel_x + panel_w * 0.62f);
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
        selected_ = i;
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
          ChangeSelected(+1);
          dirty_ = true;
        }
      }
      ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 12.0f * scale);
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
  };

  void HandleInput(ImGuiIO& io) {
    const int count = static_cast<int>(rows_.size());
    if (count == 0) {
      return;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) {
      selected_ = (selected_ + count - 1) % count;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) {
      selected_ = (selected_ + 1) % count;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow)) {
      ChangeSelected(-1);
      dirty_ = true;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_RightArrow) || ImGui::IsKeyPressed(ImGuiKey_Space) ||
        ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter)) {
      ChangeSelected(+1);
      dirty_ = true;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_S)) {
      rex::cvar::SaveConfig(config_path_);
      dirty_ = false;
      saved_flash_ = 2.0f;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Escape) || ImGui::IsKeyPressed(ImGuiKey_F5)) {
      if (dirty_) {
        rex::cvar::SaveConfig(config_path_);
      }
      Close();
      return;
    }
    if (saved_flash_ > 0.0f) {
      saved_flash_ = std::max(0.0f, saved_flash_ - io.DeltaTime);
    }
  }

  void DrawFooter(ImGuiIO& io, float panel_x, float panel_w, float scale, float font_scale) {
    ImGui::SetWindowFontScale(font_scale);
    ImGui::SetCursorPos(ImVec2(panel_x, io.DisplaySize.y - 92.0f * scale));
    ImGui::TextColored(kDim,
                       "Flechas: navegar/cambiar   S: guardar   Esc/F5: cerrar%s",
                       dirty_ ? "   (cambios sin guardar)" : "");
    if (saved_flash_ > 0.0f) {
      ImGui::SetCursorPosX(panel_x);
      ImGui::TextColored(kAccent, "Guardado en %s", config_path_.filename().string().c_str());
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
    rows_[selected_].change(delta);
    if (selected_ != preset_row_) {
      preset_index_ = 4;
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

    rows_.push_back(
        {"Desbloquear niveles",
         [] {
           return std::string(rex::cvar::Query<bool>("sh_unlock_levels") ? "activado"
                                                                         : "desactivado");
         },
         [](int d) { CycleBool("sh_unlock_levels", d); },
         false});

    rows_.push_back(
        {"Idioma",
         [] { return QueryString("sh_language"); },
         [](int d) {
           static const std::vector<std::string> values = {
               "auto", "english", "french", "italian", "german", "spanish", "japanese"};
           CycleString("sh_language", values, d);
         },
         true});

    preset_row_ = static_cast<int>(rows_.size());
    rows_.push_back(
        {"Preset",
         [this] { return std::string(PresetName(preset_index_)); },
         [this](int d) { CyclePreset(d); },
         true});

    rows_.push_back(
        {"Presentacion",
         [] { return QueryString("present_effect"); },
         [](int d) {
           static const std::vector<std::string> values = {"bilinear", "cas", "fsr", "fsr2",
                                                           "fsr3"};
           CycleString("present_effect", values, d);
         },
         true});

    rows_.push_back(
        {"Resolucion interna",
         [] {
           char buffer[32];
           std::snprintf(buffer, sizeof(buffer), "%d x %d",
                         rex::cvar::Query<int32_t>("video_mode_width"),
                         rex::cvar::Query<int32_t>("video_mode_height"));
           return std::string(buffer);
         },
         [](int d) {
           static const int presets[][2] = {{640, 360}, {1280, 720}, {1600, 900},
                                            {1920, 1080}, {2560, 1440}};
           const int count = 5;
           const int w = rex::cvar::Query<int32_t>("video_mode_width");
           const int h = rex::cvar::Query<int32_t>("video_mode_height");
           int index = 0;
           for (int i = 0; i < count; ++i) {
             if (presets[i][0] == w && presets[i][1] == h) {
               index = i;
             }
           }
           index = ((index + d) % count + count) % count;
           SetString("video_mode_width", std::to_string(presets[index][0]));
           SetString("video_mode_height", std::to_string(presets[index][1]));
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

    rows_.push_back(
        {"VSync",
         [] { return std::string(rex::cvar::Query<bool>("vsync") ? "activado" : "desactivado"); },
         [](int d) { CycleBool("vsync", d); },
         true});

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
        {"Nitidez (FSR)",
         [] { return FormatValue("present_fsr_sharpness_reduction", ""); },
         [](int d) { CycleDouble("present_fsr_sharpness_reduction", 0.2, 0.0, 2.0, d); },
         true});

    rows_.push_back(
        {"Dither",
         [] { return std::string(rex::cvar::Query<bool>("present_dither") ? "activado" : "desactivado"); },
         [](int d) { CycleBool("present_dither", d); },
         true});

    rows_.push_back(
        {"Escala de dibujo",
         [] { return std::to_string(rex::cvar::Query<int32_t>("resolution_scale")); },
         [](int d) {
           // Limitar la escala para que el ancho de render (video_mode_width *
           // escala) no supere el limite seguro con la ruta ROV. A 1080p, 2x ya
           // es 3840 px y saturaba la GPU (Application Hang). El tope es 2560.
           const int32_t width = std::max(1, rex::cvar::Query<int32_t>("video_mode_width"));
           const int max_scale = std::max(1, std::min(4, 2560 / width));
           std::vector<std::string> values;
           for (int i = 1; i <= max_scale; ++i) {
             values.push_back(std::to_string(i));
           }
           CycleString("resolution_scale", values, d);
         },
         true});
  }

  std::filesystem::path config_path_;
  std::vector<Row> rows_;
  int selected_ = 0;
  int preset_index_ = 3;
  int preset_row_ = 1;
  int warp_index_ = 0;
  bool dirty_ = false;
  float saved_flash_ = 0.0f;

  static constexpr ImVec4 kAccent = ImVec4(0.85f, 0.11f, 0.11f, 1.0f);
  static constexpr ImVec4 kText = ImVec4(0.93f, 0.91f, 0.86f, 1.0f);
  static constexpr ImVec4 kTextDim = ImVec4(0.62f, 0.58f, 0.54f, 1.0f);
  static constexpr ImVec4 kDim = ImVec4(0.48f, 0.44f, 0.42f, 1.0f);
};
