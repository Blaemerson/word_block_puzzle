
#include "../include/sprite.h"

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
    for (int i = 0; i < w*h; i++) {
        sp.pixels[i] = pixels[i];
    }

    return sp;
}

void sprite_pixel_color(sprite sp, int x, int y, u32 color) {
    sp.pixels[(x * sp.width) + y] = color;
}
