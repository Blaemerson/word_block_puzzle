#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <SDL2/SDL.h>

#define ASSERT(_e, ...) if (!(_e)) { fprintf(stderr, "%s %d: ", __FILE__, __LINE__); fprintf(stderr, __VA_ARGS__);  exit(1); }

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

typedef struct vec2_s { i32 x, y; } vec2;

#define SCREEN_WIDTH 1080
#define SCREEN_HEIGHT 720
#define WORLD_TICK 250

#define TILE_SIZE 64
#define GRID_WIDTH 8
#define GRID_HEIGHT 10
#define GRID_MAX (GRID_WIDTH * GRID_HEIGHT)

#define GRID_END_X ((GRID_WIDTH - 1) * TILE_SIZE)
#define GRID_END_Y ((GRID_HEIGHT - 1) * TILE_SIZE)

#define CLEAR 0x00000000
#define RED 0xFF0000FF
#define BLUE 0xFFFF0000

struct {
    SDL_Window *window;
    SDL_Texture *texture;
    SDL_Renderer *renderer;
    vec2 mouse_pos;
    u32 pixels[SCREEN_WIDTH * SCREEN_HEIGHT];
    bool quit, halt, paused, begin, gameover;
    double time;
    double tick;
} state;


static inline int pix_to_x_pos(int i) {return i % SCREEN_WIDTH;}
static inline int pix_to_y_pos(int i) {return i / SCREEN_WIDTH;}

static inline vec2 grid_pos_to_pix_pos(vec2 pos) { return (vec2){pos.x * TILE_SIZE, pos.y * TILE_SIZE}; }

static inline vec2 pix_index_to_pix_pos(int i) {
    return (vec2){.x = pix_to_x_pos(i), .y = pix_to_y_pos(i)};
}

static inline vec2 pix_pos_to_grid_pos(vec2 pos) {
    return (vec2){.x = pos.x / TILE_SIZE, .y = pos.y / TILE_SIZE};
}

static inline int pix_pos_to_grid_index(vec2 pos) {
    vec2 grid_pos = (vec2){.x = pos.x / TILE_SIZE, .y = pos.y / TILE_SIZE};
    return (grid_pos.y * GRID_WIDTH) + grid_pos.x;
}

static inline u32 grid_pos_to_grid_index(vec2 pos) {
    return (pos.y * GRID_WIDTH) + pos.x;
}

static inline u32 pix_index_to_grid_index(int i) {
    vec2 scr_pos = pix_index_to_pix_pos(i);
    vec2 grid_pos = pix_pos_to_grid_pos(scr_pos);
    return grid_pos_to_grid_index(grid_pos);
}


static inline u32 pix_pos_to_pix_index(vec2 pos) {
    return pos.x + SCREEN_WIDTH * pos.y;
}

typedef struct tile {
    u8 letter;
    bool filled;
    u32 color;
    vec2 pos;
} tile_t;

struct {
    tile_t tiles[GRID_WIDTH * GRID_HEIGHT];
} grid;


static void debug_print_grid() {
    for (int i = 0; i < GRID_WIDTH * GRID_HEIGHT; i++) {
        printf("%d", grid.tiles[i].filled);
        if ((i + 1) % GRID_WIDTH == 0)
            printf("\n");
    }
}
static void clear_grid() {
    for (int i = 0; i < GRID_WIDTH * GRID_HEIGHT; i++)
        grid.tiles[i] = (tile_t){.letter='\0', .filled=false, .color=CLEAR};
}

static vec2 vec2_create(int x, int y) {
    return (vec2){x, y};
}

static inline vec2 vector_add(vec2 v0, vec2 v1) {
    return (vec2){v0.x + v1.x, v0.y + v1.y};
}

static inline vec2 vector_add_to_all(vec2 v0, int c) {
    return (vec2){v0.x + c, v0.y + c};
}

static inline vec2 vector_multiply(vec2 v, int s) {
    return (vec2){v.x * s, v.y * s};
}

static void draw_tile(tile_t t) {
    int border = 2;
    vec2 real_start = grid_pos_to_pix_pos(t.pos);
    vec2 real_end = vector_add_to_all(real_start, TILE_SIZE);
    for (int x = real_start.x + (1.5*border); x < real_end.x - border; x++) {
        for (int y = real_start.y + (1.5*border); y < real_end.y - border; y++) {
            state.pixels[pix_pos_to_pix_index(((vec2){x, y}))] = t.color;
        }
    }
}

