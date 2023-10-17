#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <SDL2/SDL.h>

#define ASSERT(_e, ...) if (!(_e)) { fprintf(stderr, __VA_ARGS__); exit(1); }

typedef float f32;
typedef double f64;
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;
typedef size_t usize;
typedef ssize_t isize;

typedef struct v2_s { i32 x, y; } v2;

#define SCREEN_WIDTH 1080
#define SCREEN_HEIGHT 720
#define WORLD_TICK 250

#define TILE_SIZE 64
#define GRID_WIDTH 8
#define GRID_HEIGHT 10

#define GRID_END_X ((GRID_WIDTH - 1) * TILE_SIZE)
#define GRID_END_Y ((GRID_HEIGHT - 1) * TILE_SIZE)

struct {
    SDL_Window *window;
    SDL_Texture *texture;
    SDL_Renderer *renderer;
    v2 mouse_pos;
    u32 pixels[SCREEN_WIDTH * SCREEN_HEIGHT];
    bool quit, halt, paused, begin, gameover;
    double time;
    double tick;
} state;


static inline int scr_idx_to_col(int i) {return i % SCREEN_WIDTH;}
static inline int scr_idx_to_row(int i) {return i / SCREEN_WIDTH;}

static inline int scr_idx_to_grid_row(int i) {return (scr_idx_to_row(i) / TILE_SIZE);}
static inline int scr_idx_to_grid_col(int i) {return (scr_idx_to_col(i) / TILE_SIZE);}

static inline v2 scr_idx_to_pos(int i) {
    return (v2){.x = scr_idx_to_col(i), .y = scr_idx_to_row(i)};
}

static inline v2 scr_pos_to_grid(v2 pos) {
    return (v2){.x = pos.x / TILE_SIZE, .y = pos.y / TILE_SIZE};
}

static inline int scr_pos_to_grid_idx(v2 pos) {
    v2 grid_pos = (v2){.x = pos.x / TILE_SIZE, .y = pos.y / TILE_SIZE};
    return (grid_pos.y * GRID_WIDTH) + grid_pos.x;
}

static inline u32 grid_pos_to_idx(v2 pos) {
    return (pos.y * GRID_WIDTH) + pos.x;
}

static inline u32 scr_idx_to_grid_idx(int i) {
    v2 scr_pos = scr_idx_to_pos(i);
    v2 grid_pos = scr_pos_to_grid(scr_pos);
    return grid_pos_to_idx(grid_pos);
}


static inline int scr_pos_to_idx(v2 pos) {
    return pos.x + SCREEN_WIDTH * pos.y;
}

#define grid_idx(i) scr_idx_to_grid_idx(i)
#define scr_idx(pos) scr_pos_to_idx(pos)

typedef struct tile {
    u8 letter;
    bool filled;
    u32 color;
    v2 pos;
} tile_t;

struct {
    tile_t tiles[GRID_WIDTH * GRID_HEIGHT];
} grid;

struct {
    struct {tile_t pieces[GRID_WIDTH * GRID_HEIGHT]; usize n; } pieces;
} pieces;

static void debug_print_grid() {
    for (int i = 0; i < GRID_WIDTH * GRID_HEIGHT; i++) {
        printf("%d", grid.tiles[i].filled);
        if ((i + 1) % GRID_WIDTH == 0)
            printf("\n");
    }
}
static void clear_grid() {
    for (int i = 0; i < GRID_WIDTH * GRID_HEIGHT; i++)
        grid.tiles[i] = (tile_t){.letter='\0', .filled=false, .color=0x00000000};
}

static v2 vector_add(v2 v0, v2 v1) {
    return (v2){v0.x + v1.x, v0.y + v1.y};
}

static void draw_tile(tile_t t) {
    int border = 2;
    for (int x = t.pos.x + (1.5*border); x < t.pos.x + TILE_SIZE - border; x++) {
        for (int y = t.pos.y + (1.5*border); y < t.pos.y + TILE_SIZE - border; y++) {
            state.pixels[scr_idx(((v2){x, y}))] = t.color;
        }
    }
}

