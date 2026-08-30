#include "splatterhouse/hle_kernel.h"
#include <spdlog/spdlog.h>
#include <unordered_map>
#include <mutex>

namespace splatterhouse::hle {

static std::unordered_map<std::string, HleHandler> s_handlers;
static std::mutex s_mutex;

static std::string key(std::string_view dll, std::string_view name) {
    std::string k(dll);
    k += "!";
    k += name;
    // case-insensitive: lower
    for (auto& c : k) c = (char)tolower(c);
    return k;
}
static std::string keyOrd(std::string_view dll, uint32_t ord) {
    char buf[64];
    snprintf(buf, sizeof(buf), "%.*s!#%u", (int)dll.size(), dll.data(), ord);
    std::string k(buf);
    for (auto& c : k) c = (char)tolower(c);
    return k;
}

void Register(const char* dll, const char* name, uint32_t ord, HleHandler h) {
    std::lock_guard lk(s_mutex);
    s_handlers[key(dll, name)] = h;
    if (ord) s_handlers[keyOrd(dll, ord)] = h;
}

HleHandler FindHandler(std::string_view dll, std::string_view name) {
    std::lock_guard lk(s_mutex);
    auto it = s_handlers.find(key(dll, name));
    return it != s_handlers.end() ? it->second : nullptr;
}
HleHandler FindHandlerByOrdinal(std::string_view dll, uint32_t ord) {
    std::lock_guard lk(s_mutex);
    auto it = s_handlers.find(keyOrd(dll, ord));
    return it != s_handlers.end() ? it->second : nullptr;
}

void UnimplementedStub(const char* dll, const char* func) {
    spdlog::warn("[HLE] Unimplemented: {}!{} (stub, returning)", dll, func);
}
void NotImplementedFatal(const char* dll, const char* func) {
    spdlog::error("[HLE] NOT IMPLEMENTED FATAL: {}!{}", dll, func);
    std::abort();
}

// Declaraciones de los otros TUs
void RegisterXamExports();
void RegisterXboxkrnlExports();
void RegisterXbdmExports();
void RegisterD3DExports();

void RegisterAll() {
    spdlog::info("[HLE] Registrando exports...");
    RegisterXamExports();
    RegisterXboxkrnlExports();
    // opcionales:
    // RegisterXbdmExports();
    // RegisterD3DExports();
    spdlog::info("[HLE] {} handlers registrados", s_handlers.size());
}

} // namespace splatterhouse::hle
