#pragma once

#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>

#define DEBUG

#ifdef DEBUG
#define pf(fmt, ...) fprintf(stderr, "[Plugin] " fmt, ##__VA_ARGS__)
#define pp(fmt, ...) fprintf(stderr, fmt, ##__VA_ARGS__)
#else
#define pf(fmt, ...)
#define pp(fmt, ...)
#endif

#ifdef __GNUC__
#define likely(x)       __builtin_expect(!!(x), 1)
#define unlikely(x)     __builtin_expect(!!(x), 0)
#else
#define likely(x)       (x)
#define unlikely(x)     (x)
#endif

typedef __uint32_t u32;
typedef __uint16_t u16;
typedef __uint8_t u8;
typedef __uint64_t u64;