#include "splatterhouse/config.h"
#include <spdlog/spdlog.h>
#include <toml++/toml.hpp>
#include <fstream>
#ifdef _WIN32
#include <windows.h>
#endif

namespace splatterhouse {

fs::path GetExeDir() {
#ifdef _WIN32
    char buf[MAX_PATH] = {};
    GetModuleFileNameA(nullptr, buf, MAX_PATH);
    return fs::path(buf).parent_path();
#else
    return fs::current_path();
#endif
}

// Resuelve una ruta relativa buscando en: cwd, exe dir, y hasta 4 padres del exe
// (VS lanza el exe desde build/<preset>/, el proyecto está en la raíz)
fs::path ResolveProjectPath(const fs::path& relative) {
    if (relative.is_absolute()) return relative;
    std::error_code ec;
    std::vector<fs::path> candidates;
    candidates.push_back(fs::current_path(ec) / relative);
    fs::path exeDir = GetExeDir();
    candidates.push_back(exeDir / relative);
    fs::path cur = exeDir;
    for (int i = 0; i < 4; ++i) {
        cur = cur / "..";
        candidates.push_back(cur / relative);
    }
    for (auto& c : candidates) {
        if (fs::exists(c, ec)) return fs::weakly_canonical(c, ec);
    }
    return relative; // fallback sin resolver
}

fs::path GetConfigPath() {
#ifdef _WIN32
    // Busca config.toml en cwd o junto al proyecto
    return ResolveProjectPath("config.toml");
#else
    auto home = getenv("HOME");
    if (home) return fs::path(home) / ".config" / "splatterhouse" / "config.toml";
    return fs::path("config.toml");
#endif
}

AppConfig& GetConfig() {
    static AppConfig g_cfg;
    return g_cfg;
}

AppConfig AppConfig::loadOrCreate(const fs::path& path) {
    AppConfig cfg{};
    if (!fs::exists(path)) {
        cfg.save(path);
        spdlog::info("Config creada en {}", path.string());
        GetConfig() = cfg;
        return cfg;
    }
    try {
        auto tbl = toml::parse_file(path.string());

        if (auto* win = tbl["window"].as_table()) {
            cfg.window.width      = win->at("width").value_or(cfg.window.width);
            cfg.window.height     = win->at("height").value_or(cfg.window.height);
            cfg.window.fullscreen = win->at("fullscreen").value_or(cfg.window.fullscreen);
            cfg.window.vsync      = win->at("vsync").value_or(cfg.window.vsync);
            cfg.window.maxFps     = win->at("maxFps").value_or(cfg.window.maxFps);
        }
        if (auto* aud = tbl["audio"].as_table()) {
            cfg.audio.sampleRate = aud->at("sampleRate").value_or(cfg.audio.sampleRate);
            cfg.audio.enable     = aud->at("enable").value_or(cfg.audio.enable);
        }
        if (auto* game = tbl["game"].as_table()) {
            if (auto v = game->at("root").value<std::string>()) cfg.game.gameRoot = *v;
            cfg.game.devMode   = game->at("devMode").value_or(cfg.game.devMode);
            cfg.game.skipIntro = game->at("skipIntro").value_or(cfg.game.skipIntro);
        }
        cfg.logLevel = tbl["logLevel"].value_or(cfg.logLevel);

    } catch (const toml::parse_error& err) {
        spdlog::error("Error parseando {}: {} ({}:{})", path.string(), err.description(), err.source().begin.line, err.source().begin.column);
    }
    // Resolver gameRoot relativo (cwd/exe/parents)
    cfg.game.gameRoot = ResolveProjectPath(cfg.game.gameRoot);
    GetConfig() = cfg;
    return cfg;
}

bool AppConfig::save(const fs::path& path) const {
    try {
        if (auto parent = path.parent_path(); !parent.empty()) fs::create_directories(parent);
        std::ofstream out(path);
        out << "# Splatterhouse Recompiled - Config\n";
        out << "logLevel = " << logLevel << " # 0 trace 1 debug 2 info 3 warn 4 error 5 off\n\n";
        out << "[window]\n";
        out << "width = " << window.width << "\n";
        out << "height = " << window.height << "\n";
        out << "fullscreen = " << (window.fullscreen ? "true" : "false") << "\n";
        out << "vsync = " << (window.vsync ? "true" : "false") << "\n";
        out << "maxFps = " << window.maxFps << " # 0 = ilimitado\n\n";
        out << "[audio]\n";
        out << "enable = " << (audio.enable ? "true" : "false") << "\n";
        out << "sampleRate = " << audio.sampleRate << "\n\n";
        out << "[game]\n";
        out << "root = \"" << game.gameRoot.generic_string() << "\"\n";
        out << "devMode = " << (game.devMode ? "true" : "false") << "\n";
        out << "skipIntro = " << (game.skipIntro ? "true" : "false") << "\n";
        return true;
    } catch (...) {
        return false;
    }
}

} // namespace splatterhouse
