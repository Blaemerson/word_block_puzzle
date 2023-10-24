#pragma once

struct LetterNode {
    char letter;
    int weight;
    struct LetterNode* next;
};

struct LetterPool {
    struct LetterNode* head;
    int totalWeight;
};

void lpool_init(struct LetterPool* pool);

void lpool_populate(struct LetterPool* pool);

void lpool_add_letter(struct LetterPool* pool, char letter, int weight);

char lpool_random_letter(struct LetterPool* pool);

void lpool_destroy(struct LetterPool* pool);

