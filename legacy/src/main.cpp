#include "splatterhouse/common.h"
#include "splatterhouse/config.h"
#include "splatterhouse/memory.h"
#include "splatterhouse/hle_kernel.h"

#include <spdlog/spdlog.h>
#include <SDL3/SDL.h>
#include <thread>
#include <atomic>
#include <fstream>

// XenonUtils para parsear XEX
#include <xex.h>
#include <image.h>

// PPC recompilado - solo si existe recompiled/
#ifdef __has_include
#  if __has_include("ppc_context.h")
#    include "ppc_context.h"
#    include "ppc_config.h"
#    define HAS_RECOMPILED 1
#  endif
#endif
#ifndef HAS_RECOMPILED
// Fallback si no hay recompilado (bootstrap)
struct PPCContext {};
#endif

// Forward decls de backends
namespace splatterhouse::gpu { bool Init(void* windowHandle, int w, int h); void Shutdown(); void Present(); void Resize(int w, int h); }
namespace splatterhouse::audio { bool Init(int sampleRate); void Shutdown(); }
namespace splatterhouse::input { void Init(); void Shutdown(); void Poll(); }

// Punto de entrada del juego recompilado (generado por XenonRecomp)
// XenonRecomp genera _xstart como entry_point del XEX (ver ppc_config.h)
#ifdef HAS_RECOMPILED
extern PPC_FUNC(_xstart);
extern PPCFuncMapping PPCFuncMappings[];
#endif
// Stubs bootstrap por si no hay recompilado
extern "C" {
    void recompiled_entry() {}
    int  recompiled_main(int argc, char** argv) { (void)argc; (void)argv; return 0; }
}

// Tracking HLE + VEH para diagnosticar crashes del guest
extern "C" const char* HLE_GetLastImport();
extern "C" void HLE_SetGuestBase(uint8_t* b);
extern "C" void HLE_SetTlsData(uint32_t guestAddr, uint32_t size);
extern "C" uint32_t HLE_AllocTlsThreadPointer();

#ifdef _WIN32
namespace splatterhouse::ppc_diag {
    struct GuestRegs { const uint64_t *r1, *r2, *r13, *r3, *r4, *r5, *r6, *r7, *r8, *lr; };
}
static std::atomic<void*> g_diagCtx{nullptr};
static std::atomic<uint8_t*> g_diagBase{nullptr};
static LONG WINAPI GuestCrashHandler(EXCEPTION_POINTERS* ep) {
    auto* ctx = (splatterhouse::ppc_diag::GuestRegs*)g_diagCtx.load();
    auto* guestBase = g_diagBase.load();
    if (ctx && ctx->r1) {
        uint64_t lr = *ctx->lr;
        spdlog::critical("Guest crash: code=0x{:08X} addr=0x{:X} lastImport={} | r1=0x{:X} r2=0x{:X} r3=0x{:X} r4=0x{:X} r5=0x{:X} r6=0x{:X} r7=0x{:X} r8=0x{:X} r13=0x{:X} lr=0x{:X}",
                         (uint32_t)ep->ExceptionRecord->ExceptionCode,
                         (uint64_t)ep->ExceptionRecord->ExceptionInformation[1],
                         HLE_GetLastImport() ? HLE_GetLastImport() : "<ninguno>",
                         *ctx->r1, *ctx->r2, *ctx->r3, *ctx->r4, *ctx->r5, *ctx->r6, *ctx->r7, *ctx->r8, *ctx->r13, lr);
        // Mini-trace: instrucciones alrededor de lr y seguir el bl de lr-4
        if (guestBase) {
            auto rd = [&](uint32_t guest) -> uint32_t {
                if (guest < 0x82000000 || guest >= 0xA2000000) return 0;
                uint32_t v = *(uint32_t*)(guestBase + guest);
                return __builtin_bswap32(v);
            };
            uint32_t callSite = (uint32_t)(lr - 4);
            for (int i = 4; i >= 0; --i)
                spdlog::critical("  0x{:08X}: {:08X}", callSite - i*4, rd(callSite - i*4));
            uint32_t insn = rd(callSite);
            if ((insn >> 26) == 18) { // bl/b
                int32_t li = (int32_t)(insn & 0x03FFFFFC);
                if (li & 0x02000000) li |= 0xFC000000;
                uint32_t target = callSite + (uint32_t)li;
                spdlog::critical("  bl target 0x{:08X}:", target);
                for (int i = 0; i < 12; ++i)
                    spdlog::critical("    0x{:08X}: {:08X}", target + i*4, rd(target + i*4));
            }
        }
        // Backtrace host: direcciones relativas al modulo para mapear a funciones recompiladas
        HMODULE mod = nullptr;
        GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           (LPCSTR)&GuestCrashHandler, &mod);
        if (mod) {
            uintptr_t base = (uintptr_t)mod;
            void* frames[24] = {};
            WORD n = RtlCaptureStackBackTrace(1, 24, frames, nullptr);
            for (WORD i = 0; i < n && i < 16; ++i)
                spdlog::critical("  host#{}: +0x{:X}", i, (uintptr_t)frames[i] - base);
        }
    } else {
        spdlog::critical("Guest crash: code=0x{:08X} addr=0x{:X} lastImport={}",
                         (uint32_t)ep->ExceptionRecord->ExceptionCode,
                         (uint64_t)ep->ExceptionRecord->ExceptionInformation[1],
                         HLE_GetLastImport() ? HLE_GetLastImport() : "<ninguno>");
    }
    return EXCEPTION_CONTINUE_SEARCH;
}
#endif

