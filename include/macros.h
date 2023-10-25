#pragma once

#define ASSERT(_e, ...) if (!(_e)) { fprintf(stderr, "[ERROR] %s %d: ", __FILE__, __LINE__); fprintf(stderr, __VA_ARGS__);  exit(1); }
#define LOG(...) do { fprintf(stderr, "[LOG] %s %d: ", __FILE__, __LINE__); fprintf(stderr, __VA_ARGS__); printf("\n"); } while (0)

#define SCREEN_WIDTH 1080
#define SCREEN_HEIGHT 720

#define TILE_SIZE 64
#define GAMEBOARD_WIDTH 8
#define GAMEBOARD_HEIGHT 10
#define GAMEBOARD_MAX (GAMEBOARD_WIDTH * GAMEBOARD_HEIGHT)
#define GAMEBOARD_OFFSET_X (SCREEN_WIDTH / 2) - ((GAMEBOARD_WIDTH * TILE_SIZE) / 2)