static void draw_grid(int x, int y) {
    for (int i = x * y; i < SCREEN_WIDTH * SCREEN_HEIGHT; i++) {

        vec2 scr_pos = pix_index_to_pix_pos(i);
        if (scr_pos.x < x || scr_pos.x > (TILE_SIZE * GRID_WIDTH) + x)
            continue;

        if (scr_pos.y < y || scr_pos.y > (TILE_SIZE * GRID_HEIGHT) + y)
            continue;

        if ((scr_pos.x + x) % TILE_SIZE == 0 || (scr_pos.y + y) % TILE_SIZE == 0)
            state.pixels[i] = 0xFFAAAAAA;
        else {
            int gi = pix_index_to_grid_index(i);
            ASSERT(gi < GRID_MAX && gi >= 0, "index %d NOT within grid bounds", gi);
            state.pixels[i] = grid.tiles[gi].color;
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
    grid.tiles[grid_pos_to_grid_index(player.t1.pos)] = player.t1;
    grid.tiles[grid_pos_to_grid_index(player.t2.pos)] = player.t2;
}

static tile_t new_tile(bool filled, u32 color, char letter, vec2 pos) {
    return (tile_t){.filled=filled, .color=color, .letter=letter, .pos=pos};
}

static void render() {
    draw_grid(0, 0);
    draw_player();
}

static bool inline check_player_in_grid(vec2 move) {
    return (player.t1.pos.x + move.x < GRID_WIDTH &&
            player.t2.pos.x + move.x < GRID_WIDTH &&
            player.t1.pos.x + move.x >= 0 &&
            player.t2.pos.x + move.x >= 0 &&
            player.t1.pos.y + move.y < GRID_HEIGHT &&
            player.t2.pos.y + move.y < GRID_HEIGHT &&
            player.t1.pos.y + move.y >= 0 &&
            player.t2.pos.y + move.y >= 0);
}


static bool inline check_player_collision(vec2 move_vec) {
    bool safe = true;
    int t1_dest_idx = grid_pos_to_grid_index(vector_add(player.t1.pos, move_vec));
    int t2_dest_idx = grid_pos_to_grid_index(vector_add(player.t2.pos, move_vec));

    if (grid.tiles[t1_dest_idx].filled || grid.tiles[t2_dest_idx].filled)
        safe = false;

    return safe;
}

static bool inline check_tile_move(tile_t t, vec2 move) {
    int g_idx = grid_pos_to_grid_index(vector_add(t.pos, move));
    if (t.filled && grid.tiles[g_idx].filled == false) {
        return true;
    }
    return false;
}

static void move_player(vec2 move) {
    player.t1.pos.x += move.x;
    player.t1.pos.y += move.y;
    player.t2.pos.x += move.x;
    player.t2.pos.y += move.y;
}

static void move_tile(tile_t t, vec2 move) {
    grid.tiles[grid_pos_to_grid_index(t.pos)] = (tile_t){};
    t.pos.x += move.x;
    t.pos.y += move.y;
    grid.tiles[grid_pos_to_grid_index(t.pos)] = t;
}

static void spawn_player() {
    char t1_letter = 65 + (rand() % 26);
    char t2_letter = 65 + (rand() % 26);
    vec2 t1_pos = (vec2){3, 0};
    vec2 t2_pos = (vec2){4, 0};

    player.t1 = new_tile(true, RED, t1_letter, t1_pos);
    player.t2 = new_tile(true, BLUE, t2_letter, t2_pos);
}

static void clear_player() {
    player.t1 = (tile_t){};
    player.t2 = (tile_t){};
}

static void update_physics() {
    if (state.halt) {
        if (SDL_GetTicks() >= state.time + WORLD_TICK) {
            state.time = SDL_GetTicks();
            vec2 fall = (vec2){0, 1};
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
            vec2 fall = (vec2){0, 1};
            if (check_player_in_grid(fall) && check_player_collision(fall)) {
                move_player(fall);
            } else {
                set_player();
                state.halt = true;
                clear_player();
            }
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

    clear_grid();
    spawn_player();

    state.tick = 1000.0;
    state.time = SDL_GetTicks();

    while (!state.quit) {
        SDL_Event ev;

        update_physics();

        while (SDL_PollEvent(&ev)) {
            switch (ev.type) {
                case SDL_QUIT:
                    state.quit = true;
                    break;
                case SDL_MOUSEBUTTONDOWN:
                    printf("tile letter %c: x=%d y=%d\n", grid.tiles[pix_pos_to_grid_index(state.mouse_pos)].letter, state.mouse_pos.x, state.mouse_pos.y);
                    grid.tiles[pix_pos_to_grid_index(state.mouse_pos)] = (tile_t){};
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
                            vec2 move = (vec2){-1, 0};
                            if (check_player_in_grid(move) && check_player_collision(move))
                                move_player(move);
                            break;
                        }
                        case SDLK_RIGHT: {
                            vec2 move = (vec2){1, 0};
                            if (check_player_in_grid(move) && check_player_collision(move))
                                move_player(move);
                            break;
                        }
                        case SDLK_UP:
                            flip_player();
                            break;
                        case SDLK_DOWN: {
                            vec2 move = (vec2){0, 1};
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
