#include "tile.h"
#include "types.h"
#define MAX_CHILDREN 26

struct dict_trie_node {
    struct dict_trie_node* children[MAX_CHILDREN]; // Array to store child nodes for each letter (assuming only lowercase alphabets)
    bool is_end_of_word; // Flag to mark the end of a word
};

struct dict_trie_node* trie_node_create();

// destroy the trie
void trie_destroy(struct dict_trie_node* node);

// insert a word into the trie
void trie_insert_word(struct dict_trie_node* root, const char* word);

// search a trie from the dictionary file
int trie_construct(struct dict_trie_node* root, const char* dict_file);

// search the trie for a word
bool trie_search_word(struct dict_trie_node* root, const char* word);

// word is viable if it contains and vowel and a consonant
const bool check_word_viability(char* word);

// check that a substring is the minimum length and contains no spaces.
const bool check_string_validity(const char* substring);

// check for words in a given row of characters
const bool check_substrings(const char *str, u32 *indices, tile_t *tiles, usize grid_w, usize grid_h, struct dict_trie_node *dict);
