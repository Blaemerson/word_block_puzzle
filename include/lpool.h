#pragma once

struct letter_node {
    char letter;
    int weight;
    struct letter_node* next;
};

struct letter_pool {
    struct letter_node* head;
    int totalWeight;
};

void lpool_init(struct letter_pool* pool);

void lpool_populate(struct letter_pool* pool);

void lpool_add_letter(struct letter_pool* pool, char letter, int weight);

char lpool_random_letter(struct letter_pool* pool);

void lpool_destroy(struct letter_pool* pool);

