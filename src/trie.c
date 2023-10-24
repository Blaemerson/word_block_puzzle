#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../include/trie.h"
#include "../include/macros.h"

struct TrieNode* trie_node_create() {
    struct TrieNode* node = (struct TrieNode*)malloc(sizeof(struct TrieNode));

    for (int i = 0; i < MAX_CHILDREN; i++) {
        node->children[i] = NULL;
    }

    node->is_end_of_word = false;

    return node;
}

void trie_destroy(struct TrieNode* node) {
    if (node == NULL) {
        return;
    }

    for (int i = 0; i < 26; i++) {
        trie_destroy(node->children[i]);
    }

    free(node);
}

void trie_insert_word(struct TrieNode* root, const char* word) {
    struct TrieNode* curr = root;

    for (int i = 0; word[i] != '\0'; i++) {
        int index = word[i] - 'a'; // Convert character to index (assuming only lowercase alphabets)

        if (curr->children[index] == NULL) {
            curr->children[index] = trie_node_create();
        }

        curr = curr->children[index];
    }

    curr->is_end_of_word = true;
}

int trie_construct(struct TrieNode* root, const char* dict_file) {
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

bool trie_search_word(struct TrieNode* root, const char* word) {
    struct TrieNode* curr = root;

    for (int i = 0; word[i] != '\0'; i++) {
        int index = word[i] - 'a'; // Convert character to index (assuming only lowercase alphabets)

        if (curr->children[index] == NULL) {
            return false; // Word does not exist
        }

        curr = curr->children[index];
    }

    return (curr != NULL && curr->is_end_of_word);
}


