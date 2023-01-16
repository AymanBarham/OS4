//
// Created by Ayman Barham on 16/01/2023.
//
#include <unistd.h>

void* smalloc(size_t size){
    if(size == 0 || size > 100000000){
        return NULL;
    }

    void * ptr = sbrk(size);
    if(ptr == (void*)-1){
        return NULL;
    }

    return ptr;
}