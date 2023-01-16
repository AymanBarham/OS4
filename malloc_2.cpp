//
// Created by Ayman Barham on 16/01/2023.
//
#include <unistd.h>
#include <iostream>
#include <cstring>


// meta data containing linked list of information about allocated block
struct mallocMetadata_t
{
    size_t size;
    bool is_free;
    mallocMetadata_t *next;
    mallocMetadata_t *prev;
};

typedef mallocMetadata_t *MallocMetadata;


// first and last allocated blocks
MallocMetadata head = NULL;
MallocMetadata tail = NULL;


size_t _num_free_blocks()
{
    size_t counter = 0;

    for (MallocMetadata iter = head; iter; iter = iter->next)
    {
        if (iter->is_free)
        {
            counter++;
        }
    }
    return counter;
}

size_t _num_free_bytes()
{
    size_t counter = 0;
    for (MallocMetadata iter = head; iter; iter = iter->next)
    {
        if (iter->is_free)
        {
            counter += iter->size;
        }
    }
    return counter;
}

size_t _num_allocated_blocks()
{
    size_t counter = 0;

    for (MallocMetadata iter = head; iter; iter = iter->next)
    {
        counter++;
    }

    return counter;
}

size_t _num_allocated_bytes()
{
    size_t counter = 0;

    for (MallocMetadata iter = head; iter; iter = iter->next)
    {
        counter += iter->size;
    }

    return counter;
}

size_t _size_meta_data()
{
    return (sizeof(struct mallocMetadata_t));
}

size_t _num_meta_data_bytes()
{
    return _num_allocated_blocks() * _size_meta_data();
}

void *smalloc(size_t size)
{
    if (size == 0 || size > 100000000)
    {
        return NULL;
    }

    void *allocated_ptr;

    // empty list
    if (!head)
    {
        head = (MallocMetadata) sbrk(_size_meta_data());
        if (head == (void*) -1)
        {
            return NULL;
        }
        head->size = size;
        head->prev = NULL;
        head->next = NULL;
        head->is_free = false;
        tail = head;

        allocated_ptr = sbrk(size);
        if (allocated_ptr == (void *) -1) // syscall failed
        {
            return NULL;
        }

        // successful first allocation
        return allocated_ptr;
    }

    MallocMetadata iter;
    // not first allocation
    for (iter = head; iter; iter = iter->next)
    {
        if (iter->is_free && iter->size >= size)
        {
            // found first fit block
            break;
        }
    }

    if (iter) // found fit really
    {
        iter->is_free = false;
        return (void *) ((size_t) iter + _size_meta_data());
    }

    // must increase size of heap
    MallocMetadata allocated_metadata = (MallocMetadata) sbrk(_size_meta_data());
    if (allocated_metadata == (void *) -1)
    {
        return NULL;
    }
    allocated_metadata->next = NULL;
    allocated_metadata->is_free = false;
    allocated_metadata->prev = tail;
    tail->next = allocated_metadata;
    allocated_metadata->size = size;

    tail = allocated_metadata;

    allocated_ptr = sbrk(size);
    if (allocated_ptr == (void *) -1)
    {
        return NULL;
    }
    return allocated_ptr;

}

void *scalloc(size_t num, size_t size)
{
    if (size == 0 || num * size > 100000000)
    {
        return NULL;
    }

    void *allocated_ptr;
    allocated_ptr = smalloc(num * size);
    if (!allocated_ptr)
    {
        return NULL;
    }

    /* Set all bytes to zero */
    std::memset(allocated_ptr, 0, num * size);
    return allocated_ptr;
}

void sfree(void *p)
{
    if (!p)
    {
        return;
    }
    MallocMetadata metadata_ptr = (MallocMetadata) ((size_t) p - _size_meta_data());
    if (metadata_ptr->is_free)
    {
        return;
    }
    metadata_ptr->is_free = true;
}

void *srealloc(void *oldp, size_t size)
{
    if (size == 0 || size > 100000000)
    {
        return NULL;
    }

    void *allocated_ptr;
    if (!oldp) // NULL received
    {
        return smalloc(size);
    }

    MallocMetadata old_metadata = (MallocMetadata) ((size_t) oldp - _size_meta_data());

    if (size <= old_metadata->size)
    {
    	old_metadata->is_free = false;
        return oldp;
    }

    allocated_ptr = smalloc(size);
    if (!allocated_ptr)
    {
        return NULL;
    }

    std::memmove(allocated_ptr, oldp, size); // maybe bug here
    sfree(oldp);

    return allocated_ptr;
}
