//
// Created by Ayman Barham on 21/01/2023.
//
#define REQUIRE(c) ((c) ? printf("success\n") : printf("fail\n"))
#define MIN_SPLIT_SIZE 128
#include "malloc_e.h"
int main()
{

    void *base = sbrk(0);
    int *a = (int *)srealloc(nullptr, 30 * sizeof(int));
    REQUIRE(a != nullptr);

    for (int i = 0; i < 10; i++)
    {
        a[i] = i;
    }


    int *b = (int *)srealloc(a, 10 * sizeof(int));
    REQUIRE(b != nullptr);
    REQUIRE(b == a);
    for (int i = 0; i < 10; i++)
    {
        REQUIRE(b[i] == i);
    }


    sfree(b);


    return 0;

}