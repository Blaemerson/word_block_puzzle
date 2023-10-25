#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef float f32;
typedef double f64;
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;
typedef size_t usize;

typedef struct {
    usize width, height;
    u32 *pixels;
} sprite;

typedef struct vec2i_s { i32 x, y; } vec2i;
typedef struct vec2u_s { u32 x, y; } vec2u;
typedef struct vec2f_s { f32 x, y; } vec2f;

typedef struct obj_s {
    vec2i pos;
    sprite *sprite;
    vec2i size;
} obj_info_t;

typedef struct tile {
    u8 letter;
    bool filled;
    bool marked;
    vec2i pos;

    obj_info_t obj;
} tile_t;

