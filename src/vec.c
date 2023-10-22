
#include "../include/vec.h"

vec2i vec2_create(int x, int y) {
    return (vec2i){x, y};
}

vec2i vector_add(vec2i v0, vec2i v1) {
    return (vec2i){v0.x + v1.x, v0.y + v1.y};
}

vec2i vector_add_to_all(vec2i v0, int c) {
    return (vec2i){v0.x + c, v0.y + c};
}

vec2i vector_scale(vec2i v, int s) {
    return (vec2i){v.x * s, v.y * s};
}
