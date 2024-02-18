#ifndef MY_MALLOC_H
#define MY_MALLOC_H

#include <stddef.h>

#define FREE 0
#define NOTFREE 1
#define IS_STARTING_PAGE 2
#define IS_STARTING_PAGE_FREE 3

struct blk_meta
{
    size_t size;
    char status;
    struct blk_meta *prev;
    struct blk_meta *next;
    char data[];
};

struct free_list
{
    struct blk_meta *next;
};

void *my_malloc(size_t size);

void my_free(void *ptr);

void *my_realloc(void *ptr, size_t size);

void *my_calloc(size_t nmemb, size_t size);

#endif /* !MY_MALLOC_H */
