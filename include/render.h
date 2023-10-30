#pragma once

#include <SDL2/SDL_render.h>
#include "types.h"
#include "obj.h"

void obj_render(obj_info_t *obj, SDL_Texture *tex);
void sprite_render(int x, int y, sprite *s, SDL_Texture *tex);
void pix_buf_render(int x, int y, int w, int h, u32 *pixels, SDL_Texture *tex);

void verline(int x, int y0, int y1, u32 color, u32* pixels, int pix_buf_width);

u32* line(int length, u32 color);

void horiline(int x, int y0, int y1, u32 color, u32* pixels, int pix_buf_width);

u32 *clone_pixels(u32 const * src, size_t len);

u32 greyscale(u32 pix);
u32 darken(u32 pix);
u32 lighten(u32 pix);
