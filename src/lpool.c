#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "../include/lpool.h"

void lpool_init(struct letter_pool* pool) {
    pool->head = NULL;
    pool->totalWeight = 0;
}

void lpool_add_letter(struct letter_pool* pool, char letter, int weight) {
    struct letter_node* newNode = (struct letter_node*)malloc(sizeof(struct letter_node));
    newNode->letter = letter;
    newNode->weight = weight;
    newNode->next = NULL;

    if (pool->head == NULL) {
        pool->head = newNode;
    } else {
        struct letter_node* current = pool->head;
        while (current->next != NULL) {
            current = current->next;
        }
        current->next = newNode;
    }

    pool->totalWeight += weight;
}

void lpool_populate(struct letter_pool* pool) {
    lpool_add_letter(pool, 'A', 8);
    lpool_add_letter(pool, 'B', 2);
    lpool_add_letter(pool, 'C', 3);
    lpool_add_letter(pool, 'D', 6);
    lpool_add_letter(pool, 'E', 10);
    lpool_add_letter(pool, 'F', 4);
    lpool_add_letter(pool, 'G', 4);
    lpool_add_letter(pool, 'H', 3);
    lpool_add_letter(pool, 'I', 8);
    lpool_add_letter(pool, 'J', 1);
    lpool_add_letter(pool, 'K', 2);
    lpool_add_letter(pool, 'L', 3);
    lpool_add_letter(pool, 'M', 3);
    lpool_add_letter(pool, 'N', 4);
    lpool_add_letter(pool, 'O', 10);
    lpool_add_letter(pool, 'P', 3);
    lpool_add_letter(pool, 'Q', 1);
    lpool_add_letter(pool, 'R', 3);
    lpool_add_letter(pool, 'S', 6);
    lpool_add_letter(pool, 'T', 4);
    lpool_add_letter(pool, 'U', 6);
    lpool_add_letter(pool, 'V', 1);
    lpool_add_letter(pool, 'W', 2);
    lpool_add_letter(pool, 'X', 1);
    lpool_add_letter(pool, 'Y', 2);
    lpool_add_letter(pool, 'Z', 1);
}

char lpool_random_letter(struct letter_pool* pool) {
    int randomWeight = rand() % pool->totalWeight;
    struct letter_node* current = pool->head;

    while (current != NULL) {
        randomWeight -= current->weight;
        if (randomWeight < 0) {
            return current->letter;
        }
        current = current->next;
    }

    // This should not be reached under normal circumstances
    return '\0';
}

void lpool_destroy(struct letter_pool* pool) {
    struct letter_node* current = pool->head;
    while (current != NULL) {
        struct letter_node* next = current->next;
        free(current);
        current = next;
    }
    pool->head = NULL;
    pool->totalWeight = 0;
}

