#include <stdio.h>
#include <stdbool.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include "../include/types.h"
#include "../include/vec.h"
#include "../include/sprite.h"
#include <time.h>

#define ASSERT(_e, ...) if (!(_e)) { fprintf(stderr, "[ERROR] %s %d: ", __FILE__, __LINE__); fprintf(stderr, __VA_ARGS__);  exit(1); }
#define LOG(...) do { fprintf(stderr, "[LOG] %s %d: ", __FILE__, __LINE__); fprintf(stderr, __VA_ARGS__); printf("\n"); } while (0)

#define SCREEN_WIDTH 1080
#define SCREEN_HEIGHT 720

struct {
    SDL_Window *window;
    SDL_Texture *texture;
    SDL_Renderer *renderer;
    vec2i mouse_pos;
    u32 pixels[SCREEN_WIDTH * SCREEN_HEIGHT];
    bool quit, halt, paused, begin, gameover;
    double time;
    double tick;
} state;

typedef struct tile {
    u8 letter;
    sprite *sprite;
    bool filled;
    bool marked;
    u32 size;
    vec2i pos;
} tile_t;

struct {
    bool active;
    tile_t t1, t2;
} player;

#define TILE_SIZE 64
#define GAMEBOARD_WIDTH 8
#define GAMEBOARD_HEIGHT 10
#define GAMEBOARD_MAX (GAMEBOARD_WIDTH * GAMEBOARD_HEIGHT)
#define GAMEBOARD_OFFSET_X (SCREEN_WIDTH / 2) - ((GAMEBOARD_WIDTH * TILE_SIZE) / 2)

struct {
    vec2i offset;

    uint h, w;
    sprite *sprite;
    tile_t tiles[GAMEBOARD_MAX];
} grid;

struct {
    vec2i offset;
    sprite *sprite;
    tile_t tiles[2];
} queue;

static void write_pixel_block(int x, int y, int w, int h, u32 *pixels) {
    SDL_UpdateTexture(state.texture, &(SDL_Rect){.x=x, .y=y, .w=w, .h=h}, pixels, w * 4);
}

static void draw_queue() {
    u32 x = queue.offset.x;
    u32 y = queue.offset.y;
    u32 size = queue.tiles[0].size;
    write_pixel_block(x, y, size, size, queue.tiles[0].sprite->pixels);
    write_pixel_block(x + size, y, size, size, queue.tiles[1].sprite->pixels);
}


static int pix_pos_to_grid_index(vec2i world_pos) {
    vec2i grid_pos = {.x = ((world_pos.x - grid.offset.x) / TILE_SIZE), .y = (world_pos.y - grid.offset.y) / TILE_SIZE};
    return (grid_pos.y * grid.w) + grid_pos.x;
}

static void grid_debug_print() {
    for (int i = 0; i < GAMEBOARD_WIDTH * GAMEBOARD_HEIGHT; i++) {
        printf("%d", grid.tiles[i].filled);
        if ((i + 1) % GAMEBOARD_WIDTH == 0)
            printf("\n");
    }
}

static void clear_grid() {
    for (int i = 0; i < grid.w * grid.h; i++)
        grid.tiles[i] = (tile_t){};
}


/*
 
 DRAWING

*/
static void clear_scr() {
    for (int i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT; i++) {
        state.pixels[i] = 0x555555FF;
    }
}

static void verline(int x, int y0, int y1, u32 color) {
    for (int y = y0; y <= y1; y++)
        state.pixels[(y * SCREEN_WIDTH) + x] = color;
}


static void horiline(int x0, int x1, int y, u32 color) {
    for (int x = x0; x <= x1; x++)
        state.pixels[(y * SCREEN_WIDTH) + x] = color;
}
static void draw_rect(vec2i pos, uint w, uint h, u32 color, uint border_thickness) {
    for (int i = border_thickness; i < w - (border_thickness - 1); i++)
        verline(pos.x + i, pos.y + border_thickness, pos.y + h - border_thickness, color);
}

static void draw_square(vec2i pos, uint size, u32 color, uint border_thickness) {
    draw_rect(pos, size, size, color, border_thickness);
}

static u32 * clone_pixels(u32 const * src, size_t len) {
   u32 * p = malloc(len * sizeof(u32));
   memcpy(p, src, len * sizeof(u32));
   return p;
}

static void draw_tile(tile_t t) {
    u32 x = t.pos.x * t.size + grid.offset.x;
    u32 y = t.pos.y * t.size + grid.offset.y;
    u32 *pixels = clone_pixels(t.sprite->pixels, t.sprite->width * t.sprite->height);
    if (t.marked) {
        for (int i = 0; i < t.sprite->width * t.sprite->height; i++) {
            pixels[i] |= 0xFF1122FF;
        }
    }
    write_pixel_block(x, y, t.size, t.size, pixels);
}

