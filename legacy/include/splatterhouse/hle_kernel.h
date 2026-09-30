#pragma once
#include "splatterhouse/common.h"

namespace splatterhouse::hle {

// Tabla de imports del XEX: cada NID / ordinal del xboxkrnl/xam se mapea
// a una función HLE aquí. XenonRecomp genera calls a `__imp__<dll>_<func>`
// que nosotros implementamos.

// Registro de una función HLE
using HleHandler = void(*)();

struct HleExport {
    const char* dllName;
    const char* funcName;
    uint32_t    ordinal;
    HleHandler  handler;
    const char* comment;
};

// Inicializa todas las tablas HLE. Debe llamarse antes de ejecutar código recompilado.
void RegisterAll();
void Register(const char* dll, const char* name, uint32_t ordinal, HleHandler handler);

// Lookup por nombre para el linker de XenonRecomp (ppc_context)
HleHandler FindHandler(std::string_view dll, std::string_view name);
HleHandler FindHandlerByOrdinal(std::string_view dll, uint32_t ordinal);

// Stubs genéricos
void UnimplementedStub(const char* dll, const char* func);
void NotImplementedFatal(const char* dll, const char* func);

#define SH_HLE_STUB(dll, name) \
    extern "C" void __imp__##dll##_##name()

#define SH_HLE_UNIMPLEMENTED(dll, name) \
    extern "C" void __imp__##dll##_##name() { hle::UnimplementedStub(#dll, #name); }

} // namespace splatterhouse::hle
