#define ARENA_IMPLEMENTATION
#include "arena.h"
#include <stddef.h>

// Each block starts with a header that links it into a circular list
// headed by a sentinel. The union keeps the data after the header
// aligned the way malloc's own result is.
struct arena_link {
    struct arena_link* prev;
    struct arena_link* next;
};

union arena_header {
    struct arena_link link;
    max_align_t       align;
};

static struct arena_link arena_list = { &arena_list, &arena_list };

static void arena_link_block(struct arena_link* link){
    link->prev = &arena_list;
    link->next = arena_list.next;
    arena_list.next->prev = link;
    arena_list.next = link;
}

static void arena_unlink_block(struct arena_link* link){
    link->prev->next = link->next;
    link->next->prev = link->prev;
}

static union arena_header* arena_header_of(void* ptr){
    return (union arena_header*)ptr - 1;
}

void* arena_malloc(size_t size){
    if(size > (size_t)-1 - sizeof(union arena_header)){
        return NULL;
    }
    union arena_header* header = malloc(sizeof(union arena_header) + size);
    if(!header){
        return NULL;
    }
    arena_link_block(&header->link);
    return header + 1;
}

void* arena_calloc(size_t count, size_t size){
    if(size && count > ((size_t)-1 - sizeof(union arena_header)) / size){
        return NULL;
    }
    void* ptr = arena_malloc(count * size);
    if(ptr){
        memset(ptr, 0, count * size);
    }
    return ptr;
}

void* arena_realloc(void* ptr, size_t size){
    if(!ptr){
        return arena_malloc(size);
    }
    if(size > (size_t)-1 - sizeof(union arena_header)){
        return NULL;
    }
    union arena_header* header = arena_header_of(ptr);
    arena_unlink_block(&header->link);
    union arena_header* moved = realloc(header, sizeof(union arena_header) + size);
    if(!moved){
        arena_link_block(&header->link);
        return NULL;
    }
    arena_link_block(&moved->link);
    return moved + 1;
}

void arena_free(void* ptr){
    if(!ptr){
        return;
    }
    union arena_header* header = arena_header_of(ptr);
    arena_unlink_block(&header->link);
    free(header);
}

char* arena_strdup(const char* str){
    size_t size = strlen(str) + 1;
    char* copy = arena_malloc(size);
    if(copy){
        memcpy(copy, str, size);
    }
    return copy;
}

void arena_free_all(void){
    while(arena_list.next != &arena_list){
        struct arena_link* link = arena_list.next;
        arena_unlink_block(link);
        free(link);
    }
}
