#pragma once
#include "../include/types.h"

typedef struct vec2i_s { i32 x, y; } vec2i;
typedef struct vec2u_s { u32 x, y; } vec2u;
typedef struct vec2f_s { f32 x, y; } vec2f;

vec2i vec2_create(int x, int y);

vec2i vector_add(vec2i v0, vec2i v1);

vec2i vector_add_to_all(vec2i v0, int c);

vec2i vector_scale(vec2i v, int s);
