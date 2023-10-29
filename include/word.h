#pragma once
#include "types.h"
#include "trie.h"
#include "tile.h"
#include <stdbool.h>

// word is viable if it contains and vowel and a consonant
bool check_word_viability(char* word);

// check that a substring is the minimum length and contains no spaces.
const bool check_string_validity(const char* substring);

// check for words in a given row of characters
bool check_substrings(const char *str, u32 *indices, tile_t *tiles, usize grid_w, usize grid_h, struct TrieNode *dict);
