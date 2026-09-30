#pragma once
#include <cstdint>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>
#include <memory>
#include <optional>
#include <span>
#include <filesystem>

namespace fs = std::filesystem;

using u8  = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;
using s8  = int8_t;
using s16 = int16_t;
using s32 = int32_t;
using s64 = int64_t;
using f32 = float;
using f64 = double;

// Endian helpers para PPC (big-endian) <-> x86 (little-endian)
#if defined(_MSC_VER)
#include <stdlib.h>
#define BSWAP16 _byteswap_ushort
#define BSWAP32 _byteswap_ulong
#define BSWAP64 _byteswap_uint64
inline u16 bswap16(u16 v) { return _byteswap_ushort(v); }
inline u32 bswap32(u32 v) { return _byteswap_ulong(v); }
inline u64 bswap64(u64 v) { return _byteswap_uint64(v); }
#else
inline u16 bswap16(u16 v) { return __builtin_bswap16(v); }
inline u32 bswap32(u32 v) { return __builtin_bswap32(v); }
inline u64 bswap64(u64 v) { return __builtin_bswap64(v); }
#endif

// Log macros (spdlog se incluye donde se usa)
#define SH_LOG_TRACE(...) spdlog::trace(__VA_ARGS__)
#define SH_LOG_DEBUG(...) spdlog::debug(__VA_ARGS__)
#define SH_LOG_INFO(...)  spdlog::info(__VA_ARGS__)
#define SH_LOG_WARN(...)  spdlog::warn(__VA_ARGS__)
#define SH_LOG_ERROR(...) spdlog::error(__VA_ARGS__)

#define SH_ASSERT(cond, msg) do { if(!(cond)) { spdlog::error("ASSERT {}:{}: {}", __FILE__, __LINE__, msg); std::abort(); } } while(0)
#define SH_TODO(msg) do { spdlog::warn("TODO {}:{}: {}", __FILE__, __LINE__, msg); } while(0)

#define SH_BE32(v) bswap32(v)
#define SH_BE16(v) bswap16(v)
