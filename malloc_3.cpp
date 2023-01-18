//
// Created by Ayman Barham on 16/01/2023.
//
//
// Created by Ayman Barham on 16/01/2023.
//
#include <unistd.h>
#include <iostream>
#include <cstring>
#include <assert.h>
#include <sys/mman.h>

#define TO_CUT_THRESHOLD 128
// meta data containing linked list of information about allocated block
struct mallocMetadata_t
{
    int cookie;
    size_t size;
    bool is_free;
    mallocMetadata_t *next;
    mallocMetadata_t *prev;

    mallocMetadata_t *next_free;
    mallocMetadata_t *prev_free;
};

typedef mallocMetadata_t *MallocMetadata;


// first and last allocated blocks
MallocMetadata head = NULL;
MallocMetadata tail = NULL;

MallocMetadata free_list_head = NULL;
MallocMetadata free_list_tail = NULL;



int random_cookie = rand();

void _check_for_overflow();
void _remove_from_free_list(MallocMetadata to_delete);
void _remove_from_block_list(MallocMetadata to_delete);
size_t _size_meta_data();
void _insert_to_free_list(MallocMetadata to_insert);
void _insert_to_block_list(MallocMetadata to_insert);
void _cut_if_needed(MallocMetadata to_cut, size_t wanted_size);


bool _is_free_block(MallocMetadata block)
{
    if (!block)
    {
        return false;
    }
    return block->is_free;
}

// can we assume that both neighbours of original block aren't free?
void _cut_if_needed(MallocMetadata to_cut, size_t wanted_size)
{
    // we assume block->size >= wanted_size
    if (!to_cut)
    {
        return;
    }
    if (to_cut->size - wanted_size - _size_meta_data() < TO_CUT_THRESHOLD)
    {
        // no need to cut
        return;
    }
    // there is need to cut
    MallocMetadata new_metadata = (MallocMetadata)((size_t)to_cut + _size_meta_data() + wanted_size);

    _insert_to_block_list(new_metadata);
    new_metadata->is_free = true;
    new_metadata->size = to_cut->size - wanted_size - _size_meta_data();

    _insert_to_free_list(new_metadata);


}
// with a given block we would like to create a new block that unites it and its neighbours
void _coalesce_free_blocks(MallocMetadata block)
{
    if (!block)
    {
        return;
    }

    MallocMetadata prev = block->prev;
    MallocMetadata next = block->next;

    MallocMetadata new_block = block;

    size_t total_size_of_coalesced_block = block->size;
    _remove_from_free_list(block);


    if (_is_free_block(next))
    {
        total_size_of_coalesced_block += next->size + _size_meta_data();
        _remove_from_free_list(next);
        _remove_from_block_list(next);
    }

    if (_is_free_block(prev))
    {
        total_size_of_coalesced_block += prev->size + _size_meta_data();
        _remove_from_free_list(prev);
        _remove_from_block_list(block);
        new_block = prev;
    }

    new_block->size = total_size_of_coalesced_block;
    _insert_to_free_list(new_block);
}
void _remove_from_block_list(MallocMetadata to_delete)
{
    if (!to_delete)
    {
        return;
    }

    MallocMetadata prev = to_delete->prev;
    MallocMetadata next = to_delete->prev;

    if (!prev) // is head of free list
    {
        head = next;
        if (next) // head but not tail
        {
            next->prev = NULL;
        } else { // head and tail
            tail = prev;
        }
        return;
    }
    if (!next) // is tail but not head of free list
    {
        tail = prev;
        prev->next = NULL;
        return;
    }
    // is somewhere in the middle
    prev->next = next;
    next->prev = prev;

    to_delete->next = NULL;
    to_delete->prev = NULL;
}
void _remove_from_free_list(MallocMetadata to_delete)
{
    if (!to_delete)
    {
        return;
    }

    MallocMetadata prev = to_delete->prev_free;
    MallocMetadata next = to_delete->next_free;

    if (!prev) // is head of free list
    {
        free_list_head = next;
        if (next) // head but not tail
        {
            next->prev_free = NULL;
        } else { // head and tail
            free_list_tail = prev;
        }
        return;
    }
    if (!next) // is tail but not head of free list
    {
        free_list_tail = prev;
        prev->next_free = NULL;
        return;
    }
    // is somewhere in the middle
    prev->next_free = next;
    next->prev_free = prev;

    to_delete->next_free = NULL;
    to_delete->prev_free = NULL;
}

