
#include <string.h>

#include "../include/word.h"

// word is viable if it contains and vowel and a consonant
bool check_word_viability(char* word) {
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
bool check_substrings(
    const char* str,
    u32* indices,
    tile_t *tiles,
    usize grid_w,
    usize grid_h,
    struct TrieNode *dict)
{
    int len = strlen(str);
    int maxLen = len < grid_h ? len : grid_h;

    for (int subLen = maxLen; subLen >= 3; subLen--) {
        for (int i = 0; i <= len - subLen; i++) {
            char substr[subLen + 1];
            strncpy(substr, str + i, subLen);
            substr[subLen] = '\0';


            if (check_word_viability(substr) && check_string_validity(substr)) {
                if (trie_search_word(dict, substr)) {
                    int indexStart = i;
                    int indexEnd = i + subLen;

                    for (int x = indexStart; x < indexEnd; x++) {
                        // IFDEBUG_LOG("Marked %d", indices[x]);
                        tiles[indices[x]].marked = true;
                    }
                    // IFDEBUG_LOG("WORD FOUND: %s", substr);

                    return true;
                }
            }
        }
    }

    return false;
}
