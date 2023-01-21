//
// Created by Ayman Barham on 21/01/2023.
//

#ifndef OS4_MALLOC_E_H
#define OS4_MALLOC_E_H

#endif //OS4_MALLOC_E_H
#include <unistd.h>
#include <iostream>
#include <cstring>
#include <assert.h>
#include <sys/mman.h>

void *smalloc(size_t size);
void sfree(void *p);
size_t _size_meta_data();