static void draw_grid(int x, int y) {
    for (int i = x * y; i < SCREEN_WIDTH * SCREEN_HEIGHT; i++) {

        if (scr_idx_to_col(i) < x || scr_idx_to_col(i) > (TILE_SIZE * GRID_WIDTH) + x)
            continue;

        if (scr_idx_to_row(i) < y || scr_idx_to_row(i) > (TILE_SIZE * GRID_HEIGHT) + y)
            continue;

        if ((scr_idx_to_col(i) + x) % TILE_SIZE == 0 || (scr_idx_to_row(i) + y) % TILE_SIZE == 0)
            state.pixels[i] = 0xFFAAAAAA;
        else {
            int gi = grid_idx(i);
            ASSERT(gi < GRID_WIDTH * GRID_HEIGHT && gi >= 0, "index %d NOT within grid bounds", gi);
            // draw_tile(grid.tiles[grid_idx(i)]);
            state.pixels[i] = grid.tiles[grid_idx(i)].color;
        }
    }
}

static void verline(int x, int y0, int y1, u32 color) {
    for (int y = y0; y <= y1; y++)
        state.pixels[(y * SCREEN_WIDTH) + x] = color;
}

struct {
    tile_t t1, t2;
} player;

static void flip_player() {
    tile_t t1 = player.t1;
    player.t1.filled = player.t2.filled;
    player.t1.color = player.t2.color;
    player.t1.letter = player.t2.letter;

    player.t2.filled = t1.filled;
    player.t2.color = t1.color;
    player.t2.letter = t1.letter;
}


static void draw_player() {
    draw_tile(player.t1);
    draw_tile(player.t2);
}

static void set_player() {
    grid.tiles[scr_pos_to_grid_idx(player.t1.pos)] = player.t1;
    grid.tiles[scr_pos_to_grid_idx(player.t2.pos)] = player.t2;
}

static tile_t new_tile(bool filled, u32 color, char letter, v2 pos) {
    return (tile_t){.filled=filled, .color=color, .letter=letter, .pos=pos};
}

static void render() {
    draw_grid(0, 0);
    draw_player();
}

static bool inline check_player_in_grid(v2 move) {
    return (player.t1.pos.x + move.x <= GRID_END_X &&
            player.t2.pos.x + move.x <= GRID_END_X &&
            player.t1.pos.x + move.x >= 0 &&
            player.t2.pos.x + move.x >= 0 &&
            player.t1.pos.y + move.y <= GRID_END_Y &&
            player.t2.pos.y + move.y <= GRID_END_Y &&
            player.t1.pos.y + move.y >= 0 &&
            player.t2.pos.y + move.y >= 0);
}


static bool inline check_player_collision(v2 move_vec) {
    bool safe = true;
    int t1_dest_idx = scr_pos_to_grid_idx(vector_add(player.t1.pos, move_vec));
    int t2_dest_idx = scr_pos_to_grid_idx(vector_add(player.t2.pos, move_vec));

    if (grid.tiles[t1_dest_idx].filled || grid.tiles[t2_dest_idx].filled)
        safe = false;

    return safe;
}

static bool inline check_tile_move(tile_t t, v2 move) {
    int g_idx = scr_pos_to_grid_idx(vector_add(t.pos, move));
    if (t.filled && grid.tiles[g_idx].filled == false) {
        return true;
    }
    return false;
}

static void move_player(v2 move) {
    player.t1.pos.x += move.x;
    player.t1.pos.y += move.y;
    player.t2.pos.x += move.x;
    player.t2.pos.y += move.y;
}

static void move_tile(tile_t t, v2 move) {
    grid.tiles[scr_pos_to_grid_idx(t.pos)] = (tile_t){.color=0, .filled=false, .letter='\0', .pos={}};
    t.pos.x += move.x;
    t.pos.y += move.y;
    grid.tiles[scr_pos_to_grid_idx(t.pos)] = t;
}

#include <time.h>

static void spawn_player() {
    // todo: make random
    char t1_letter = 65 + (rand() % 26);
    char t2_letter = 65 + (rand() % 26);
    v2 t1_pos = (v2){0, 0};
    v2 t2_pos = (v2){TILE_SIZE, 0};

    player.t1 = new_tile(true, 0xFF0000FF, t1_letter, t1_pos);
    player.t2 = new_tile(true, 0xFFFF0000, t2_letter, t2_pos);
}

static void clear_player() {
    player.t1 = (tile_t){};
    player.t2 = (tile_t){};
}

