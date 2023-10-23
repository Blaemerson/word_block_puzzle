#pragma once

#define ASSERT(_e, ...) if (!(_e)) { fprintf(stderr, "[ERROR] %s %d: ", __FILE__, __LINE__); fprintf(stderr, __VA_ARGS__);  exit(1); }
#define LOG(...) do { fprintf(stderr, "[LOG] %s %d: ", __FILE__, __LINE__); fprintf(stderr, __VA_ARGS__); printf("\n"); } while (0)

#define SCREEN_WIDTH 1080
#define SCREEN_HEIGHT 720

