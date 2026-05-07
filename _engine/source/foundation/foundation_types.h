#pragma once
#include <cstdint>
#include <cstddef>
#include <cstring>

// ============================================================
// Entelechy Foundation Types
// Zero-dependency header: scalar aliases, platform macros,
// assert tiers, and symbol export helpers.
// ============================================================

// ---------- 1. Fixed-width scalars ----------
using u8  = uint8_t;   using i8  = int8_t;
using u16 = uint16_t;  using i16 = int16_t;
using u32 = uint32_t;  using i32 = int32_t;
using u64 = uint64_t;  using i64 = int64_t;
using f32 = float;     using f64 = double;

using usize = std::size_t;
using isize = std::ptrdiff_t;

// ---------- 2. Platform detection (zero-first + #if) ----------
#define PLATFORM_WINDOWS 0
#define PLATFORM_LINUX   0
#define PLATFORM_MACOS   0
#define PLATFORM_ANDROID 0
#define PLATFORM_IOS     0
#define PLATFORM_UNKNOWN 0

#if defined(_WIN32)
    #undef PLATFORM_WINDOWS
    #define PLATFORM_WINDOWS 1
#elif defined(__linux__) && !defined(__ANDROID__)
    #undef PLATFORM_LINUX
    #define PLATFORM_LINUX 1
#elif defined(__APPLE__)
    #include <TargetConditionals.h>
    #if TARGET_OS_IPHONE
        #undef PLATFORM_IOS
        #define PLATFORM_IOS 1
    #else
        #undef PLATFORM_MACOS
        #define PLATFORM_MACOS 1
    #endif
#elif defined(__ANDROID__)
    #undef PLATFORM_ANDROID
    #define PLATFORM_ANDROID 1
#else
    #undef PLATFORM_UNKNOWN
    #define PLATFORM_UNKNOWN 1
#endif

#if defined(_M_X64) || defined(__x86_64__) || defined(__aarch64__)
    #define ARCH_64BIT 1
    #define ARCH_32BIT 0
#else
    #define ARCH_64BIT 0
    #define ARCH_32BIT 1
#endif

// ---------- 2.5 Architecture SIMD families ----------
#define ARCH_X86   0
#define ARCH_ARM   0
#define ARCH_WASM  0
#define ARCH_SCALAR 0

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
    #undef ARCH_X86
    #define ARCH_X86 1
#elif defined(__aarch64__) || defined(_M_ARM64) || defined(__arm__) || defined(_M_ARM)
    #undef ARCH_ARM
    #define ARCH_ARM 1
#elif defined(__wasm__) || defined(__wasm32__) || defined(__wasm64__)
    #undef ARCH_WASM
    #define ARCH_WASM 1
#else
    #undef ARCH_SCALAR
    #define ARCH_SCALAR 1
#endif

// ---------- 3. Symbol export macro ----------
#if defined(ENGINE_BUILD_DLL)
    #if PLATFORM_WINDOWS
        #define ENGINE_API __declspec(dllexport)
    #else
        #define ENGINE_API __attribute__((visibility("default")))
    #endif
#elif defined(ENGINE_USE_DLL)
    #if PLATFORM_WINDOWS
        #define ENGINE_API __declspec(dllimport)
    #else
        #define ENGINE_API __attribute__((visibility("default")))
    #endif
#else
    #define ENGINE_API // static link — no decoration
#endif

// ---------- 4. Assert tiers ----------
#if defined(_DEBUG) || defined(DEBUG)
    #include <cstdlib>
    #include <cstdio>

    #if defined(_MSC_VER)
        #define DEBUG_BREAK() __debugbreak()
    #else
        #define DEBUG_BREAK() __builtin_trap()
    #endif

    #define CHECK_IMPL(cond, expr_str, file, line) \
        do { if (!(cond)) { \
            std::fprintf(stderr, "[CHECK FAILED] %s at %s:%d\n", expr_str, file, line); \
            DEBUG_BREAK(); \
            std::abort(); \
        } } while(0)

    // CHECK: internal invariant, stripped in Release (zero cost)
    #define CHECK(cond) CHECK_IMPL(cond, #cond, __FILE__, __LINE__)

    // VERIFY: expression always executes, failure handling is Debug-only
    #define VERIFY(cond) CHECK_IMPL(cond, #cond, __FILE__, __LINE__)

    // ENSURE: log error and continue (non-fatal)
    #define ENSURE_MSG(cond, fmt, ...) \
        do { if (!(cond)) { \
            std::fprintf(stderr, "[ENSURE] " fmt " at %s:%d\n" __VA_OPT__(,) __VA_ARGS__, __FILE__, __LINE__); \
        } } while(0)
    #define ENSURE(cond) ENSURE_MSG(cond, "%s", #cond)
#else
    #define CHECK(cond)      ((void)0)
    #define VERIFY(cond)     ((void)(cond)) // expression must execute
    #define ENSURE(cond)     ((void)0)
    #define ENSURE_MSG(...)  ((void)0)
#endif

#define STATIC_ASSERT(cond, msg) static_assert(cond, msg)

// ---------- 5. Debug fill (use-after-free / overrun detection) ----------
#if defined(_DEBUG) || defined(DEBUG)
    constexpr u8 CANARY_ALLOC = 0xCD;
    constexpr u8 CANARY_FREE  = 0xFE;
    inline void debugFill(void* ptr, usize size, u8 value) {
        std::memset(ptr, static_cast<int>(value), size);
    }
#else
    inline void debugFill(void*, usize, u8) {}
#endif