int main(int argc, char *argv[]) {
    ASSERT(!SDL_Init(SDL_INIT_VIDEO), "SDL failed to initialize: %s\n", SDL_GetError());

    state.window = SDL_CreateWindow("Wordtris", SDL_WINDOWPOS_CENTERED_DISPLAY(1), SDL_WINDOWPOS_CENTERED_DISPLAY(1), SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_ALLOW_HIGHDPI);
    ASSERT(state.window, "Failed to create SDL Window: %s\n", SDL_GetError());

    state.renderer = SDL_CreateRenderer(state.window, -1, SDL_RENDERER_PRESENTVSYNC);
    ASSERT(state.renderer, "Failed to create SDL Renderer: %s\n", SDL_GetError());

    state.texture = SDL_CreateTexture(state.renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING, SCREEN_WIDTH, SCREEN_HEIGHT);
    ASSERT(state.renderer, "Failed to create SDL Texture: %s\n", SDL_GetError());

    clear_grid();
    spawn_player();

    state.tick = 1000.0;
    state.time = SDL_GetTicks();

    while (!state.quit) {
        SDL_Event ev;

        if (state.halt) {

            if (SDL_GetTicks() >= state.time + WORLD_TICK) {
                state.time = SDL_GetTicks();
                v2 fall = (v2){0, TILE_SIZE};
                bool none_falling = true;
                for (int i = ((GRID_WIDTH * GRID_HEIGHT) - GRID_WIDTH) - 1; i > -1; i--) {
                    if (check_tile_move(grid.tiles[i], fall)) {
                        none_falling = false;
                        move_tile(grid.tiles[i], fall);
                    }
                }
                if (none_falling) {
                    state.halt = false;
                    spawn_player();
                }
            }

        } else {

            if (SDL_GetTicks() >= state.time + state.tick) {
                state.time = SDL_GetTicks();
                v2 fall = (v2){0, TILE_SIZE};
                if (check_player_in_grid(fall) && check_player_collision(fall)) {
                    move_player(fall);
                } else {
                    set_player();
                    state.halt = true;
                    clear_player();
                }
            }

        }


        while (SDL_PollEvent(&ev)) {
            switch (ev.type) {
                case SDL_QUIT:
                    state.quit = true;
                    break;
                case SDL_MOUSEBUTTONDOWN:
                    printf("tile letter %c: x=%d y=%d\n", grid.tiles[scr_pos_to_grid_idx(state.mouse_pos)].letter, state.mouse_pos.x, state.mouse_pos.y);
                    grid.tiles[scr_pos_to_grid_idx(state.mouse_pos)] = (tile_t){};
                    break;
                case SDL_MOUSEMOTION:
                    SDL_GetMouseState(&state.mouse_pos.x, &state.mouse_pos.y);
                    break;
                case SDL_KEYDOWN:
                    /* Check the SDLKey values and move change the coords */
                    switch ( ev.key.keysym.sym ) {
                        case SDLK_ESCAPE: {
                            state.quit = true;
                        }
                        case SDLK_LEFT: {
                            v2 move = (v2){-TILE_SIZE, 0};
                            if (check_player_in_grid(move) && check_player_collision(move))
                                move_player(move);
                            break;
                        }
                        case SDLK_RIGHT: {
                            v2 move = (v2){TILE_SIZE, 0};
                            if (check_player_in_grid(move) && check_player_collision(move))
                                move_player(move);
                            break;
                        }
                        case SDLK_UP:
                            flip_player();
                            break;
                        case SDLK_DOWN: {
                            v2 move = (v2){0, TILE_SIZE};
                            if (check_player_in_grid(move) && check_player_collision(move)) {
                                state.time = SDL_GetTicks();
                                move_player(move);
                            }
                            else {
                                set_player();
                                state.halt = true;
                                clear_player();
                            }
                            break;
                        }
                        case SDLK_SPACE:
                            set_player();
                            spawn_player();

                            debug_print_grid();
                            break;
                        default:
                            break;
                    }
                    break;
            }
        }


        render();

        SDL_UpdateTexture(state.texture, NULL, state.pixels, SCREEN_WIDTH * 4);
        SDL_RenderCopyEx(state.renderer, state.texture, NULL, NULL, 0.0, NULL, SDL_FLIP_NONE);
        SDL_RenderPresent(state.renderer);
    }

    SDL_DestroyTexture(state.texture);


    return 0;
}
