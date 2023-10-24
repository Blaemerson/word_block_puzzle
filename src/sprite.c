
#include "../include/sprite.h"
#include "../include/macros.h"
#include <stdio.h>
#include <stdlib.h>

void sprite_push_to_buf(sprite sp, int x, int y, u32 *buf, u32 buf_width, u32 buf_height) {
    for (int i = 0; i < sp.width * sp.height; i++) {
        int screen_x = (i % sp.width) + x;
        int screen_y = (i / sp.width) + y;
        int index = screen_y * buf_width + screen_x;
        buf[index] = sp.pixels[i];
    }
}

sprite sprite_create(usize w, usize h, u32 color) {
    sprite sp;
    ASSERT(w < SPRITE_MAX_W && h < SPRITE_MAX_H, "%zu or %zu exceeds max sprite width %d or height %d", w, h, SPRITE_MAX_W, SPRITE_MAX_H);
    sp.pixels = malloc(sizeof(u32) * w * h);
    LOG("sprite can hold = %lu pixels", (sizeof(u32) * w * h) / 4);
    sp.width = w;
    sp.height = h;
    for (int i = 0; i < w * h; i++) {
        sp.pixels[i] = color;
    }

    return sp;
}

sprite sprite_create_from(usize w, usize h, u32* pixels) {
    sprite sp;
    sp.width = w;
    sp.height = h;
    sp.pixels = malloc(sizeof(u32) * w * h);
    LOG("sprite pixels can hold = %lu", (sizeof(u32) * w * h) / 4);
    for (int i = 0; i < w*h; i++) {
        sp.pixels[i] = pixels[i];
    }

    return sp;
}

void sprite_pixel_color(sprite sp, int x, int y, u32 color) {
    sp.pixels[(x * sp.width) + y] = color;
}
