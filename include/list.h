#ifndef LIST_H
#define LIST_H

#include <types.h>
#include <stddef.h>
#include <typedef.h>

// This structure will represent both a list and a list node. This implies that the list 
// will be circular
struct List {
    struct List* next;
    struct List* prev;
};

// Returns a pointer to the struct entry in which the list is embedded
#define list_to_struct(node, type, member) \
    (type *)((u8 *)node - offsetof(type, member))

// Initializes a list
static inline void list_init(struct List* list) {
    list->prev = list;
    list->next = list;
}

static inline void list_node_init(struct List* list) {
    // TODO: 0 trap
    list->next = 0;
    list->prev = 0;
}

// Inserts a list node between two consecutive entries
static inline void __list_add(struct List* new, struct List* prev,
    struct List* next) {
    prev->next = new;
    next->prev = new;
    new->next = next;
    new->prev = prev;
}

// Delets a node from the list between two consecutive entries
static inline void __list_delete(struct List* prev, struct List* next) {
    prev->next = next;
    next->prev = prev;
}

// Inserts a list node first in the list
static inline void list_add_first(struct List* new, struct List* list) {
    __list_add(new, list, list->next);
}

// Inserts a list node last in the list
static inline void list_add_last(struct List* new, struct List* list) {
    __list_add(new, list->prev, list);
}

static inline void list_add_before(struct List* new, struct List* before) {
    __list_add(new, before->prev, before);
}

// Deletes a node in the list
static inline void list_remove(struct List* node) {
    __list_delete(node->prev, node->next);

    // TODO: add MPU protected address, issue #50 
    node->next = 0;
    node->prev = 0;
}

// Deletes the first node in the list
static inline struct List* list_remove_first(struct List* list) {
    if (list->next == list) {
        return 0;
    }
    struct List* node = list->next;
    list_remove(list->next);
    return node;
}

// Deletes the last node in the list
static inline struct List* list_remove_last(struct List* list) {
    if (list->prev == list) {
        return 0;
    }
    struct List* node = list->prev;
    list_remove(list->prev);
    return node;
}

static inline u8 list_is_empty(struct List* list) {
    return (list->next == list) ? 1 : 0;
}

#define list_get_first(list) ((list)->next)
#define list_get_last(list) ((list)->prev)

// Defines for iterating lists
#define list_iterate(node, list) \
    for (node = (list)->next; node != (list); node = node->next)

#define list_iterate_reverse(node, list) \
    for (node = (list)->prev; node != (list); node = node->prev)

static inline u32 list_get_size(struct List* list) {
    u32 size = 0;
    
    struct List* node;
    list_iterate(node, list) {
        size++;
    }
    return size;
}

// Merges list src into list dest
static inline void list_merge(struct List* src, struct List* dest) {
    src->prev->next = dest->next;
    dest->next->prev = src->prev;

    dest->next = src->next;
    dest->next->prev = dest;
}

#endif
