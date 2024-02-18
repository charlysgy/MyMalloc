#include "my_malloc.h"

#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

static struct blk_meta *meta = NULL;

static void merge_mem(struct blk_meta *ptr)
{
    struct blk_meta *prev = ptr->prev
            && (ptr->prev->status == FREE
                || ptr->prev->status == IS_STARTING_PAGE_FREE)
        ? ptr->prev
        : NULL;
    if (ptr->status == IS_STARTING_PAGE_FREE)
        prev = NULL;
    struct blk_meta *next = ptr->next
            && (ptr->next->status == FREE
                || ptr->next->status == IS_STARTING_PAGE_FREE)
        ? ptr->next
        : NULL;

    if (prev)
    {
        prev->size += sizeof(struct blk_meta) + ptr->size;
        prev->next = ptr->next;
        ptr->next = NULL;
        if (next)
        {
            prev->size += sizeof(struct blk_meta) + next->size;
            prev->next = next->next;
            ptr->next = NULL;
        }
        if (prev->next)
            prev->next->prev = prev;
    }
    else if (next)
    {
        ptr->size += sizeof(struct blk_meta) + next->size;
        ptr->next = next->next;
        if (ptr->next)
            ptr->next->prev = ptr;
    }
}

void my_free(void *ptr)
{
    if (!ptr)
        return;

    struct blk_meta *tmp = meta;
    while (tmp)
    {
        if (tmp->data == ptr)
            break;
        tmp = tmp->next;
    }

    if (!tmp || tmp->status == FREE)
        return;

    if (tmp->status == IS_STARTING_PAGE)
        tmp->status = IS_STARTING_PAGE_FREE;
    else
        tmp->status = FREE;

    merge_mem(tmp);

    if (tmp->status == IS_STARTING_PAGE_FREE && tmp->next == NULL)
    {
        munmap(tmp, tmp->size + sizeof(struct blk_meta));
        meta = NULL;
    }
    else if (tmp->status == IS_STARTING_PAGE_FREE
             && tmp->next->status == IS_STARTING_PAGE)
    {
        if (tmp->prev)
            tmp->prev->next = tmp->next;
        if (tmp->next)
            tmp->next->prev = tmp->prev;
        if (meta == tmp)
            meta = tmp->next;
        munmap(tmp, tmp->size + sizeof(struct blk_meta));
    }
}

static size_t align(size_t size, size_t base)
{
    size_t rem = size % base;
    size_t fact = size / base;
    fact = rem > 0 ? fact + 1 : fact;
    if (__builtin_umull_overflow(fact, base, &size))
        return 0;

    return size;
}

static struct blk_meta *find_free(size_t size)
{
    struct blk_meta *tmp = meta;

    while (tmp)
    {
        if ((tmp->status == FREE || tmp->status == IS_STARTING_PAGE_FREE)
            && tmp->size >= size)
            return tmp;
        tmp = tmp->next;
    }

    return NULL;
}

static void split_data(struct blk_meta *blk, size_t needed_size);

