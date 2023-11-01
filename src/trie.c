#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../include/trie.h"
#include "../include/tile.h"
#include "../include/macros.h"

struct dict_trie_node* trie_node_create() {
    struct dict_trie_node* node = (struct dict_trie_node*)malloc(sizeof(struct dict_trie_node));

    for (int i = 0; i < MAX_CHILDREN; i++) {
        node->children[i] = NULL;
    }

    node->is_end_of_word = false;

    return node;
}

void trie_destroy(struct dict_trie_node* node) {
    if (node == NULL) {
        return;
    }

    for (int i = 0; i < 26; i++) {
        trie_destroy(node->children[i]);
    }

    free(node);
}

void trie_insert_word(struct dict_trie_node* root, const char* word) {
    struct dict_trie_node* curr = root;

    for (int i = 0; word[i] != '\0'; i++) {
        int index = word[i] - 'a'; // Convert character to index (assuming only lowercase alphabets)

        if (curr->children[index] == NULL) {
            curr->children[index] = trie_node_create();
        }

        curr = curr->children[index];
    }

    curr->is_end_of_word = true;
}

int trie_construct(struct dict_trie_node* root, const char* dict_file) {
    LOG("Construct dict trie");
    FILE* file = fopen(dict_file, "r");

    ASSERT(file, "unable to open dictionary file %s", dict_file);

    char word[10];

    while (fgets(word, sizeof(word), file) != NULL) {
        // Remove newline character from the word, if present
        word[strcspn(word, "\n")] = '\0';

        trie_insert_word(root, word);
    }

    fclose(file);

    return 0;
}

bool trie_search_word(struct dict_trie_node* root, const char* word) {
    struct dict_trie_node* curr = root;

    for (int i = 0; word[i] != '\0'; i++) {
        int index = word[i] - 'a'; // Convert character to index (assuming only lowercase alphabets)

        if (curr->children[index] == NULL) {
            return false; // Word does not exist
        }

        curr = curr->children[index];
    }

    return (curr != NULL && curr->is_end_of_word);
}


// word is viable if it contains and vowel and a consonant
const bool check_word_viability(char* word) {
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
const bool check_string_validity(const char* substring) {
    const int length = strlen(substring);

    for (int i = 0; i < length; i++) {
        if (substring[i] == ' ') {
            return false;
        }
    }

    if (length < 3) {
        return false;
    }

    return true;
}


// check for words in a given row of characters
const bool check_substrings(
    const char* str,
    u32* indices,
    tile_t *tiles,
    usize grid_w,
    usize grid_h,
    struct dict_trie_node *dict)
{
    int len = strlen(str);
    int max_len = len < grid_h ? len : grid_h;

    int longest = 0;
    int index_start = -1;
    int index_end = -1;

    bool found = false;

    for (int sub_len = max_len; sub_len >= 3; sub_len--) {
        for (int i = 0; i <= len - sub_len; i++) {
            char substr[sub_len + 1];
            strncpy(substr, str + i, sub_len);
            substr[sub_len] = '\0';


            if (check_word_viability(substr) && check_string_validity(substr)) {
                if (trie_search_word(dict, substr)) {
                    if (sub_len > longest) {
                        longest = sub_len;
                        index_start = i;
                        index_end = i + sub_len;
                        found = true;
                    }

                    break;
                }
            }
        }
    }

    for (int x = index_start; x < index_end; x++) {
        // IFDEBUG_LOG("Marked %d", indices[x]);
        tiles[indices[x]].marked = true;
    }

    return found;
}
