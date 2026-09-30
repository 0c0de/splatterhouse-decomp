#pragma once
#include "splatterhouse/common.h"

namespace splatterhouse::mem {

// Memoria virtual del guest (PPC) emulada en host.
// El heap host (g_base) mapea la dirección guest GUEST_BASE_ADDR.
// Todo lo que este por debajo (XEX a 0x82000000) mapea al inicio del heap.

// Dirección guest mínima que mapea al inicio del heap host
// Mapeamos TODO el espacio guest usable (0..0xA2000000) para que las escrituras
// a traves de punteros no inicializados caigan en RAM cero en vez de crashear.
constexpr u32 GUEST_BASE_ADDR = 0x00000000;
// Tamaño de la región guest mapeada (0x00000000..0xA2000000, ~2.5GB commit,
// el SO asigna páginas físicas solo bajo demanda)
constexpr u32 GUEST_MEM_SIZE  = 0xA2000000;
// El bump allocator empieza tras el XEX (~0x832C0000), function table (~0x84D8B030)
// y stack guest (0x85000000-0x85200000)
constexpr u32 HEAP_BASE       = 0x85200000;
constexpr u32 STACK_SIZE      = 0x00200000; // 2 MB por thread guest

// Inicializa heap guest (VirtualAlloc / mmap)
bool Initialize();
void Shutdown();

// Traducción de direcciones guest -> host pointer
// Retorna nullptr si out of bounds.
void*  Translate(u32 guestAddr);
const void* TranslateConst(u32 guestAddr);

// Puntero host al inicio del heap (mapea la dirección guest GUEST_BASE_ADDR)
uint8_t* GetBase();

// Lectura/escritura con byteswap automático para BE
u32  ReadBE32(u32 guestAddr);
void WriteBE32(u32 guestAddr, u32 value);
u64  ReadBE64(u32 guestAddr);
void WriteBE64(u32 guestAddr, u64 value);

// Allocadores simples (bump + free list para HLE)
u32  Alloc(u32 size, u32 align = 16);
void Free(u32 guestAddr);

// Helpers de strings guest (char* big-endian / utf16)
std::string ReadString(u32 guestAddr);
std::string ReadString(u32 guestAddr, size_t maxLen);

} // namespace splatterhouse::mem