static void *add_blk_at_end(size_t size)
{
    size = align(size, sizeof(long double));
    struct blk_meta *tmp = meta;

    while (tmp->next)
        tmp = tmp->next;

    size_t map_size =
        align(size + sizeof(struct blk_meta), sysconf(_SC_PAGESIZE));
    struct blk_meta *new = mmap(NULL, map_size, PROT_READ | PROT_WRITE,
                                MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (new == MAP_FAILED)
        return NULL;

    new->size = map_size - sizeof(struct blk_meta);
    new->status = IS_STARTING_PAGE;
    new->prev = tmp;
    tmp->next = new;
    new->next = NULL;

    if (size <= new->size - sizeof(struct blk_meta) - sizeof(long double))
        split_data(new, size);
    void *data = new->data;
    return data;
}

static void split_data(struct blk_meta *blk, size_t needed_size)
{
    size_t new_blk_size = align(needed_size, sizeof(long double));

    void *tmp = blk->data + new_blk_size;
    struct blk_meta *new = tmp;
    new->size = blk->size - sizeof(struct blk_meta) - new_blk_size;
    new->status = FREE;
    new->prev = blk;
    new->next = blk->next;
    blk->next = new;
    blk->size = new_blk_size;
}

void *my_malloc(size_t size)
{
    size = align(size, sizeof(long double));
    if (!meta)
    {
        size_t map_size =
            align(size + sizeof(struct blk_meta), sysconf(_SC_PAGESIZE));
        meta = mmap(NULL, map_size, PROT_READ | PROT_WRITE,
                    MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (meta == MAP_FAILED)
            return NULL;
        meta->size = map_size - sizeof(struct blk_meta);
        meta->status = IS_STARTING_PAGE;
        meta->prev = NULL;
        meta->next = NULL;
        if (size <= meta->size - sizeof(struct blk_meta) - sizeof(long double))
            split_data(meta, size);
        void *tmp = meta->data;
        return tmp;
    }

    struct blk_meta *res = find_free(size);
    if (!res)
        return add_blk_at_end(size);

    if (size <= res->size - sizeof(struct blk_meta) - sizeof(long double))
        split_data(res, size);

    res->status = NOTFREE;
    void *ptr = res->data;
    return ptr;
}

void *my_calloc(size_t nmemb, size_t size)
{
    if (__builtin_umull_overflow(nmemb, size, &size))
        return NULL;
    void *ptr = my_malloc(size);
    if (!ptr)
        return NULL;
    memset(ptr, '\0', size);
    return ptr;
}

static struct blk_meta *get_prev(struct blk_meta *blk)
{
    struct blk_meta *prev = blk->prev
            && (blk->prev->status == FREE
                || (blk->status < IS_STARTING_PAGE
                    && blk->prev->status == IS_STARTING_PAGE_FREE))
        ? blk->prev
        : NULL;

    return prev;
}

static struct blk_meta *neighbors_free(struct blk_meta *blk, size_t size)
{
    struct blk_meta *prev = get_prev(blk);
    struct blk_meta *next =
        blk->next && (blk->next->status == FREE) ? blk->next : NULL;

    if (prev && prev->size + blk->size + sizeof(struct blk_meta) >= size)
    {
        prev->next = blk->next;
        if (blk->next)
            blk->next->prev = prev;
        prev->size += blk->size + sizeof(struct blk_meta);
        memcpy(prev->data, blk->data, blk->size);
        return prev;
    }
    if (next && next->size + blk->size + sizeof(struct blk_meta) >= size)
    {
        blk->next = next->next;
        if (blk->next)
            blk->next->prev = blk;
        blk->size += sizeof(struct blk_meta) + next->size;
        return blk;
    }
    if (prev && next
        && blk->size + 2 * sizeof(struct blk_meta) + prev->size + next->size
            >= size)
    {
        prev->next = next->next;
        if (next->next)
            next->next->prev = prev;
        prev->size += blk->size + next->size + 2 * sizeof(struct blk_meta);
        memcpy(prev->data, blk->data, blk->size);
        return prev;
    }

    return NULL;
}

void *my_realloc(void *ptr, size_t size)
{
    size = align(size, sizeof(long double));
    if (!ptr)
        return my_malloc(size);

    if (size == 0)
    {
        my_free(ptr);
        return NULL;
    }
    struct blk_meta *tmp = meta;
    while (tmp)
    {
        if (tmp->data == ptr)
            break;
        tmp = tmp->next;
    }

    if (!tmp)
        return NULL;

    if (tmp->size >= size)
    {
        if (size <= tmp->size - sizeof(struct blk_meta) - sizeof(long double))
            split_data(tmp, size);
        return tmp->data;
    }

    struct blk_meta *new = neighbors_free(tmp, size);
    if (new)
    {
        if (size <= new->size - sizeof(struct blk_meta) - sizeof(long double))
            split_data(new, size);
        if (new->status >= IS_STARTING_PAGE)
            new->status = IS_STARTING_PAGE;
        else
            new->status = NOTFREE;
        return new->data;
    }

    my_free(tmp);
    new = my_malloc(size);
    memcpy(new, tmp->data, tmp->size);

    return new->data;
}
