#include <math.h>
#include <stdio.h>
#include <stdbool.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <time.h>

#include "../include/types.h"
#include "../include/lpool.h"
// #include "../include/trie.h"
#include "../include/macros.h"
#include "../include/render.h"
#include "../include/tile.h"
#include "../include/word.h"

struct {
    SDL_Window *window;
    SDL_Texture *texture;
    SDL_Renderer *renderer;

    struct TrieNode *dict_trie;
    struct LetterPool letter_pool;

    vec2i mouse_pos;
    u32 pixels[SCREEN_WIDTH * SCREEN_HEIGHT];
    bool quit, halt, paused, begin, gameover, clearing;
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

struct {
    bool active;
    tile_t t1, t2;
} player;

u32 *load_img_pixels(const char *file) {
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

static tile_t *create_empty_tile() {
    IFDEBUG_LOG("Created default tile");
    return &(tile_t){
        .letter='\0',
        .marked=false,
        .filled=false,
        .connected=CON_NONE,
        .pos={-1, -1},
        .obj={},
    };
}

/*
 
 DRAWING

*/
static void clear_pixel_buf(u32 *pix, usize size) {
    for (int i = 0; i < size; i++) {
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
    }
    else if (t.greyed) {
        u32 *pixels = clone_pixels(t.obj.sprite->pixels, t.obj.sprite->width * t.obj.sprite->height);

        for (int i = 0; i < t.obj.sprite->width * t.obj.sprite->height; i++) {
            pixels[i] ^= 0xFFCCCC99;
        }

        pix_buf_render(x, y, t.obj.sprite->width, t.obj.sprite->height, pixels, state.texture);

    } else {
        sprite_render(x, y, t.obj.sprite, state.texture);
    }
}

static void draw_tiles(tile_t *tiles, u32 count) {
    for (int i = 0; i < count; i++)
        if (tiles[i].filled)
            draw_tile(tiles[i]);
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



static void player_flip() {
    tile_t t1 = player.t1;
    player.t1.filled = player.t2.filled;
    player.t1.letter = player.t2.letter;
    player.t1.obj.sprite = player.t2.obj.sprite;

    player.t2.filled = t1.filled;
    player.t2.letter = t1.letter;
    player.t2.obj.sprite = t1.obj.sprite;

}

static void player_draw() {
    draw_tile(player.t1);
    draw_tile(player.t2);
}

static void player_set() {
    grid.tiles[(player.t1.pos.y * GAMEBOARD_WIDTH) + player.t1.pos.x] = player.t1;
    grid.tiles[(player.t2.pos.y * GAMEBOARD_WIDTH) + player.t2.pos.x] = player.t2;
    IFDEBUG_LOG( "Set tiles at (%d, %d) and (%d, %d) : (%c %c)",
        player.t1.pos.x,
        player.t1.pos.y,
        player.t2.pos.x,
        player.t2.pos.y,
        player.t1.letter,
        player.t2.letter);
}

static void player_set_greyed() {
    player.t1.greyed = true;
    player.t2.greyed = true;
    grid.tiles[(player.t1.pos.y * grid.width) + player.t1.pos.x] = player.t1;
    grid.tiles[(player.t2.pos.y * grid.width) + player.t2.pos.x] = player.t2;
    IFDEBUG_LOG( "Set tiles at (%d, %d) and (%d, %d) : (%c %c)",
        player.t1.pos.x,
        player.t1.pos.y,
        player.t2.pos.x,
        player.t2.pos.y,
        player.t1.letter,
        player.t2.letter);
}

static void render() {
    clear_pixel_buf(state.pixels, SCREEN_WIDTH * SCREEN_HEIGHT);
    draw_bg();
    draw_grid();

    SDL_UpdateTexture(state.texture, NULL, state.pixels, SCREEN_WIDTH * 4);

    draw_tiles(grid.tiles, grid.width * grid.height);

    queue_render();

    if (player.active) player_draw();

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


static bool check_tile_move(tile_t t, vec2i move) {

    int g_idx = ((t.pos.y + move.y) * grid.width) + (t.pos.x + move.x);
    if (t.filled && !(grid.tiles[g_idx].filled)) {
        return true;
    }
    return false;
}

static void move_tile(tile_t *t, vec2i move) {
    ASSERT(check_tile_move(*t, move), "couldn't move tile");

    int old_idx = (t->pos.y * grid.width) + t->pos.x;
    t->pos = vector_add(t->pos, move);
    grid.tiles[(t->pos.y * grid.width) + t->pos.x] = *t;
    grid.tiles[old_idx] = (tile_t){.letter = ' '};
}

static bool check_player_in_grid(vec2i move) {
    return check_tile_in_grid(player.t1, move) && check_tile_in_grid(player.t2, move);
}

static inline bool check_player_move(vec2i move) {
    return check_tile_move(player.t1, move) && check_tile_move(player.t2, move);
}

static void player_move(vec2i move) {
    player.t1.pos = vector_add(player.t1.pos, move);
    player.t2.pos = vector_add(player.t2.pos, move);
}


static void player_clear() {
    player.t1 = *create_empty_tile();
    player.t2 = *create_empty_tile();
}


static bool grid_scan_hori() {
    bool found_word = false;
    // Horizontal check
    for (int i = 0; i < grid.height * grid.width; i += grid.width) {
        struct {
            char *letters;
            uint *indices;
        } row;

        row.letters = malloc(grid.width * sizeof(char));
        row.indices = malloc(grid.width * sizeof(uint));

        row.letters[grid.width] = '\0';

        // collect row letters along with their index in the game board
        for (int j = 0; j < grid.width; j++) {
            if (grid.tiles[i + j].filled) {
                row.letters[j] = tolower(grid.tiles[i+j].letter);
            }
            else {
                row.letters[j] = ' ';
            }
            row.indices[j] = i + j;
        }
        found_word = check_substrings(row.letters, row.indices, grid.tiles, grid.width, grid.height, state.dict_trie) || found_word;

        free(row.letters);
        free(row.indices);
    }

    return found_word;
}

static bool grid_scan_vert() {
    bool found_word = false;
    // Vertical check
    for (int i = 0; i < grid.width; i += 1) {
        struct {
            char *letters;
            uint *indices;
        } col;

        col.letters = malloc(grid.height * sizeof(char));
        col.indices = malloc(grid.height * sizeof(uint));

        col.letters[grid.height] = '\0';

        for (int j = 0; j < grid.height; j += 1) {
            int idx = j * grid.width + i;

            if (grid.tiles[idx].filled)
                col.letters[j] = tolower(grid.tiles[idx].letter);
            else
                col.letters[j] = ' ';

            col.indices[j] = idx;
        }


        found_word = check_substrings(col.letters, col.indices, grid.tiles, grid.width, grid.height, state.dict_trie) || found_word;

        free(col.letters);
        free(col.indices);
    }

    return found_word;
}

// returns true if any tiles were cleared - that way we know to check for falling tiles
static bool grid_clear_marked() {
    bool cleared = false;
    for (int i = 0; i < grid.width * grid.height; i++) {
        if (grid.tiles[i].marked) {
            cleared = true;
            grid.tiles[i] = *create_empty_tile();
        }
    }

    return cleared;
}

static bool grid_scan_for_words() {
    bool marked = false;

    LOG("before grid_hori");
    marked = grid_scan_hori();
    LOG("before grid_vert");
    marked = marked || grid_scan_vert();

    LOG("Scanning... clearing should be %d", marked);

    return marked;
}


static tile_t *tile_create(u8 letter, bool filled, bool marked, bool greyed, uint x, uint y) {
    tile_t *t = &(tile_t){
        .letter = letter,
        .filled = filled,
        .marked = marked,
        .greyed = greyed,
        .connected = CON_NONE,
        .pos = {x, y},
        .obj = (obj_info_t){
            .sprite = &sprites[letter - 'A'],
            .pos = {x * TILE_SIZE + grid.obj.pos.x, y * TILE_SIZE + grid.obj.pos.y},
            .size = {TILE_SIZE, TILE_SIZE},
        },
    };

    return t;
}

static void grid_randomize_grey_tiles() {
    int count = 0;
    int max = 10;
    for (int i = grid.width * 2; i < grid.width * grid.height; i++) {
        if (i > rand() % 240 && count < 10) {
            grid.tiles[i] = *tile_create(lpool_random_letter(&state.letter_pool), true, false, true, i % grid.width, i / grid.width);
            count++;
        }
    }
}


static void queue_enqueue() {
    queue.tiles[0] = *tile_create(lpool_random_letter(&state.letter_pool), true, false, false, -1, -1);
    queue.tiles[1] = *tile_create(lpool_random_letter(&state.letter_pool), true, false, false, -1, -1);
    LOG("Enqueued two tiles");
}

// true if successful, false otherwise
static bool spawn_player() {
    player.t1 = queue.tiles[0];
    player.t2 = queue.tiles[1];

    player.t1.pos = (vec2i){(grid.width / 2) - 1, 0};
    player.t2.pos = (vec2i){(grid.width / 2), 0};

    player.active = true;

    player.t1.connected=CON_RIGHT;
    player.t2.connected=CON_LEFT;

    LOG("Spawned player");
    if (grid.tiles[at(player.t1.pos.y, player.t1.pos.x, grid.width)].filled ||
        grid.tiles[at(player.t2.pos.y, player.t2.pos.x, grid.width)].filled) {
        return false;
    }

    return true;
}

static bool update_tile_connections() {
    bool updated = false;
    for (int i = 0; i < grid.width * grid.height; i++) {
        if (grid.tiles[i].filled) {
            switch(grid.tiles[i].connected) {
                case(CON_LEFT):
                    if (grid.tiles[i - 1].connected != CON_RIGHT) {
                        updated = true;
                        grid.tiles[i].connected = CON_NONE;
                    }
                    break;
                case(CON_RIGHT):
                    if (grid.tiles[i + 1].connected != CON_LEFT) {
                        updated = true;
                        grid.tiles[i].connected = CON_NONE;
                    }
                    break;
                case(CON_UP):
                    if (grid.tiles[i - grid.width].connected != CON_DOWN) {
                        updated = true;
                        grid.tiles[i].connected = CON_NONE;
                    }
                    break;
                case(CON_DOWN):
                    if (grid.tiles[i + grid.width].connected != CON_UP) {
                        updated = true;
                        grid.tiles[i].connected = CON_NONE;
                    }
                    break;
                default:
                    break;
            }
        }
    }

    return updated;
}

static bool update_world_physics() {
    bool updated = false;

    vec2i move = {0, 1};
    for (int i = ((grid.width * grid.height) - grid.width) - 1; i > -1; i--) {
        if (grid.tiles[i].greyed) {
            continue;
        }
        switch(grid.tiles[i].connected) {
            case (CON_LEFT):
                ASSERT(i % grid.width > 0, "Impossible, tile cannot be connected to the left" );
                if (check_tile_move(grid.tiles[i - 1], move) && check_tile_move(grid.tiles[i], move)) {
                    move_tile(&grid.tiles[i], move);
                    updated = true;
                }
                break;
            case (CON_RIGHT):
                ASSERT(i % grid.width < grid.width - 1, "Impossible, tile cannot be connected to the right" );
                if (check_tile_move(grid.tiles[i + 1], move) && check_tile_move(grid.tiles[i], move)) {
                    move_tile(&grid.tiles[i], move);
                    updated = true;
                }
                break;
            default:
                if (check_tile_move(grid.tiles[i], move)) {
                    move_tile(&grid.tiles[i], move);
                    updated = true;
                }
                break;
        }
    }

    return updated;
}

void stop_player() {
    player_set();
    LOG("Player set");
    state.halt = true;
    player_clear();
    LOG("Cleared player");

    player.active = false;
}

static bool update_player_physics() {
    vec2i fall = {0, 1};
    if (check_player_in_grid(fall) && check_player_move(fall)) {
        player_move(fall);
        return true;
    } else {
        stop_player();
    }
    return false;
}

static void update_tick() {
    // varying tick speeds --> larger = slower
    if (state.halt)
        state.tick = 250;
    else if (state.clearing)
        state.tick = 400;
    else
        state.tick = 1000;

    if (SDL_GetTicks() >= state.time + state.tick) {
        update_tile_connections();

        state.time = SDL_GetTicks();

        // Scan for words only if not already scanned
        if (!state.clearing) {
            if (grid_scan_for_words()) {
                state.clearing = true;
                return;
            }
        } else {
            // Clear the board of marked files
            grid_clear_marked();
            state.clearing = false;
        }

        // TODO: don't clear words that are falling
        if (state.halt) {
            state.halt = update_world_physics();
            if (!state.halt) {
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

static void player_rotate_cw() {
    vec2i t1pos = player.t1.pos;
    vec2i t2pos = player.t2.pos;

    short t1con = player.t1.connected;
    short t2con = player.t2.connected;

    if (t1pos.x < t2pos.x) {
        LOG("cw rotation 1");
        t1pos.y -= 1;
        t2pos.x -= 1;
        t1con = CON_DOWN;
        t2con = CON_UP;
    } else if (t1pos.x == t2pos.x && t1pos.y < t2pos.y) {
        LOG("cw rotation 2");
        t1pos.y += 1;
        t1pos.x += 1;
        t1con = CON_LEFT;
        t2con = CON_RIGHT;
    } else if (t1pos.x > t2pos.x && t1pos.y == t2pos.y) {
        LOG("cw rotation 3");
        t1pos.x -= 1;
        t2pos.y -= 1;
        t1con = CON_UP;
        t2con = CON_DOWN;
    } else {
        LOG("cw rotation 4");
        t2pos.x += 1;
        t2pos.y += 1;
        t1con = CON_RIGHT;
        t2con = CON_LEFT;
    }

    if (t1pos.y < 0 || t2pos.y < 0) {
        return;
    }

    bool kick_check =
        (grid.tiles[at(t2pos.y, t2pos.x, grid.width)].filled &&
        !grid.tiles[at(t2pos.y, t2pos.x - 1, grid.width)].filled) || (t1pos.x >= grid.width) || (t2pos.x >= grid.width) ||
        (grid.tiles[at(t1pos.y, t1pos.x, grid.width)].filled &&
        !grid.tiles[at(t1pos.y, t1pos.x - 1, grid.width)].filled);

    if (kick_check) {
        LOG("kick left");
        t1pos.x -= 1;
        t2pos.x -= 1;
    }

    bool t1_check = (grid.tiles[at(t1pos.y, t1pos.x, grid.width)].filled || t1pos.x < 0 || t1pos.x >= grid.width);
    bool t2_check = (grid.tiles[at(t2pos.y, t2pos.x, grid.width)].filled || t2pos.x < 0 || t2pos.x >= grid.width);

    if (t1_check || t2_check) {
        return;
    }

    player.t1.pos = t1pos;
    player.t2.pos = t2pos;

    player.t1.connected = t1con;
    player.t2.connected = t2con;
}

static void player_rotate_ccw() {
    vec2i t1pos = player.t1.pos;
    vec2i t2pos = player.t2.pos;

    short t1con = player.t1.connected;
    short t2con = player.t2.connected;

    if (t1pos.x < t2pos.x) {
        LOG("ccw rotation 1");
        t1pos.x += 1;
        t2pos.y -= 1;
        t1con = CON_UP;
        t2con = CON_DOWN;
    } else if (t1pos.x == t2pos.x && t1pos.y > t2pos.y) {
        LOG("ccw rotation 2");
        t2pos.x -= 1;
        t2pos.y += 1;
        t1con = CON_LEFT;
        t2con = CON_RIGHT;
    } else if (t1pos.x > t2pos.x && t1pos.y == t2pos.y) {
        LOG("ccw rotation 3");
        t2pos.x += 1;
        t1pos.y -= 1;
        t1con = CON_DOWN;
        t2con = CON_UP;
    } else {
        LOG("ccw rotation 4");
        t1pos.y += 1;
        t1pos.x -= 1;
        t1con = CON_RIGHT;
        t2con = CON_LEFT;
    }

    if (t1pos.y < 0 || t2pos.y < 0) {
        return;
    }

    bool kick_check =
        (grid.tiles[at(t2pos.y, t2pos.x, grid.width)].filled &&
        !grid.tiles[at(t2pos.y, t2pos.x + 1, grid.width)].filled) || (t1pos.x < 0) || (t2pos.x < 0) ||
        (grid.tiles[at(t1pos.y, t1pos.x, grid.width)].filled &&
        !grid.tiles[at(t1pos.y, t1pos.x + 1, grid.width)].filled);

    if (kick_check) {
        LOG("kick right");
        t1pos.x += 1;
        t2pos.x += 1;
    }

    bool t1_move_failed =
        (grid.tiles[at(t1pos.y, t1pos.x, grid.width)].filled ||
        t1pos.x < 0 || t1pos.x >= grid.width);
    bool t2_move_failed =
        (grid.tiles[at(t2pos.y, t2pos.x, grid.width)].filled ||
        t2pos.x < 0 || t2pos.x >= grid.width);

    if (t1_move_failed || t2_move_failed) {
        return;
    }

    player.t1.pos = t1pos;
    player.t2.pos = t2pos;

    player.t1.connected = t1con;
    player.t2.connected = t2con;
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
                LOG("\nTILE %d:\n.connected='%d',\n.letter='%c',\n.pos=(%d, %d),\n.filled=%d,\n.marked=%d",
                    tile_index, t.connected, t.letter, t.pos.x, t.pos.y, t.filled, t.marked);
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
                            player_move(move);
                        break;
                    }
                    case SDLK_d:
                    case SDLK_RIGHT: {
                        vec2i move = (vec2i){1, 0};
                        if (player_check_movement(move))
                            player_move(move);
                        break;
                    }
                    case SDLK_s:
                    case SDLK_DOWN: {
                        if (!state.halt && !state.clearing) {
                            vec2i move = (vec2i){0, 1};
                            if (player_check_movement(move)) {
                                state.time = SDL_GetTicks();
                                player_move(move);
                            }
                            else {
                                stop_player();
                            }
                        }
                        break;
                    }
                    case SDLK_k:
                        player_rotate_cw();
                        break;
                    case SDLK_j:
                        player_rotate_ccw();
                        break;
                    case SDLK_w:
                    case SDLK_UP:
                        player_flip();
                        break;
                    case SDLK_SPACE:
                        player_set_greyed();
                        stop_player();
                        // grid_clear_marked();
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
    int w = TILE_SIZE * 3;
    int h = TILE_SIZE * 2;

    queue.obj = (obj_info_t){
        .size = {w, h},
        .pos = {x, y},
        .sprite = &sprites[26],
    };
    queue_enqueue();
    LOG("Queue created");
}

static void sdl_init() {
    ASSERT(!SDL_Init(SDL_INIT_VIDEO),
           "SDL failed to initialize: %s\n", SDL_GetError());

    state.window = SDL_CreateWindow("Wordtris", SDL_WINDOWPOS_CENTERED_DISPLAY(1), SDL_WINDOWPOS_CENTERED_DISPLAY(1), SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_ALLOW_HIGHDPI);

    ASSERT(state.window, "Failed to create SDL Window: %s\n", SDL_GetError());

    state.renderer = SDL_CreateRenderer(state.window, -1, SDL_RENDERER_PRESENTVSYNC);

    ASSERT(state.renderer, "Failed to create SDL Renderer: %s\n", SDL_GetError());

    state.texture = SDL_CreateTexture(state.renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING, SCREEN_WIDTH, SCREEN_HEIGHT);

    ASSERT(state.renderer, "Failed to create SDL Texture: %s\n", SDL_GetError());
}

static void game_init() {
    srand(time(NULL));
    load_sprites();

    state.dict_trie = trie_node_create();
    trie_construct(state.dict_trie, "./dictionary.txt");

    lpool_init(&state.letter_pool);
    lpool_populate(&state.letter_pool);

    grid_init(8, 10);
    grid_randomize_grey_tiles();

    queue_init();
    spawn_player();
    queue_enqueue();
}

int main(int argc, char *argv[]) {

    sdl_init();
    game_init();

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
