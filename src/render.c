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

u32 * clone_pixels(u32 const * src, size_t len) {
   u32 *p = malloc(len * sizeof(u32));
   memcpy(p, src, len * sizeof(u32));
   return p;
}

inline u32 greyscale(u32 pix) {
    u16 b = ((pix >> 16) & 0xFF);
    u16 g = ((pix >> 8) & 0xFF);
    u16 r = (pix & 0xFF);
    u16 avg = (b + g + r) / 3;
    return avg + (avg << 8) + (avg << 16);
}
