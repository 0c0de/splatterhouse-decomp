#pragma once
#include "splatterhouse/common.h"

namespace splatterhouse {

struct WindowConfig {
    int width = 1280;
    int height = 720;
    bool fullscreen = false;
    bool vsync = true;
    int maxFps = 0; // 0 = unlimited, 60/30 for cap
};

struct AudioConfig {
    int sampleRate = 48000;
    bool enable = true;
};

struct GameConfig {
    fs::path gameRoot = "game"; // carpeta donde el usuario pone los archivos extraídos del disco
    bool devMode = false;
    bool skipIntro = false;
};

// Resuelve una ruta relativa buscando en: cwd, exe dir, y hasta 4 padres del exe
// (VS lanza el exe desde build/<preset>/, el proyecto está en la raíz)
fs::path ResolveProjectPath(const fs::path& relative);

struct AppConfig {
    WindowConfig window{};
    AudioConfig  audio{};
    GameConfig   game{};
    int logLevel = 2; // 0 trace .. 5 off

    static AppConfig loadOrCreate(const fs::path& path);
    bool save(const fs::path& path) const;
};

// Ruta global (singleton simple)
AppConfig& GetConfig();
fs::path GetConfigPath();

} // namespace splatterhouse
