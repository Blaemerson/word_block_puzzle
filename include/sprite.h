#pragma once
#include "types.h"

typedef struct {
    usize width, height;
    u32 pixels[64 * 64];
} sprite;

void sprite_push_to_buf(sprite sp, int x, int y, u32 *buf, u32 buf_width, u32 buf_height);

sprite sprite_create(usize w, usize h, u32 color);

sprite sprite_create_from(usize w, usize h, u32* pixels);

void sprite_pixel_color(sprite sp, int x, int y, u32 color);
