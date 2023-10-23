#include <stdio.h>
#include <stdbool.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include "../include/types.h"
#include "../include/vec.h"
#include "../include/sprite.h"
#include <time.h>
#include "../include/macros.h"

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

typedef struct obj_s {
    vec2i pos;
    sprite *sprite;
    vec2i size;
} obj_t;

typedef struct tile {
    u8 letter;
    bool filled;
    bool marked;
    vec2i pos;

    obj_t obj;
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
    uint height, width;
    tile_t tiles[GAMEBOARD_MAX];

    obj_t obj;
} grid;

struct {
    tile_t tiles[2];

    obj_t obj;
} queue;

static void write_pixel_block(int x, int y, sprite *s) {
    SDL_UpdateTexture(state.texture, &(SDL_Rect){.x=x, .y=y, .w=s->width, .h=s->height}, s->pixels, s->width * 4);
}

static void queue_render() {
    u32 x = queue.obj.pos.x;
    u32 y = queue.obj.pos.y;


    write_pixel_block(x, y, queue.obj.sprite);

    write_pixel_block(x + 32, y + 32, queue.tiles[0].obj.sprite);
    write_pixel_block(x + 64 + 32, y + 32, queue.tiles[1].obj.sprite);
}


static int pix_pos_to_grid_index(vec2i world_pos) {
    vec2i grid_pos = {.x = ((world_pos.x - grid.obj.pos.x) / TILE_SIZE), .y = (world_pos.y - grid.obj.pos.y) / TILE_SIZE};
    return (grid_pos.y * grid.width) + grid_pos.x;
}

static void grid_debug_print() {
    for (int i = 0; i < GAMEBOARD_WIDTH * GAMEBOARD_HEIGHT; i++) {
        printf("%d", grid.tiles[i].filled);
        if ((i + 1) % GAMEBOARD_WIDTH == 0)
            printf("\n");
    }
}

static u32 at(uint x, uint y, u32 width) {
    return (x * width) + y;
}

static void grid_clear() {
    for (int i = 0; i < grid.width * grid.height; i++)
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
        state.pixels[at(y, x, SCREEN_WIDTH)] = color;
}

static void horiline(int x0, int x1, int y, u32 color) {
    for (int x = x0; x <= x1; x++)
        state.pixels[at(y, x, SCREEN_WIDTH)] = color;
}

static u32 * clone_pixels(u32 const * src, size_t len) {
   u32 * p = malloc(len * sizeof(u32));
   memcpy(p, src, len * sizeof(u32));
   return p;
}

static void draw_tile(tile_t t) {
    u32 x = t.pos.x * t.obj.size.x + grid.obj.pos.x;
    u32 y = t.pos.y * t.obj.size.y + grid.obj.pos.y;
    if (t.marked) {
        u32 *pixels = clone_pixels(t.obj.sprite->pixels, t.obj.sprite->width * t.obj.sprite->height);

        for (int i = 0; i < t.obj.sprite->width * t.obj.sprite->height; i++) {
            pixels[i] |= 0xFF1122FF;
        }

        sprite s = sprite_create_from(t.obj.size.x, t.obj.size.y, pixels);
        write_pixel_block(x, y, &s);
    } else {
        write_pixel_block(x, y, t.obj.sprite);
    }
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

    for (int i = 0; i < (grid.width * TILE_SIZE) + 1; i += TILE_SIZE) {
        verline(i + grid.obj.pos.x, grid.obj.pos.y, grid.height * TILE_SIZE + grid.obj.pos.y, line_color);
    }

    for (int i = grid.obj.pos.y; i < (grid.height * TILE_SIZE) + 1 + grid.obj.pos.y; i += TILE_SIZE) {
        horiline(grid.obj.pos.x, (grid.width * TILE_SIZE) + grid.obj.pos.x, i, line_color);
    }
}



static void flip_player() {
    tile_t t1 = player.t1;
    player.t1.filled = player.t2.filled;
    player.t1.letter = player.t2.letter;
    player.t1.obj.sprite = player.t2.obj.sprite;

    player.t2.filled = t1.filled;
    player.t2.letter = t1.letter;
    player.t2.obj.sprite = t1.obj.sprite;

}

