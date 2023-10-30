#include "../include/render.h"
#include "../include/macros.h"

void obj_render(obj_info_t *obj, SDL_Texture *tex) {
    SDL_UpdateTexture(tex,
                      &(SDL_Rect){.x=obj->pos.x, .y=obj->pos.y, .w=obj->size.x, .h=obj->size.y},
                      obj->sprite->pixels,
                      obj->size.x * 4);
}

void sprite_render(int x, int y, sprite *s, SDL_Texture *tex) {
    SDL_UpdateTexture(tex,
                      &(SDL_Rect){.x=x, .y=y, .w=s->width, .h=s->height},
                      s->pixels,
                      s->width * 4);
}

// update tex with pixels at pos x, y
void pix_buf_render(int x, int y, int w, int h, u32 *pixels, SDL_Texture *tex) {
    SDL_UpdateTexture(tex,
                      &(SDL_Rect){.x=x, .y=y, .w=w, .h=h},
                      pixels,
                      w * 4);
}

void verline(int x, int y0, int y1, u32 color, u32* pixels, int pix_buf_width) {
    for (int y = y0; y <= y1; y++)
        pixels[(y * pix_buf_width) + x] = color;
}

u32* line(int length, u32 color) {
    u32 *pixels = malloc(sizeof(u32) * length);

    for (int i = 0; i < length; i++) {
        pixels[i] = color;
    }

    return pixels;
}

void horiline(int x0, int x1, int y, u32 color, u32* pixels, int pix_buf_width) {
    for (int x = x0; x <= x1; x++)
        pixels[(y * pix_buf_width) + x] = color;
}

u32 *clone_pixels(u32 const * src, size_t len) {
   u32 *p = malloc(len * (sizeof *p));
   memcpy(p, src, len * (sizeof *p));
   return p;
}

inline u32 greyscale(u32 pix) {
    u8 a = (pix >> 24) & 0xFF;
    u8 b = (pix >> 16) & 0xFF;
    u8 g = (pix >> 8) & 0xFF;
    u8 r = pix & 0xFF;
    u8 avg = (b + g + r) / 3;
    return (a << 24) + (avg << 16) + (avg << 8) + avg;
}

inline u32 darken(u32 pix) {
    return (pix & 0x00fefefe) >> 1;
}

inline u32 lighten(u32 pix) {
    // Extract the alpha, blue, green, and red components
    u8 alpha = (pix >> 24) & 0xFF;
    u8 blue = (pix >> 16) & 0xFF;
    u8 green = (pix >> 8) & 0xFF;
    u8 red = pix & 0xFF;

    // Lighten the color by adding a fraction of the maximum value
    red = (u8) ((255 - red) * 0.3 + red);
    green = (u8) ((255 - green) * 0.4 + green);
    blue = (u8) ((255 - blue) * 0.7 + blue);

    // Combine the components and return the new color
    return ((u32)alpha << 24) | ((u32)blue << 16) | ((u32)green << 8) | (u32)red;
}
