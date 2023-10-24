#include <stdbool.h>
#define MAX_CHILDREN 26

struct TrieNode {
    struct TrieNode* children[MAX_CHILDREN]; // Array to store child nodes for each letter (assuming only lowercase alphabets)
    bool is_end_of_word; // Flag to mark the end of a word
};

struct TrieNode* trie_node_create();

void trie_destroy(struct TrieNode* node);
void trie_insert_word(struct TrieNode* root, const char* word);
int trie_construct(struct TrieNode* root, const char* dict_file);
bool trie_search_word(struct TrieNode* root, const char* word);

