#pragma once
#include "obj.h"
#include "types.h"
#include "vec.h"

typedef struct tile {
    u8 letter;
    bool filled;
    bool greyed;
    bool marked;
    vec2i pos;

    enum {
        CON_NONE = 0,
        CON_UP = 1,
        CON_DOWN = 2,
        CON_LEFT = 3,
        CON_RIGHT = 4,
    } connected;

    obj_info_t obj;
} tile_t;

