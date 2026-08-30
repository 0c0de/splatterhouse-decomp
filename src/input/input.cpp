#include "splatterhouse/common.h"
#include <spdlog/spdlog.h>
#include <SDL3/SDL.h>

namespace splatterhouse::input {

static SDL_Gamepad* s_pad = nullptr;

void Init() {
    // SDL3 gamepad init ya hecho en SDL_Init; abrir primer pad
    int count = 0;
    SDL_JoystickID* ids = SDL_GetGamepads(&count);
    if (ids && count > 0) {
        s_pad = SDL_OpenGamepad(ids[0]);
        if (s_pad) spdlog::info("[input] Gamepad abierto: {}", SDL_GetGamepadName(s_pad));
    }
    SDL_free(ids);
    spdlog::info("[input] Init ({} pads)", count);
}

void Shutdown() {
    if (s_pad) { SDL_CloseGamepad(s_pad); s_pad = nullptr; }
}

void Poll() {
    // SDL_PollEvent ya bombea input; aquí podríamos mapear a XInput HLE struct
    // XINPUT_STATE guest en memoria: escribir thumbsticks/buttons
    // SH_TODO("Mapear SDL_Gamepad -> XINPUT_STATE guest (0x82 bytes)");
}

// Helpers para HLE XamInputGetState
bool GetXInputState(int user, void* outState) {
    (void)user; (void)outState;
    if (!s_pad) return false;
    // TODO: llenar XINPUT_STATE BE en guest memory
    return true;
}

} // namespace splatterhouse::input