using namespace splatterhouse;

static void SetupLogging(const AppConfig& cfg) {
    auto level = spdlog::level::info;
    switch (cfg.logLevel) {
        case 0: level = spdlog::level::trace; break;
        case 1: level = spdlog::level::debug; break;
        case 2: level = spdlog::level::info; break;
        case 3: level = spdlog::level::warn; break;
        case 4: level = spdlog::level::err; break;
        default: level = spdlog::level::off; break;
    }
    spdlog::set_level(level);
    spdlog::set_pattern("[%H:%M:%S.%e] [%^%l%$] %v");
}

int main(int argc, char* argv[]) {
    (void)argc; (void)argv;

    fs::path cfgPath = GetConfigPath();
    AppConfig cfg = AppConfig::loadOrCreate(cfgPath);
    SetupLogging(cfg);

    SH_LOG_INFO("========================================");
    SH_LOG_INFO(" Splatterhouse Recompiled v0.1.0");
    SH_LOG_INFO(" Xbox 360 Static Recompilation (XenonRecomp)");
    SH_LOG_INFO("========================================");
    SH_LOG_INFO("Config: {}", cfgPath.string());
    SH_LOG_INFO("Game root: {}", cfg.game.gameRoot.string());

    if (!fs::exists(cfg.game.gameRoot)) {
        SH_LOG_WARN("Game root no existe: {} - creandolo...", cfg.game.gameRoot.string());
        fs::create_directories(cfg.game.gameRoot);
        SH_LOG_WARN("Coloca los archivos del juego (extraidos del disco/ISO) en '{}'", cfg.game.gameRoot.string());
        SH_LOG_WARN("Necesitas al menos: default.xex desenriptado o el dump del XEX, y la carpeta de datos.");
    }

    // Inicializa subsistemas HLE
    if (!mem::Initialize()) {
        SH_LOG_ERROR("Fallo al inicializar memoria guest");
        return 1;
    }
#ifdef _WIN32
    AddVectoredExceptionHandler(1, GuestCrashHandler);
#endif
    hle::RegisterAll();

    // SDL
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD | SDL_INIT_EVENTS)) {
        SH_LOG_ERROR("SDL_Init fallo: {}", SDL_GetError());
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow(
        "Splatterhouse (Recompiled)",
        cfg.window.width, cfg.window.height,
        SDL_WINDOW_RESIZABLE | (cfg.window.fullscreen ? SDL_WINDOW_FULLSCREEN : 0)
    );
    if (!window) {
        SH_LOG_ERROR("SDL_CreateWindow fallo: {}", SDL_GetError());
        return 1;
    }

    // GPU / Audio / Input
    void* nativeHandle = window; // SDL3: pasar SDL_Window* al backend que hace SDL_GetWindowWMInfo si necesita HWND
    if (!gpu::Init(nativeHandle, cfg.window.width, cfg.window.height)) {
        SH_LOG_WARN("gpu::Init fallo - continuando sin render (modo headless parcial)");
    }
    if (cfg.audio.enable && !audio::Init(cfg.audio.sampleRate)) {
        SH_LOG_WARN("audio::Init fallo - sin audio");
    }
    input::Init();

    SH_LOG_INFO("Inicializacion completa. Entrando al main loop...");

    // Si hay código recompilado, lanzarlo en un thread guest
    bool hasRecompiled = false;
    // Heurística robusta: busca recompiled/ relativo a cwd, exe y project root (VS lanza desde build/)
    {
        std::vector<fs::path> candidates = {
            fs::path("recompiled"),
            fs::path(argv[0]).parent_path() / "recompiled",
            fs::path(argv[0]).parent_path() / ".." / "recompiled",
            fs::path(argv[0]).parent_path() / ".." / ".." / "recompiled",
            fs::path(argv[0]).parent_path() / ".." / ".." / ".." / "recompiled",
            fs::current_path() / "recompiled",
            fs::current_path() / ".." / "recompiled",
            fs::current_path() / ".." / ".." / "recompiled",
        };
        for (auto &p : candidates) {
            std::error_code ec;
            if (fs::exists(p, ec) && !fs::is_empty(p, ec)) { hasRecompiled = true; SH_LOG_INFO("recompiled/ encontrado en: {}", p.string()); break; }
        }
        if (!hasRecompiled) {
            SH_LOG_DEBUG("hasRecompiled=false, cwd={}, exe={}", fs::current_path().string(), fs::path(argv[0]).string());
        }
    }

    // Lanzar guest thread si hay recompilado
    std::shared_ptr<PPCContext> guestCtx;
    std::unique_ptr<std::thread> guestThread;
    std::atomic<bool> guestRunning{false};

    if (hasRecompiled) {
#ifdef HAS_RECOMPILED
        SH_LOG_INFO("Codigo recompilado detectado - creando PPCContext y lanzando _xstart...");
        guestCtx = std::make_shared<PPCContext>();
        uint32_t guestEntryPoint = 0x82232798; // sobrescrito al parsear el XEX
        
        // Cargar el XEX usando XenonUtils
        // Busca el XEX en varias ubicaciones (gameRoot, exe dir, padres)
        fs::path xexPath;
        {
            std::vector<fs::path> candidates = {
                cfg.game.gameRoot / "default.dec.xex",
                fs::path(argv[0]).parent_path() / "game" / "default.dec.xex",
                fs::path(argv[0]).parent_path() / ".." / "game" / "default.dec.xex",
                fs::path(argv[0]).parent_path() / ".." / ".." / "game" / "default.dec.xex",
                fs::path(argv[0]).parent_path() / ".." / ".." / ".." / "game" / "default.dec.xex",
            };
            for (auto& c : candidates) {
                std::error_code ec;
                if (fs::exists(c, ec)) { xexPath = c; break; }
            }
            if (xexPath.empty()) xexPath = candidates.front();
        }
        SH_LOG_INFO("Buscando XEX en: {}", xexPath.string());
        if (!fs::exists(xexPath)) {
            SH_LOG_ERROR("XEX no encontrado en: {}", xexPath.string());
        } else {
            std::ifstream xexFile(xexPath, std::ios::binary | std::ios::ate);
            if (!xexFile) {
                SH_LOG_ERROR("No se pudo abrir: {}", xexPath.string());
            } else {
                size_t xexSize = xexFile.tellg();
                xexFile.seekg(0, std::ios::beg);
                std::vector<uint8_t> xexData(xexSize);
                xexFile.read(reinterpret_cast<char*>(xexData.data()), xexSize);
                xexFile.close();
                
                SH_LOG_INFO("XEX leído: {} bytes", xexSize);
                
                // Parsear el XEX usando Xex2LoadImage
                Image image = Xex2LoadImage(xexData.data(), xexData.size());
                SH_LOG_INFO("XEX parseado: base=0x{:X} size=0x{:X} entry=0x{:X}", image.base, image.size, image.entry_point);
                guestEntryPoint = (uint32_t)image.entry_point;
                SH_LOG_INFO("image.data.get() = {:p}", (void*)image.data.get());
                SH_LOG_INFO("image.sections.size() = {}", image.sections.size());
                
                // Verificar que image.data sea válido
                if (!image.data) {
                    SH_LOG_ERROR("image.data es null después de Xex2LoadImage");
                } else if (image.size == 0) {
                    SH_LOG_ERROR("image.size es 0");
                } else {
                    // Localizar la seccion .tls (datos iniciales de TLS por thread)
                    for (const auto& s : image.sections) {
                        if (s.name == ".tls") {
                            HLE_SetTlsData((uint32_t)s.base, (uint32_t)s.size);
                            break;
                        }
                    }
                    // Copiar la imagen a la memoria guest sección por sección
                    uint8_t* heap_start = mem::GetBase();
                    SH_LOG_INFO("heap_start={:p}", (void*)heap_start);
                    
                    // Con base = heap_start - image.base, tenemos:
                    // base + guest_addr = heap_start + (guest_addr - image.base)
                    // Así que para copiar una sección en guest_addr, copiamos a heap_start + (guest_addr - image.base)
                    
                    for (const auto& section : image.sections) {
                        // Saltar secciones problemáticas
                        if (section.name == ".reloc") {
                            SH_LOG_INFO("Saltando sección '{}' (problemática)", section.name);
                            continue;
                        }
                        
                        uint32_t sectionOffset = section.base - image.base;
                        // Con GUEST_BASE_ADDR=0, heap_start mapea guest 0:
                        // la seccion va a heap_start + direccion guest completa
                        uint8_t* dest = heap_start + section.base;
                        const uint8_t* src = image.data.get() + sectionOffset;
                        SH_LOG_INFO("Copiando sección '{}' en guest 0x{:X} (offset 0x{:X}), {} bytes", 
                                   section.name, section.base, sectionOffset, section.size);
                        
                        // Verificar que no nos salgamos de los límites
                        if (sectionOffset + section.size > image.size) {
                            SH_LOG_WARN("Sección '{}' se sale de los límites: offset 0x{:X} + size 0x{:X} > image.size 0x{:X}",
                                       section.name, sectionOffset, section.size, image.size);
                            // Copiar solo lo que quepa
                            uint32_t safeSize = image.size - sectionOffset;
                            if (safeSize > 0) {
                                memcpy(dest, src, safeSize);
                            }
                        } else {
                            memcpy(dest, src, section.size);
                        }
                    }
                    
                    SH_LOG_INFO("XEX cargado: {} secciones en heap {:p}", image.sections.size(), (void*)heap_start);
                }
            }
        }
        
        // Stack guest de 2MB en direccion alta, DESPUES de la function table
        // (tabla ocupa 0x832C0000..~0x84D8B030, ver populateFunctionTable)
        constexpr uint32_t kStackGuestBase = 0x85000000;
        constexpr uint32_t kStackSize = 2 * 1024 * 1024;
        uint32_t stackTop = kStackGuestBase + kStackSize - 0x20;
        stackTop &= ~0x1Fu; // alinear 32
        guestCtx->r1.u32 = stackTop;
        SH_LOG_INFO("Stack guest en 0x{:08X} - 0x{:08X}", kStackGuestBase, stackTop);

        uint8_t* heap_start = mem::GetBase();
        uint8_t* stackHost = heap_start + (kStackGuestBase - splatterhouse::mem::GUEST_BASE_ADDR);
        memset(stackHost, 0, kStackSize); // Inicializar stack a 0

        // Poblar la tabla de funciones para PPC_CALL_INDIRECT_FUNC (vtable hash lookup)
        // La tabla vive en guest [IMAGE_BASE+IMAGE_SIZE, +CODE_SIZE*2)
        for (auto* m = PPCFuncMappings; m->host != nullptr; ++m) {
            uint64_t slot = (uint64_t)(heap_start - splatterhouse::mem::GUEST_BASE_ADDR)
                          + PPC_IMAGE_BASE + PPC_IMAGE_SIZE
                          + (uint64_t(uint32_t(m->guest) - PPC_CODE_BASE) * 2);
            *(uint64_t*)slot = (uint64_t)m->host;
        }
        SH_LOG_INFO("Function table poblada ({} entradas)", [&]{
            size_t n = 0; for (auto* m = PPCFuncMappings; m->host != nullptr; ++m) ++n; return n; }());

        // r13 = thread pointer (TEB/TLS). HLE gestiona bloques TLS por thread.
        guestCtx->r13.u32 = HLE_AllocTlsThreadPointer();

        // r2 = TOC: el kernel Xbox lo configura antes de saltar al entry.
        // Detectarlo del entry stub: lis r2, hi ; addi r2, r2, lo
        {
            uint32_t entry = guestEntryPoint;
            uint32_t i0 = mem::ReadBE32(entry);
            uint32_t i1 = mem::ReadBE32(entry + 4);
            if ((i0 >> 26) == 15 && ((i0 >> 21) & 31) == 2 && ((i0 >> 16) & 31) == 0) { // lis r2, hi
                uint32_t toc = (i0 & 0xFFFF) << 16;
                if ((i1 >> 26) == 14 && ((i1 >> 21) & 31) == 2 && ((i1 >> 16) & 31) == 2) { // addi r2, r2, lo
                    toc += (uint32_t)(int32_t)(int16_t)(i1 & 0xFFFF);
                }
                guestCtx->r2.u32 = toc;
                SH_LOG_INFO("TOC detectado del entry stub: 0x{:08X} (insns {:08X} {:08X})", toc, i0, i1);
            } else {
                SH_LOG_WARN("Entry stub sin patron TOC: {:08X} {:08X} (r2=0)", i0, i1);
            }
        }
        guestCtx->r2.u64 = guestCtx->r2.u64 ? guestCtx->r2.u64 : guestCtx->r2.u64;

        uint8_t* base = heap_start - splatterhouse::mem::GUEST_BASE_ADDR;
        HLE_SetGuestBase(base);
        g_diagBase = base;
        // Conectar diag para VEH (r1/r2/r13/r3 en vivo del main thread)
        static splatterhouse::ppc_diag::GuestRegs diagRegs;
        diagRegs.r1 = &guestCtx->r1.u64;
        diagRegs.r2 = &guestCtx->r2.u64;
        diagRegs.r13 = &guestCtx->r13.u64;
        diagRegs.r3 = &guestCtx->r3.u64;
        diagRegs.r4 = &guestCtx->r4.u64;
        diagRegs.r5 = &guestCtx->r5.u64;
        diagRegs.r6 = &guestCtx->r6.u64;
        diagRegs.r7 = &guestCtx->r7.u64;
        diagRegs.r8 = &guestCtx->r8.u64;
        diagRegs.lr = &guestCtx->lr;
        g_diagCtx = &diagRegs;
        SH_LOG_INFO("PPCContext r1=0x{:08X} r2=0x{:08X} r13=0x{:08X} heap={:p} base={:p} -> _xstart",
                    guestCtx->r1.u32, guestCtx->r2.u32, guestCtx->r13.u32, (void*)heap_start, (void*)base);
        guestRunning = true;
        guestThread = std::make_unique<std::thread>([guestCtx, base, &guestRunning]() {
            SH_LOG_INFO("[guest] _xstart(ctx={}, base={}) ...", (void*)guestCtx.get(), (void*)base);
            try {
                _xstart(*guestCtx, base);
                SH_LOG_INFO("[guest] _xstart retorno (juego terminado)");
            } catch (const std::exception& e) {
                SH_LOG_ERROR("[guest] Excepción: {}", e.what());
            } catch (...) {
                SH_LOG_ERROR("[guest] Excepción desconocida");
            }
            guestRunning = false;
        });
        // No detach, esperar a que termine o crashee
        guestThread->join();
        SH_LOG_INFO("Guest thread terminado");
#else
        SH_LOG_WARN("HAS_RECOMPILED no definido - recompila con Clang y verifica recompiled/ppc_context.h");
#endif
    } else {
        SH_LOG_WARN("No hay codigo recompilado. Ejecuta: python tools/recompile.py --xex game/default.xex");
        SH_LOG_WARN("El proyecto compila como bootstrap para validar toolchain.");
    }

    // Main loop host
    bool running = true;
    Uint64 lastCounter = SDL_GetPerformanceCounter();
    double freq = (double)SDL_GetPerformanceFrequency();

    while (running) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_EVENT_QUIT) running = false;
            if (ev.type == SDL_EVENT_WINDOW_RESIZED) {
                gpu::Resize(ev.window.data1, ev.window.data2);
            }
            if (ev.type == SDL_EVENT_KEY_DOWN && ev.key.key == SDLK_ESCAPE) {
                running = false;
            }
            // F11 fullscreen toggle
            if (ev.type == SDL_EVENT_KEY_DOWN && ev.key.key == SDLK_F11) {
                bool isFs = SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN;
                SDL_SetWindowFullscreen(window, !isFs);
            }
        }

        input::Poll();

        // Tick rate / present
        Uint64 now = SDL_GetPerformanceCounter();
        double dt = (now - lastCounter) / freq;
        lastCounter = now;
        (void)dt;

        // TODO: gpu::BeginFrame / Execute command buffer del guest
        gpu::Present();

        // Cap FPS si configurado
        if (cfg.window.maxFps > 0) {
            double targetMs = 1000.0 / cfg.window.maxFps;
            double frameMs = dt * 1000.0;
            if (frameMs < targetMs) {
                SDL_Delay((Uint32)(targetMs - frameMs));
            }
        }

        // VSync ya lo maneja el backend; si no hay present costoso, ceder CPU
        if (!cfg.window.vsync && cfg.window.maxFps == 0) {
            SDL_Delay(1);
        }
    }

    SH_LOG_INFO("Saliendo...");
    input::Shutdown();
    audio::Shutdown();
    gpu::Shutdown();
    SDL_DestroyWindow(window);
    SDL_Quit();
    mem::Shutdown();
    return 0;
}
