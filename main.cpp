//
// Created by Ayman Barham on 21/01/2023.
//

#include "malloc_3.h"
#include <unistd.h>

#define REQUIRE(c) ((c)? printf("success\n") : printf("fail\n"))
#define MAX_ALLOCATION_SIZE (1e8)
#define MMAP_THRESHOLD (128 * 1024)
#define MIN_SPLIT_SIZE (128)

static inline size_t aligned_size(size_t size)
{
    return size;
}

#define verify_blocks(allocated_blocks, allocated_bytes, free_blocks, free_bytes)                                      \
    do                                                                                                                 \
    {                                                                                                                  \
        REQUIRE(_num_allocated_blocks() == allocated_blocks);                                                          \
        REQUIRE(_num_allocated_bytes() == aligned_size(allocated_bytes));                                              \
        REQUIRE(_num_free_blocks() == free_blocks);                                                                    \
        REQUIRE(_num_free_bytes() == aligned_size(free_bytes));                                                        \
        REQUIRE(_num_meta_data_bytes() == aligned_size(_size_meta_data() * allocated_blocks));                         \
    } while (0)

#define verify_size(base)                                                                                              \
    do                                                                                                                 \
    {                                                                                                                  \
        void *after = sbrk(0);                                                                                         \
        REQUIRE(_num_allocated_bytes() + aligned_size(_size_meta_data() * _num_allocated_blocks()) ==                  \
                (size_t)after - (size_t)base);                                                                         \
    } while (0)

#define verify_size_with_large_blocks(base, diff)                                                                      \
    do                                                                                                                 \
    {                                                                                                                  \
        void *after = sbrk(0);                                                                                         \
        REQUIRE(diff == (size_t)after - (size_t)base);                                                                 \
    } while (0)

template <typename T>
void populate_array(T *array, size_t len)
{
    for (size_t i = 0; i < len; i++)
    {
        array[i] = (T)i;
    }
}

template <typename T>
void validate_array(T *array, size_t len)
{
    for (size_t i = 0; i < len; i++)
    {
        REQUIRE((array[i] == (T)i));
    }
}


int main()
{
    verify_blocks(0, 0, 0, 0);
    void *base = sbrk(0);
    char *pad1 = (char *)smalloc(32);
    char *a = (char *)smalloc(32);
    char *b = (char *)smalloc(32);
    char *c = (char *)smalloc(32);
    REQUIRE(pad1 != nullptr);
    REQUIRE(a != nullptr);
    REQUIRE(b != nullptr);
    REQUIRE(c != nullptr);

    size_t pad_size = 32;

    verify_blocks(4, 32 * 3 + pad_size, 0, 0);
    verify_size(base);
    populate_array(b, 32);

    sfree(c);
    verify_blocks(4, 32 * 3 + pad_size, 1, 32);
    verify_size(base);

    char *new_b = (char *)srealloc(b, 32 * 3 + _size_meta_data());
    REQUIRE(new_b != nullptr);
    REQUIRE(new_b == b);
    verify_blocks(3, 32 * 4 + _size_meta_data() + pad_size, 0, 0);
    verify_size(base);
    validate_array(new_b, 32);

    sfree(new_b);
    verify_blocks(3, 32 * 4 + _size_meta_data() + pad_size, 1, 32 * 3 + _size_meta_data());
    verify_size(base);

    sfree(a);
    verify_blocks(2, 32 * 4 + 2 * _size_meta_data() + pad_size, 1, 32 * 4 + 2 * _size_meta_data());
    verify_size(base);

    sfree(pad1);
    verify_blocks(1, 32 * 4 + 3 * _size_meta_data() + pad_size, 1, 32 * 4 + 3 * _size_meta_data() + pad_size);
    verify_size(base);
    return 0;
}