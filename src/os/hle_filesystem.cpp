#include "splatterhouse/common.h"
#include "splatterhouse/config.h"
#include <spdlog/spdlog.h>

namespace splatterhouse::hle::vfs {

// Traducción de rutas Xbox 360 -> host filesystem
//  game:  -> GetConfig().gameRoot
//  d:     -> GetConfig().gameRoot (DVD)
//  hdd:   -> game/hdd (opcional)

::fs::path TranslatePath(std::string_view xboxPath) {
    auto& cfg = GetConfig();
    std::string lower(xboxPath);
    for (auto& c: lower) c = (char)tolower(c);

    if (lower.starts_with("game:") || lower.starts_with("game:\\") || lower.starts_with("game:/") || lower == "game:") {
        std::string rel = std::string(xboxPath.substr(5));
        // limpiar separadores
        for (auto& c: rel) if (c=='\\') c='/';
        while (!rel.empty() && rel.front()=='/') rel.erase(rel.begin());
        return cfg.game.gameRoot / rel;
    }
    if (lower.starts_with("d:") || lower.starts_with("d:\\")) {
        std::string rel = std::string(xboxPath.substr(2));
        for (auto& c: rel) if (c=='\\') c='/';
        while (!rel.empty() && rel.front()=='/') rel.erase(rel.begin());
        return cfg.game.gameRoot / rel;
    }
    // fallback: relativo a game root
    std::string rel(xboxPath);
    for (auto& c: rel) if (c=='\\') c='/';
    return cfg.game.gameRoot / rel;
}

} // namespace splatterhouse::hle::vfs
