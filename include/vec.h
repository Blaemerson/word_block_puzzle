#pragma once
#include "../include/types.h"

vec2i vec2_create(int x, int y);

vec2i vector_add(vec2i v0, vec2i v1);

vec2i vector_add_to_all(vec2i v0, int c);

vec2i vector_scale(vec2i v, int s);
