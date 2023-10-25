#include <math.h>
#include <stdio.h>
#include <stdbool.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <time.h>

#include "../include/types.h"
#include "../include/vec.h"
#include "../include/sprite.h"
#include "../include/lpool.h"
#include "../include/trie.h"
#include "../include/macros.h"
#include "../include/render.h"

struct {
    SDL_Window *window;
    SDL_Texture *texture;
    SDL_Renderer *renderer;

    struct TrieNode *dict_trie;
    struct LetterPool letter_pool;

    vec2i mouse_pos;
    u32 pixels[SCREEN_WIDTH * SCREEN_HEIGHT];
    bool quit, updating_physics, paused, begin, gameover, clearing;
    double time;
    double tick;
} state;

struct grid {
    uint height, width;
    tile_t tiles[GAMEBOARD_MAX];

    obj_info_t obj;
} grid;

struct {
    tile_t tiles[2];

    obj_info_t obj;
} queue;


typedef enum {
    CLOCKWISE,
    COUNTER_CLOCKWISE,
} rotation_t;

typedef enum {
    HORI_AB,
    VERT_AB,
    HORI_BA,
    VERT_BA,
} player_orientation_t;

struct {
    bool active;
    player_orientation_t or;
    tile_t t1, t2;
} player;


static void queue_render() {
    u32 x = queue.obj.pos.x;
    u32 y = queue.obj.pos.y;

    // border
    sprite_render(x, y, queue.obj.sprite, state.texture);

    // tiles
    int gap_x = queue.tiles[0].obj.size.x / 2;
    int gap_y = queue.tiles[0].obj.size.y / 2;

    sprite_render(x + gap_x, y + gap_y, queue.tiles[0].obj.sprite, state.texture);
    sprite_render(x + gap_x * 3, y + gap_y, queue.tiles[1].obj.sprite, state.texture);
}


static int pix_pos_to_grid_index(vec2i world_pos) {
    vec2i grid_pos = {.x = ((world_pos.x - grid.obj.pos.x) / TILE_SIZE), .y = (world_pos.y - grid.obj.pos.y) / TILE_SIZE};
    return (grid_pos.y * grid.width) + grid_pos.x;
}

static u32 at(uint x, uint y, u32 width) {
    return (x * width) + y;
}

static void grid_clear() {
    for (int i = 0; i < grid.width * grid.height; i++)
        grid.tiles[i] = (tile_t){.letter=' '};
}


/*
 
 DRAWING

*/
static void clear_pixel_buf(u32 *pix, usize size) {
    for (int i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT; i++) {
        state.pixels[i] = 0x00000000;
    }
}