// coalece and cut before adding

// inserts a ready free block to the free list in a sorted manner.
void _insert_to_free_list(MallocMetadata to_insert) // coalece and cut before adding
{
    _check_for_overflow();
    if (!to_insert)
    {
        return;
    }
    if (!free_list_head) // empty free list
    {
        free_list_head = to_insert;
        free_list_tail = to_insert;
        to_insert->next_free = NULL;
        to_insert->prev_free = NULL;
        return;
    }

    MallocMetadata iter;
    for (iter = free_list_head; iter; iter = iter->next_free)
    {
        if (iter->size >= to_insert->size)
        {
            break;
        }
    }
    // now insert after size
    if (!iter)
    {
        free_list_tail->next_free = to_insert;
        to_insert->prev_free = free_list_tail;
        free_list_tail = to_insert;
        to_insert->next_free = NULL;
        return;
    }

    if (iter->size == to_insert->size)
    {
        if (iter > to_insert)
        {
            if (!iter->prev_free)
            {
                to_insert->next_free = iter;
                to_insert->prev_free = NULL;
                iter->prev_free = to_insert;

                free_list_head = to_insert;
            } else
            {
                to_insert->next_free = iter;
                to_insert->prev_free = iter->prev_free;
                iter->prev_free->next_free = to_insert;
                iter->prev_free = to_insert;
            }
            return;
        }
        while (iter->size == to_insert->size && iter < to_insert && iter->next_free
                && iter->next_free->size == to_insert->size && iter->next_free < to_insert)
        {
            iter = iter->next_free;
        }

        // add after iterator
        to_insert->prev_free = iter;
        to_insert->next_free = iter->next_free;
        if (iter->next_free)
        {
            iter->next_free->prev_free = to_insert;
        } else
        {
            free_list_tail = to_insert;
            to_insert->next_free = NULL;
        }
        iter->next_free = to_insert;

        return;
    }

    if (!iter->prev_free)
    {
        to_insert->next_free =iter;
        to_insert->prev_free = NULL;
        iter->prev_free = to_insert;

        free_list_head = to_insert;
        return;
    }

    iter->prev_free->next_free = to_insert;
    to_insert->next_free = iter;
    iter->prev_free = to_insert;
    to_insert->prev_free = iter->prev_free;
}

void _insert_to_block_list(MallocMetadata to_insert)
{
    if (!to_insert)
    {
        return;
    }

    if (to_insert < head) // insert at head
    {
        head->prev = to_insert;
        to_insert->next = head;
        to_insert->prev = NULL;
        head = to_insert;
        return;
    }

    MallocMetadata iter = head->next;

    MallocMetadata iter_prev = head;
    while (iter && iter < to_insert) // search for last node smaller than to_insert
    {
        iter = iter->next;
        iter_prev = iter_prev->next;
    }

    // add after prev
    if (iter)
    {
        iter->prev = iter;
    } else
    {
        tail = to_insert; // inserted last so tail updated
    }

    to_insert->next = iter;
    iter_prev->next = to_insert;
    to_insert->prev = iter_prev;
}
void _check_for_overflow()
{
    for (MallocMetadata iter = head; iter; iter = iter->next)
    {
        if (iter->cookie != random_cookie)
        {
            exit(0xdeadbeef);
        }
    }
}

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
        if (head == (void *) -1)
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

    std::memmove(allocated_ptr, oldp, old_metadata->size); // maybe bug here
    sfree(oldp);

    return allocated_ptr;
}