static void draw_tiles() {
    for (int i = 0; i < GAMEBOARD_MAX; i++) {
        if (grid.tiles[i].filled)
            draw_tile(grid.tiles[i]);
    }
}

static void draw_bg() {
    u32 bg_color = 0xCCCCDDFF;
    for (int i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT; i++) {
        state.pixels[i] = bg_color;
    }
}

static void draw_grid() {
    u32 line_color = 0xBBBBBBBB;

    for (int i = 0; i < (grid.w * TILE_SIZE) + 1; i += TILE_SIZE) {
        verline(i + grid.offset.x, grid.offset.y, grid.h * TILE_SIZE + grid.offset.y, line_color);
    }
    for (int i = grid.offset.y; i < (grid.h * TILE_SIZE) + 1 + grid.offset.y; i += TILE_SIZE) {
        horiline(grid.offset.x, (grid.w * TILE_SIZE) + grid.offset.x, i, line_color);
    }
}



static void flip_player() {
    tile_t t1 = player.t1;
    player.t1.filled = player.t2.filled;
    player.t1.letter = player.t2.letter;
    player.t1.sprite = player.t2.sprite;

    player.t2.filled = t1.filled;
    player.t2.letter = t1.letter;
    player.t2.sprite = t1.sprite;

}

static void draw_player() {
    draw_tile(player.t1);
    draw_tile(player.t2);
}

static void set_player() {
    grid.tiles[(player.t1.pos.y * GAMEBOARD_WIDTH) + player.t1.pos.x] = player.t1;
    grid.tiles[(player.t2.pos.y * GAMEBOARD_WIDTH) + player.t2.pos.x] = player.t2;
}

static tile_t new_tile(bool filled, char letter, vec2i pos) {
    return (tile_t){.filled=filled, .letter=letter, .pos=pos, .size=64};
}

static void render() {
    // clear_scr();
    draw_bg();
    draw_grid();

    SDL_UpdateTexture(state.texture, NULL, state.pixels, SCREEN_WIDTH * 4);

    draw_tiles();
    draw_queue();
    if (player.active) draw_player();

    SDL_RenderCopyEx(state.renderer, state.texture, NULL, NULL, 0.0, NULL, SDL_FLIP_NONE);
    SDL_RenderPresent(state.renderer);
}

static bool check_tile_in_grid(tile_t t, vec2i move) {
    vec2i dest = vector_add(t.pos, move);
    return (dest.x < grid.w &&
            dest.x >= 0 &&
            dest.y < grid.h &&
            dest.y >= 0);
}

static bool check_player_in_grid(vec2i move) {
    return check_tile_in_grid(player.t1, move) && check_tile_in_grid(player.t2, move);
}

static bool check_tile_move(tile_t t, vec2i move) {

    int g_idx = ((t.pos.y + move.y) * grid.w) + (t.pos.x + move.x);
    if (t.filled && !(grid.tiles[g_idx].filled)) {
        return true;
    }
    return false;
}

static bool check_player_move(vec2i move) {
    return check_tile_move(player.t1, move) && check_tile_move(player.t2, move);
}

static void move_tile(tile_t *t, vec2i move) {
    ASSERT(check_tile_move(*t, move), "couldn't move tile");

    int old_idx = (t->pos.y * grid.w) + t->pos.x;
    t->pos = vector_add(t->pos, move);
    grid.tiles[(t->pos.y * grid.w) + t->pos.x] = *t;
    grid.tiles[old_idx] = (tile_t){};
}

static void move_player(vec2i move) {
    LOG("move_player: t1.pos=(%d,%d), t2.pos=(%d,%d)", player.t1.pos.x, player.t1.pos.y, player.t2.pos.x, player.t2.pos.y);
    player.t1.pos = vector_add(player.t1.pos, move);
    player.t2.pos = vector_add(player.t2.pos, move);
}


static void clear_player() {
    player.t1 = (tile_t){};
    player.t2 = (tile_t){};
}

static void grid_scan_hori() {
    for (int i = 1; i < grid.w * grid.h; i++) {
    }
}

static void grid_clear_marked() {
    for (int i = 0; i < grid.w * grid.h; i++) {
        if (grid.tiles[i].marked) {
            grid.tiles[i] = (tile_t){};
        }
    }
}

