#pragma once

#include <stdlib.h>
#include <stddef.h>

#ifdef DEBUG
#define pf(fmt, ...) printf("[Plugin %zu, %zi] " fmt, getpid(), gettid(), ##__VA_ARGS__)
#define pp(fmt, ...) printf(fmt, ##__VA_ARGS__)
#else
#define pf(fmt, ...)
#define pp(fmt, ...)
#endif

typedef unsigned long long u64;
typedef unsigned int u32;
typedef unsigned short u16;
typedef unsigned char u8;
typedef signed long long s64;
typedef signed int s32;
typedef signed short s16;
typedef signed char s8;