static void draw_player() {
    draw_tile(player.t1);
    draw_tile(player.t2);
}

static void set_player() {
    grid.tiles[(player.t1.pos.y * GAMEBOARD_WIDTH) + player.t1.pos.x] = player.t1;
    grid.tiles[(player.t2.pos.y * GAMEBOARD_WIDTH) + player.t2.pos.x] = player.t2;
    LOG( "Set tiles at (%d, %d) and (%d, %d) : (%c %c)",
        player.t1.pos.x,
        player.t1.pos.y,
        player.t2.pos.x,
        player.t2.pos.y,
        player.t1.letter,
        player.t2.letter);
}



// static tile_t new_tile(bool filled, char letter, vec2i pos) {
//     return (tile_t){.filled=filled, .letter=letter, .pos=pos, .size=64};
// }

static void render() {
    clear_scr();
    draw_bg();
    draw_grid();

    SDL_UpdateTexture(state.texture, NULL, state.pixels, SCREEN_WIDTH * 4);

    draw_tiles();

    queue_render();

    if (player.active) draw_player();

    SDL_RenderCopyEx(state.renderer, state.texture, NULL, NULL, 0.0, NULL, SDL_FLIP_NONE);
    SDL_RenderPresent(state.renderer);
}

static bool check_tile_in_grid(tile_t t, vec2i move) {
    vec2i dest = vector_add(t.pos, move);
    return (dest.x < grid.width &&
            dest.x >= 0 &&
            dest.y < grid.height &&
            dest.y >= 0);
}

static bool check_player_in_grid(vec2i move) {
    return check_tile_in_grid(player.t1, move) && check_tile_in_grid(player.t2, move);
}

static bool check_tile_move(tile_t t, vec2i move) {

    int g_idx = ((t.pos.y + move.y) * grid.width) + (t.pos.x + move.x);
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

    int old_idx = (t->pos.y * grid.width) + t->pos.x;
    t->pos = vector_add(t->pos, move);
    grid.tiles[(t->pos.y * grid.width) + t->pos.x] = *t;
    grid.tiles[old_idx] = (tile_t){};
}

static void move_player(vec2i move) {
    // LOG("move_player: t1.pos=(%d,%d), t2.pos=(%d,%d)", player.t1.pos.x, player.t1.pos.y, player.t2.pos.x, player.t2.pos.y);
    player.t1.pos = vector_add(player.t1.pos, move);
    player.t2.pos = vector_add(player.t2.pos, move);
}


static void clear_player() {
    player.t1 = (tile_t){};
    player.t2 = (tile_t){};
}

static void grid_scan_hori() {
    for (int i = 1; i < grid.width * grid.height; i++) {
    }
}

// returns true if any tiles were cleared - that way we know to check for falling tiles
static bool grid_clear_marked() {
    bool cleared = false;
    for (int i = 0; i < grid.width * grid.height; i++) {
        if (grid.tiles[i].marked) {
            cleared = true;
            grid.tiles[i] = (tile_t){};
        }
    }

    return cleared;
}

static bool grid_scan() {
    bool cleared = false;
    LOG("Scanning...");
    state.halt = true;
    grid_scan_hori();
    cleared = grid_clear_marked();

    return cleared;
}

