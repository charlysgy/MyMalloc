#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

struct blk_meta
{
    size_t size;
    char status;
    struct blk_meta *prev;
    struct blk_meta *next;
    char data[];
};

int main(void)
{
    int *arr1 = malloc(50 * sizeof(int));
    int *arr2 = calloc(200, sizeof(int));
    int *arr3 = malloc(50 * sizeof(int));

    void *ptr1 = arr1;
    void *ptr2 = arr2;
    void *ptr3 = arr3;

    char *char_1 = ptr1;
    char *char_2 = ptr2;
    char *char_3 = ptr3;

    char_1 -= sizeof(struct blk_meta);
    char_2 -= sizeof(struct blk_meta);
    char_3 -= sizeof(struct blk_meta);

    struct blk_meta *blk_arr_1 = (struct blk_meta *)char_1;
    struct blk_meta *blk_arr_2 = (struct blk_meta *)char_2;
    struct blk_meta *blk_arr_3 = (struct blk_meta *)char_3;

    free(arr1);
    free(arr3);
    arr2 = realloc(arr2, 300 * sizeof(int));
    if (arr2 != arr1)
        return 2;
    free(arr2);
    return 0;
}
