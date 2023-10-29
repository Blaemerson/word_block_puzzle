#pragma once
#include "vec.h"
#include "sprite.h"

typedef struct obj_s {
    vec2i pos;
    sprite *sprite;
    vec2i size;
} obj_info_t;