u32 *load_img_pixels(const char *file)
{
    SDL_Surface *surface = IMG_Load(file);
    u32 *pixels;
    if (surface) {
        LOG("Loading %s", file);
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

static sprite sprites[27];

static void load_sprites() {
    for (int i = 0; i < 26; i++) {
        sprites[i] = sprite_create_from(64, 64, load_img_pixels(letter_textures[i]));
    }
    LOG("Create sprite");
    sprites[26] = sprite_create_from(64 * 3, 64 * 2, load_img_pixels("gfx/QueueBorder.png"));
}

static tile_t *tile_create(u8 letter, bool filled, bool marked, uint x, uint y, uint h, uint w) {
    tile_t *t = &(tile_t){
        .letter = letter,
        .filled = filled,
        .marked = marked,
        .pos = {x, y},

        .obj = (obj_t){
            .sprite = &sprites[letter - 'A'],
            .pos = {x * w + grid.obj.pos.x, y * h + grid.obj.pos.y},
            .size = {w, h},
        },
    };

    return t;
}

static void queue_enqueue() {

    // queue.tiles[0] = new_tile(true, t1_letter, t1_pos);
    // queue.tiles[1] = new_tile(true, t2_letter, t2_pos);
    queue.tiles[0] = *tile_create(gen_random_letter(), true, false, -1, -1, 64, 64);
    queue.tiles[1] = *tile_create(gen_random_letter(), true, false, -1, -1, 64, 64);
}

static void spawn_player() {
    player.t1 = queue.tiles[0];
    player.t2 = queue.tiles[1];

    player.t1.pos = (vec2i){(grid.width / 2) - 1, 0};
    player.t2.pos = (vec2i){(grid.width / 2), 0};

    player.active = true;
}

static bool update_falling() {
    bool fall = false;

    vec2i move = {0, 1};
    for (int i = ((grid.width * grid.height) - grid.width) - 1; i > -1; i--) {
        if (check_tile_move(grid.tiles[i], move)) {
            move_tile(&grid.tiles[i], move);
            fall = true;
        }
    }

    return fall;
}

static void update_world_physics() {
    // anything falling? if not, check for marked tiles
    if (!update_falling()) {
        state.halt = grid_scan();

        // tiles were removed - don't spawn player yet
        if (state.halt) return;

        spawn_player();
        queue_enqueue();
    }
}

void stop_player() {
    set_player();
    state.halt = true;
    clear_player();

    player.active = false;
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
                LOG("CLICK - tile %d info: {.letter='%c', .pos=(%d, %d), .filled=%d, .marked=%d }", tile_index, t.letter, t.pos.x, t.pos.y, t.filled, t.marked);
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
                        grid_clear();
                        break;
                    default:
                        break;
                }
                break;
        }
    }
}

static void grid_init(int cols, int rows, int x, int y) {
    grid.width = cols;
    grid.height = rows;
    grid.obj = (obj_t){
        .pos = {x, y},
        .sprite = &((sprite){}),
        .size = {cols * TILE_SIZE, rows * TILE_SIZE},
    };
}

static void queue_init(int w, int h, int x, int y) {
    LOG("Creating queue...");
    queue.obj = (obj_t){
        .size = {w, h},
        .pos = {x, y},
        .sprite = &sprites[26],
    };
    queue_enqueue();
    LOG("Queue created");
}

int main(int argc, char *argv[]) {
    ASSERT(!SDL_Init(SDL_INIT_VIDEO), "SDL failed to initialize: %s\n", SDL_GetError());

    state.window = SDL_CreateWindow("Wordtris", SDL_WINDOWPOS_CENTERED_DISPLAY(1), SDL_WINDOWPOS_CENTERED_DISPLAY(1), SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_ALLOW_HIGHDPI);
    ASSERT(state.window, "Failed to create SDL Window: %s\n", SDL_GetError());

    state.renderer = SDL_CreateRenderer(state.window, -1, SDL_RENDERER_PRESENTVSYNC);
    ASSERT(state.renderer, "Failed to create SDL Renderer: %s\n", SDL_GetError());

    state.texture = SDL_CreateTexture(state.renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING, SCREEN_WIDTH, SCREEN_HEIGHT);
    ASSERT(state.renderer, "Failed to create SDL Texture: %s\n", SDL_GetError());

    SDL_SetTextureBlendMode(state.texture, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawBlendMode(state.renderer, SDL_BLENDMODE_BLEND);

    srand(time(NULL));

    grid_init(8, 10, (SCREEN_WIDTH / 2) - ((8 * TILE_SIZE) / 2), TILE_SIZE / 2);

    load_sprites();

    queue_init(64 * 3, 64 * 2, (SCREEN_WIDTH - (3 * TILE_SIZE)) - 16, TILE_SIZE * 1.5);

    spawn_player();
    queue_enqueue();

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