static void grid_scan() {
    state.halt = true;
    grid_scan_hori();
    grid_clear_marked();
}

u32 *load_img_pixels(const char *file)
{
    SDL_Surface *surface = IMG_Load(file);
    u32 *pixels;
    if (surface) {
        pixels = surface->pixels;
        SDL_FreeSurface(surface);
    }
    return pixels;
}


static char gen_random_letter() {
    return 65 + (rand() % 26);
}

static const char* letter_textures[] = {
    "gfx/TileA.png",
    "gfx/TileB.png",
    "gfx/TileC.png",
    "gfx/TileD.png",
    "gfx/TileE.png",
    "gfx/TileF.png",
    "gfx/TileG.png",
    "gfx/TileH.png",
    "gfx/TileI.png",
    "gfx/TileJ.png",
    "gfx/TileK.png",
    "gfx/TileL.png",
    "gfx/TileM.png",
    "gfx/TileN.png",
    "gfx/TileO.png",
    "gfx/TileP.png",
    "gfx/TileQ.png",
    "gfx/TileR.png",
    "gfx/TileS.png",
    "gfx/TileT.png",
    "gfx/TileU.png",
    "gfx/TileV.png",
    "gfx/TileW.png",
    "gfx/TileX.png",
    "gfx/TileY.png",
    "gfx/TileZ.png",
};

static sprite sprites[26];

static void load_sprites() {
    for (int i = 0; i < 26; i++) {
        sprites[i] = sprite_create_from(64, 64, load_img_pixels(letter_textures[i]));
    }
}

static void queue_next() {
    vec2i t1_pos = (vec2i){(grid.w / 2) - 1, 0};
    vec2i t2_pos = (vec2i){(grid.w / 2), 0};

    char t1_letter = gen_random_letter();
    char t2_letter = gen_random_letter();
    LOG("queue_next: t1.letter=%c t2.letter=%c", t1_letter, t2_letter);

    queue.tiles[0] = new_tile(true, t1_letter, t1_pos);
    queue.tiles[1] = new_tile(true, t2_letter, t2_pos);

    queue.tiles[0].sprite = &sprites[t1_letter - 'A'];
    queue.tiles[1].sprite = &sprites[t2_letter - 'A'];
}

static void spawn_player() {
    player.t1 = queue.tiles[0];
    player.t2 = queue.tiles[1];

    player.active = true;
}

static bool update_falling() {
    bool fall = false;

    vec2i move = {0, 1};
    for (int i = ((grid.w * grid.h) - grid.w) - 1; i > -1; i--) {
        if (check_tile_move(grid.tiles[i], move)) {
            move_tile(&grid.tiles[i], move);
            fall = true;
        }
    }

    return fall;
}

static void update_world_physics() {
    if (!update_falling()) {
        state.halt = false;
        spawn_player();
        queue_next();
    }
}

void stop_player() {
    set_player();
    state.halt = true;
    clear_player();

    player.active = false;
    grid_scan();
}

static void update_player_physics() {
    vec2i fall = {0, 1};
    if (check_player_in_grid(fall) && check_player_move(fall)) {
        move_player(fall);
    } else {
        stop_player();
    }
}

static void update_physics() {
    if (state.halt) {
        update_world_physics();
    } else {
        update_player_physics();
    }
}

static void update_tick() {
    if (state.halt)
        state.tick = 250;
    else
        state.tick = 1000;

    if (SDL_GetTicks() >= state.time + state.tick) {
        state.time = SDL_GetTicks();
        update_physics();
    }
}

static bool player_check_movement(vec2i move) {
    if (check_player_in_grid(move) && check_player_move(move))
        return true;
    return false;
}

void tile_set_marked(tile_t *t, bool marked) {
    t->marked = marked;
}