static void draw_tile(tile_t t) {
    u32 x = t.pos.x * t.obj.size.x + grid.obj.pos.x;
    u32 y = t.pos.y * t.obj.size.y + grid.obj.pos.y;
    if (t.marked) {
        u32 *pixels = clone_pixels(t.obj.sprite->pixels, t.obj.sprite->width * t.obj.sprite->height);

        for (int i = 0; i < t.obj.sprite->width * t.obj.sprite->height; i++) {
            pixels[i] |= 0xFFCCCC99;
        }

        pix_buf_render(x, y, t.obj.sprite->width, t.obj.sprite->height, pixels, state.texture);
    } else {
        sprite_render(x, y, t.obj.sprite, state.texture);
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

    int x = grid.obj.pos.x;
    int y = grid.obj.pos.y;

    for (int i = 0; i < (grid.width * TILE_SIZE) + 1; i += TILE_SIZE) {
        verline(i + x, y, grid.height * TILE_SIZE + y, line_color, state.pixels, SCREEN_WIDTH);
    }

    for (int i = y; i < (grid.height * TILE_SIZE) + 1 + y; i += TILE_SIZE) {
        horiline(x, (grid.width * TILE_SIZE) + x, i, line_color, state.pixels, SCREEN_WIDTH);
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
    clear_pixel_buf(state.pixels, SCREEN_WIDTH * SCREEN_HEIGHT);
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
    grid.tiles[old_idx] = (tile_t){.letter = ' '};
}

static void move_player(vec2i move) {
    // LOG("move_player: t1.pos=(%d,%d), t2.pos=(%d,%d)", player.t1.pos.x, player.t1.pos.y, player.t2.pos.x, player.t2.pos.y);
    player.t1.pos = vector_add(player.t1.pos, move);
    player.t2.pos = vector_add(player.t2.pos, move);
}


static void clear_player() {
    player.t1 = (tile_t){.letter = ' '};
    player.t2 = (tile_t){.letter = ' '};
}

// word is viable if it contains and vowel and a consonant
static bool check_word_viability(char* word) {
    bool contains_vowel = false;
    bool contains_consonant = false;

    for (int i = 0; i < strlen(word); i++) {
        switch(word[i]) {
            case 'a': case 'e': case 'i': case 'o': case 'u':
                contains_vowel = true;
                break;
            default:
                contains_consonant = true;
                break;
        }
    }

    return contains_vowel && contains_consonant;
}

// check that a substring is the minimum length and contains no spaces.
static const bool check_string_validity(const char* substring) {
    const int length = strlen(substring);

    if (length < 3) {
        return false;
    }

    for (int i = 0; i < length; i++) {
        if (substring[i] == ' ') {
            return false;
        }
    }

    return true;
}


// check for words in a given row of characters
static bool check_substrings(const char* str, const int* position) {
    int len = strlen(str);
    int maxLen = len < grid.height ? len : grid.height;

    for (int subLen = maxLen; subLen >= 3; subLen--) {
        for (int i = 0; i <= len - subLen; i++) {
            char substr[subLen + 1];
            strncpy(substr, str + i, subLen);
            substr[subLen] = '\0';

            if (check_word_viability(substr) && check_string_validity(substr)) {
                if (trie_search_word(state.dict_trie, substr)) {
                    int indexStart = i;
                    int indexEnd = i + subLen;

                    for (int x = indexStart; x < indexEnd; x++) {
                        grid.tiles[position[x]].marked = true;
                        LOG("Marked %d", position[x]);
                    }

                    return true;
                }
            }
        }
    }

    return false;
}


static bool grid_scan_hori() {
    bool found_word = false;
    // Horizontal check
    for (int i = 0; i < grid.height * grid.width; i += grid.width) {
        struct {
            char letters[8];
            int indexes[8];
        } row;

        // char line[grid.width];
        for (int j = 0; j < grid.width; j++) {
            if (grid.tiles[i + j].filled && !grid.tiles[i + j].marked)
                row.letters[j] = tolower(grid.tiles[i+j].letter);
            else
                row.letters[j] = ' ';

            row.indexes[j] = i + j;
        }

        row.letters[grid.width] = '\0';

        found_word = check_substrings(row.letters, row.indexes) || found_word;
        if (found_word)
            LOG("FOUND at row %d: '%s'", i / 8, row.letters);
    }

    return found_word;
}

static bool grid_scan_vert() {
    bool found_word = false;
    // Vertical check
    for (int i = 0; i < grid.width; i += 1) {
        struct {
            char letters[10];
            int indexes[10];
        } col;

        for (int j = 0; j < grid.height; j += 1) {
            int idx = j * grid.width + i;

            if (grid.tiles[idx].filled && !grid.tiles[idx].marked)
                col.letters[j] = tolower(grid.tiles[idx].letter);
            else
                col.letters[j] = ' ';

            col.indexes[j] = idx;
        }

        col.letters[grid.height] = '\0';

        found_word = check_substrings(col.letters, col.indexes) || found_word;
        if (found_word)
            LOG("FOUND in col %d: '%s'", i, col.letters);
    }

    return found_word;
}

// returns true if any tiles were cleared - that way we know to check for falling tiles
static bool grid_clear_marked() {
    bool cleared = false;
    for (int i = 0; i < grid.width * grid.height; i++) {
        if (grid.tiles[i].marked) {
            cleared = true;
            grid.tiles[i] = (tile_t){.letter = ' '};
        }
    }

    return cleared;
}

static bool grid_scan_marked() {
    bool marked = false;

    marked = grid_scan_hori();
    marked = marked || grid_scan_vert();

    LOG("Scanning... clearing should be %d", marked);

    return marked;
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


static const char* letter_textures[] = {
    "gfx/TileA.png", "gfx/TileB.png", "gfx/TileC.png", "gfx/TileD.png",
    "gfx/TileE.png", "gfx/TileF.png", "gfx/TileG.png", "gfx/TileH.png",
    "gfx/TileI.png", "gfx/TileJ.png", "gfx/TileK.png", "gfx/TileL.png",
    "gfx/TileM.png", "gfx/TileN.png", "gfx/TileO.png", "gfx/TileP.png",
    "gfx/TileQ.png", "gfx/TileR.png", "gfx/TileS.png", "gfx/TileT.png",
    "gfx/TileU.png", "gfx/TileV.png", "gfx/TileW.png", "gfx/TileX.png",
    "gfx/TileY.png", "gfx/TileZ.png",
};

static sprite sprites[27];

static void load_sprites() {
    for (int i = 0; i < 26; i++) {
        sprites[i] = sprite_create_from(64, 64, load_img_pixels(letter_textures[i]));
    }
    LOG("Create sprite");
    sprites[26] = sprite_create_from(64 * 3, 64 * 2, load_img_pixels("gfx/QueueBorder.png"));
}

static tile_t *tile_create(u8 letter, bool filled, bool marked, uint x, uint y) {
    tile_t *t = &(tile_t){
        .letter = letter,
        .filled = filled,
        .marked = marked,
        .pos = {x, y},

        .obj = (obj_info_t){
            .sprite = &sprites[letter - 'A'],
            .pos = {x * TILE_SIZE + grid.obj.pos.x, y * TILE_SIZE + grid.obj.pos.y},
            .size = {TILE_SIZE, TILE_SIZE},
        },
    };

    return t;
}

static void queue_enqueue() {
    queue.tiles[0] = *tile_create(lpool_random_letter(&state.letter_pool), true, false, -1, -1);
    queue.tiles[1] = *tile_create(lpool_random_letter(&state.letter_pool), true, false, -1, -1);
    LOG("Enqueued two tiles");
}

static void spawn_player() {
    player.or = HORI_AB;

    player.t1 = queue.tiles[0];
    player.t2 = queue.tiles[1];

    player.t1.pos = (vec2i){(grid.width / 2) - 1, 0};
    player.t2.pos = (vec2i){(grid.width / 2), 0};

    player.active = true;

    LOG("spawned player");
}

static bool update_world_physics() {
    bool updated = false;

    vec2i move = {0, 1};
    for (int i = ((grid.width * grid.height) - grid.width) - 1; i > -1; i--) {
        if (check_tile_move(grid.tiles[i], move)) {
            move_tile(&grid.tiles[i], move);
            updated = true;
        }
    }

    return updated;
}

void stop_player() {
    set_player();
    state.updating_physics = true;
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

static void update_tick() {
    if (state.clearing)
        state.tick = 300;
    else if (state.updating_physics)
        state.tick = 250;
    else
        state.tick = 1000;


    if (SDL_GetTicks() >= state.time + state.tick) {
        state.time = SDL_GetTicks();

        if (!state.clearing) {
            state.clearing = grid_scan_marked();
            if (state.clearing) {
                LOG("Set to clear at time: %f", state.time);
                return;
            }
        }

        if (state.clearing) {
            LOG("Cleared at time: %f", state.time);
            grid_clear_marked();
            state.clearing = false;
        }

        if (state.updating_physics) {
            state.updating_physics = update_world_physics();
            if (!state.updating_physics) {
                spawn_player();
                queue_enqueue();
            }
        } else {
            update_player_physics();
        }
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

static void rotate_player_cw() {
    int t1_x = player.t1.pos.x;
    int t2_x = player.t2.pos.x;
    int t1_y = player.t1.pos.y;
    int t2_y = player.t2.pos.y;

    if (t1_x < t2_x) {
        LOG("cw rotation 1");
        t1_y -= 1;
        t2_x -= 1;
    } else if (t1_x == t2_x && t1_y < t2_y) {
        LOG("cw rotation 2");
        t1_y += 1;
        t1_x += 1;
    } else if (t1_x > t2_x && t1_y == t2_y) {
        LOG("cw rotation 3");
        t1_x -= 1;
        t2_y -= 1;
    } else {
        LOG("cw rotation 4");
        t2_x += 1;
        t2_y += 1;
    }

    if (t1_y < 0 || t2_y < 0) {
        return;
    }

    bool kick_check =
        (grid.tiles[at(t2_y, t2_x, grid.width)].filled &&
        !grid.tiles[at(t2_y, t2_x - 1, grid.width)].filled) || (t1_x >= grid.width) || (t2_x >= grid.width) ||
        (grid.tiles[at(t1_y, t1_x, grid.width)].filled &&
        !grid.tiles[at(t1_y, t1_x - 1, grid.width)].filled);

    if (kick_check) {
        LOG("kick left");
        t1_x -= 1;
        t2_x -= 1;
    }


    bool t1_check =
        (grid.tiles[at(t1_y, t1_x, grid.width)].filled ||
        t1_x < 0 || t1_x >= grid.width);
    bool t2_check =
        (grid.tiles[at(t2_y, t2_x, grid.width)].filled ||
        t2_x < 0 || t2_x >= grid.width);

    if (t1_check || t2_check) {
        return;
    }

    player.t1.pos = (vec2i){t1_x, t1_y};
    player.t2.pos = (vec2i){t2_x, t2_y};
}

static void rotate_player_ccw() {

    int t1_x = player.t1.pos.x;
    int t2_x = player.t2.pos.x;
    int t1_y = player.t1.pos.y;
    int t2_y = player.t2.pos.y;

    if (t1_x < t2_x) {
        LOG("ccw rotation 1");
        t1_x += 1;
        t2_y -= 1;
    } else if (t1_x == t2_x && t1_y > t2_y) {
        LOG("ccw rotation 2");
        t2_x -= 1;
        t2_y += 1;
    } else if (t1_x > t2_x && t1_y == t2_y) {
        LOG("ccw rotation 3");
        t2_x += 1;
        t1_y -= 1;
    } else {
        LOG("ccw rotation 4");
        t1_y += 1;
        t1_x -= 1;
    }

    if (t1_y < 0 || t2_y < 0) {
        return;
    }

    bool kick_check =
        (grid.tiles[at(t2_y, t2_x, grid.width)].filled &&
        !grid.tiles[at(t2_y, t2_x + 1, grid.width)].filled) || (t1_x < 0) || (t2_x < 0) ||
        (grid.tiles[at(t1_y, t1_x, grid.width)].filled &&
        !grid.tiles[at(t1_y, t1_x + 1, grid.width)].filled);

    if (kick_check) {
        LOG("kick right");
        t1_x += 1;
        t2_x += 1;
    }

    bool t1_check =
        (grid.tiles[at(t1_y, t1_x, grid.width)].filled ||
        t1_x < 0 || t1_x >= grid.width);
    bool t2_check =
        (grid.tiles[at(t2_y, t2_x, grid.width)].filled ||
        t2_x < 0 || t2_x >= grid.width);

    if (t1_check || t2_check) {
        return;
    }

    player.t1.pos = (vec2i){t1_x, t1_y};
    player.t2.pos = (vec2i){t2_x, t2_y};

}

static void handle_input() {
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        switch (ev.type) {
            case SDL_QUIT:
                state.quit = true;
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
                        if (!state.updating_physics && !state.clearing) {
                            vec2i move = (vec2i){0, 1};
                            if (player_check_movement(move)) {
                                state.time = SDL_GetTicks();
                                move_player(move);
                            }
                            else {
                                stop_player();
                            }
                        }
                        break;
                    }
                    case SDLK_k:
                        rotate_player_cw();
                        break;
                    case SDLK_j:
                        rotate_player_ccw();
                        break;
                    case SDLK_w:
                    case SDLK_UP:
                        flip_player();
                        break;
                    case SDLK_SPACE:
                        grid_clear_marked();
                        break;
                    default:
                        break;
                }
                break;
        }
    }
}

static void grid_init(int cols, int rows) {
    int grid_x = (SCREEN_WIDTH / 2) - ((8 * TILE_SIZE) / 2);
    int grid_y = TILE_SIZE / 2;

    grid.width = cols;
    grid.height = rows;
    grid.obj = (obj_info_t){
        .pos = {grid_x, grid_y},
        .sprite = &((sprite){}),
        .size = {cols * TILE_SIZE, rows * TILE_SIZE},
    };
}

static void queue_init() {
    LOG("Creating queue...");
    int x = (SCREEN_WIDTH - (4 * TILE_SIZE));
    int y = TILE_SIZE * 1.5;
    int w = 64 * 3;
    int h = 64 * 2;

    queue.obj = (obj_info_t){
        .size = {w, h},
        .pos = {x, y},
        .sprite = &sprites[26],
    };
    queue_enqueue();
    LOG("Queue created");
}

static void init_engine() {
    ASSERT(!SDL_Init(SDL_INIT_VIDEO),
           "SDL failed to initialize: %s\n", SDL_GetError());

    state.window =
        SDL_CreateWindow("Wordtris",
                         SDL_WINDOWPOS_CENTERED_DISPLAY(1),
                         SDL_WINDOWPOS_CENTERED_DISPLAY(1),
                         SCREEN_WIDTH, SCREEN_HEIGHT,
                         SDL_WINDOW_ALLOW_HIGHDPI);

    ASSERT(state.window, "Failed to create SDL Window: %s\n", SDL_GetError());

    state.renderer =
        SDL_CreateRenderer(state.window, -1, SDL_RENDERER_PRESENTVSYNC);

    ASSERT(state.renderer, "Failed to create SDL Renderer: %s\n", SDL_GetError());

    state.texture
        = SDL_CreateTexture(state.renderer,
                            SDL_PIXELFORMAT_ABGR8888,
                            SDL_TEXTUREACCESS_STREAMING,
                            SCREEN_WIDTH,
                            SCREEN_HEIGHT);

    ASSERT(state.renderer, "Failed to create SDL Texture: %s\n", SDL_GetError());
}

static void init_game() {
    srand(time(NULL));
    load_sprites();

    state.dict_trie = trie_node_create();
    trie_construct(state.dict_trie, "./dictionary.txt");
    state.tick = 1000;
    state.time = SDL_GetTicks();

    lpool_init(&state.letter_pool);
    lpool_populate(&state.letter_pool);

    grid_init(8, 10);
    queue_init();
    spawn_player();
    queue_enqueue();
}

int main(int argc, char *argv[]) {

    init_engine();
    init_game();

    while (!state.quit) {
        update_tick();

        handle_input();

        render();
    }

    lpool_destroy(&state.letter_pool);
    trie_destroy(state.dict_trie);

    SDL_DestroyTexture(state.texture);

    return 0;
}
