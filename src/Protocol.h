#pragma once
#include <stdint.h>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <time.h>
#include <unistd.h>
#endif

// Fixed layout shared by the C engine process and C++ FFGL bundle.
// Atomic builtins avoid a dependency on C/C++ atomic type layout.
#define MD_WIDTH 320
#define MD_HEIGHT 200
#define MD_PIXELS (MD_WIDTH * MD_HEIGHT)
#define MD_MAGIC 0x4d444d31u
#define MD_KEYS 23
typedef struct {
    uint32_t state; // 0 free, 1 writer owns, 2 published, 3 reader owns
    uint32_t sequence;
    uint32_t pixels[MD_PIXELS];
} MDFrame;
typedef struct {
    uint32_t magic;
    uint32_t keys;
    uint32_t paused;
    uint32_t quit;
    uint64_t heartbeat;
    MDFrame frames[3];
} MDShared;
static inline uint64_t md_now(void) {
#ifdef _WIN32
    LARGE_INTEGER count, frequency;
    QueryPerformanceCounter(&count); QueryPerformanceFrequency(&frequency);
    return (uint64_t)(count.QuadPart / frequency.QuadPart) * 1000000000ull
        + (uint64_t)(count.QuadPart % frequency.QuadPart) * 1000000000ull / frequency.QuadPart;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + ts.tv_nsec;
#endif
}
static inline uint32_t md_load32(const uint32_t *p) {
#ifdef _WIN32
    return (uint32_t)InterlockedCompareExchange((volatile LONG*)p,0,0);
#else
    return __atomic_load_n(p,__ATOMIC_ACQUIRE);
#endif
}
static inline void md_store32(uint32_t *p,uint32_t v) {
#ifdef _WIN32
    InterlockedExchange((volatile LONG*)p,(LONG)v);
#else
    __atomic_store_n(p,v,__ATOMIC_RELEASE);
#endif
}
static inline uint64_t md_load64(const uint64_t *p) {
#ifdef _WIN32
    return (uint64_t)InterlockedCompareExchange64((volatile LONG64*)p,0,0);
#else
    return __atomic_load_n(p,__ATOMIC_ACQUIRE);
#endif
}
static inline void md_store64(uint64_t *p,uint64_t v) {
#ifdef _WIN32
    InterlockedExchange64((volatile LONG64*)p,(LONG64)v);
#else
    __atomic_store_n(p,v,__ATOMIC_RELEASE);
#endif
}
static inline int md_claim(uint32_t *p,uint32_t from,uint32_t to) {
#ifdef _WIN32
    return (uint32_t)InterlockedCompareExchange((volatile LONG*)p,(LONG)to,(LONG)from)==from;
#else
    return __atomic_compare_exchange_n(p,&from,to,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE);
#endif
}
static inline void md_sleep(uint32_t ms) {
#ifdef _WIN32
    Sleep(ms);
#else
    usleep(ms*1000);
#endif
}