static void handle_input() {
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        switch (ev.type) {
            case SDL_QUIT:
                state.quit = true;
                break;
            case SDL_MOUSEBUTTONDOWN: {
                int tile_index = pix_pos_to_grid_index(state.mouse_pos);
                tile_t t = grid.tiles[tile_index];
                LOG("tile %d info: {.letter='%c', .pos=(%d, %d), .filled=%d, .marked=%d }", tile_index, t.letter, t.pos.x, t.pos.y, t.filled, t.marked);
                tile_set_marked(&grid.tiles[tile_index], true);
                break;
            }
            case SDL_MOUSEMOTION:
                SDL_GetMouseState(&state.mouse_pos.x, &state.mouse_pos.y);
                break;
            case SDL_KEYDOWN:
                /* Check the SDLKey values and move change the coords */
                switch ( ev.key.keysym.sym ) {
                    case SDLK_ESCAPE: {
                        state.quit = true;
                    }
                    case SDLK_a:
                    case SDLK_LEFT: {
                        vec2i move = (vec2i){-1, 0};
                        if (player_check_movement(move))
                            move_player(move);
                        break;
                    }
                    case SDLK_d:
                    case SDLK_RIGHT: {
                        vec2i move = (vec2i){1, 0};
                        if (player_check_movement(move))
                            move_player(move);
                        break;
                    }
                    case SDLK_k:
                    case SDLK_e: {
                        int x1 = player.t1.pos.x;
                        int x2 = player.t2.pos.x;
                        int y1 = player.t1.pos.y;
                        int y2 = player.t2.pos.y;

                        int move_x, move_y;

                        if (x1 <= x2 && y1 == y2) {
                            move_x = 1;
                            move_y = -1;
                        }
                        else if (x1 == x2 && y1 < y2) {
                            move_x = 1;
                            move_y = 1;
                        }
                        else if (x1 >= x2 && y1 == y2) {
                            move_x = -1;
                            move_y = 1;
                        } else {
                            move_x = -1;
                            move_y = -1;
                        }

                        vec2i move = (vec2i){move_x, move_y};
                        if (check_tile_move(player.t1, move) && check_tile_in_grid(player.t1, move))
                            player.t1.pos = vector_add(player.t1.pos, move);
                        break;
                    }
                    case SDLK_j:
                    case SDLK_q: {

                        int x2 = player.t1.pos.x;
                        int x1 = player.t2.pos.x;
                        int y2 = player.t1.pos.y;
                        int y1 = player.t2.pos.y;

                        int move_x, move_y;

                        if (x1 <= x2 && y1 == y2) {
                            move_x = 1;
                            move_y = 1;
                        }
                        else if (x1 == x2 && y1 < y2) {
                            move_x = -1;
                            move_y = 1;
                        }
                        else if (x1 >= x2 && y1 == y2) {
                            move_x = -1;
                            move_y = -1;
                        } else {
                            move_x = 1;
                            move_y = -1;
                        }

                        vec2i move = (vec2i){move_x, move_y};
                        if (check_tile_move(player.t2, move) && check_tile_in_grid(player.t2, move))
                            player.t2.pos = vector_add(player.t2.pos, move);
                        break;
                    }
                    case SDLK_s:
                    case SDLK_DOWN: {
                        vec2i move = (vec2i){0, 1};
                        if (player_check_movement(move)) {
                            state.time = SDL_GetTicks();
                            move_player(move);
                        }
                        else {
                            stop_player();
                        }
                        break;
                    }
                    case SDLK_w:
                    case SDLK_UP:
                        flip_player();
                        break;
                    case SDLK_SPACE:
                        clear_grid();
                        break;
                    default:
                        break;
                }
                break;
        }
    }
}

int main(int argc, char *argv[]) {
    ASSERT(!SDL_Init(SDL_INIT_VIDEO), "SDL failed to initialize: %s\n", SDL_GetError());

    state.window = SDL_CreateWindow("Wordtris", SDL_WINDOWPOS_CENTERED_DISPLAY(1), SDL_WINDOWPOS_CENTERED_DISPLAY(1), SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_ALLOW_HIGHDPI);
    ASSERT(state.window, "Failed to create SDL Window: %s\n", SDL_GetError());

    state.renderer = SDL_CreateRenderer(state.window, -1, SDL_RENDERER_PRESENTVSYNC);
    ASSERT(state.renderer, "Failed to create SDL Renderer: %s\n", SDL_GetError());

    state.texture = SDL_CreateTexture(state.renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING, SCREEN_WIDTH, SCREEN_HEIGHT);
    ASSERT(state.renderer, "Failed to create SDL Texture: %s\n", SDL_GetError());

    SDL_SetRenderDrawBlendMode(state.renderer, SDL_BLENDMODE_BLEND);

    srand(time(NULL));

    grid.w = 8;
    grid.h = 10;
    grid.offset = vec2_create((SCREEN_WIDTH / 2) - ((grid.w * TILE_SIZE) / 2), TILE_SIZE / 2);

    clear_grid();

    load_sprites();

    queue.offset = vec2_create(SCREEN_WIDTH - (3 * TILE_SIZE), TILE_SIZE * 1.5);

    queue_next();
    spawn_player();
    queue_next();

    state.tick = 1000.0;
    state.time = SDL_GetTicks();

    while (!state.quit) {

        update_tick();

        handle_input();

        render();
    }

    SDL_DestroyTexture(state.texture);

    return 0;
}
