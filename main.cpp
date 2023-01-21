//
// Created by Ayman Barham on 21/01/2023.
//
#define REQUIRE(c) ((c) ? printf("success\n") : printf("fail\n"))
#define MIN_SPLIT_SIZE 128
#include "malloc_e.h"
int main()
{
    void *base = sbrk(0);
    char *a = (char *)smalloc(16 + MIN_SPLIT_SIZE * 2 + _size_meta_data());
    REQUIRE(a != nullptr);

    sfree(a);

    char *b = (char *)smalloc(16);
    REQUIRE(b != nullptr);
    REQUIRE(b == a);


    sfree(b);


    return 0;

}