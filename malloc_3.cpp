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
#define MMAP_THRESHOLD 131072
#define MAX_BLOCK_SIZE 100000000

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

MallocMetadata mmap_head = NULL;

int random_cookie = rand();

void _check_for_overflow();

void _remove_from_free_list(MallocMetadata to_delete);

void _remove_from_block_list(MallocMetadata to_delete);

size_t _size_meta_data();

void _insert_to_free_list(MallocMetadata to_insert);

MallocMetadata _find_best_fit_in_free_list(size_t size);

void _insert_to_block_list(MallocMetadata to_insert);

void _cut_if_needed(MallocMetadata to_cut, size_t wanted_size);

void _insert_to_mmap_list(MallocMetadata to_insert);

void _remove_from_mmap_list(MallocMetadata to_remove);

MallocMetadata _allocate_mmap(size_t wanted_size);

MallocMetadata _increase_wilderness_size_if_needed(size_t wanted_size);

bool _is_mmap_allocation(MallocMetadata alloc);

void _insert_to_mmap_list(MallocMetadata to_insert)
{
    if (!to_insert)
    {
        return;
    }
    if (!mmap_head)
    {
        mmap_head = to_insert;
        mmap_head->next = NULL;
        mmap_head->prev = NULL;
        return;
    }

    MallocMetadata iter = mmap_head;
    while (iter->next)
    {
        iter = iter->next;
    }
    iter->next = to_insert;
    to_insert->prev = iter;

}

void _remove_from_mmap_list(MallocMetadata to_remove)
{
    if (!to_remove)
    {
        return;
    }
    if (!mmap_head)
    {
        return;
    }
    MallocMetadata iter = mmap_head;
    while (iter)
    {
        if (iter == to_remove)
        {
            break;
        }
        iter = iter->next;
    }
    if (!iter)
    {
        // not found
        return;
    }
    if (!iter->prev && !iter->next)
    {
        mmap_head = NULL;
        return;
    }

    if (iter->prev)
    {
        iter->prev->next = to_remove->next;

    }
    if (iter->next)
    {
        iter->next->prev = to_remove->prev;
    }
}

bool _is_mmap_allocation(MallocMetadata alloc)
{
    if (!alloc)
    {
        return false;
    }
    return alloc->size >= MMAP_THRESHOLD;
}

// returns NULL if no fit is possible
MallocMetadata _find_best_fit_in_free_list(size_t size)
{
    for (MallocMetadata iter = free_list_head; iter; iter = iter->next_free)
    {
        if (iter->size <= size)
        {
            return iter;
        }
    }
    return NULL;
}

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
    MallocMetadata new_metadata = (MallocMetadata) ((size_t) to_cut + _size_meta_data() + wanted_size);

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
    if (!block->is_free)
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
        } else
        { // head and tail
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
        } else
        { // head and tail
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

// send only if wilderness is free and all other options are non-viable
// return NULL if :
//      head is NULL so list is empty
//      tail is NULL so list is empty
//      if the wilderness block isn't free
//      if sbrk fails
// otherwise increases wilderness size after allocating size using sbrk
MallocMetadata _increase_wilderness_size_if_needed(size_t wanted_size)
{
    if (!head)
    {
        return NULL;
    }
    if (!tail)
    {
        return NULL;
    }
    if (!tail->is_free)
    {
        return NULL;
    }
    if (wanted_size <= tail->size)
    {
        tail->is_free = false;
        return tail;
    }
    // need to increase wilderness size
    if (sbrk(wanted_size - tail->size) == (void *) -1)
    {
        return NULL;
    }

    tail->size = wanted_size;
    tail->is_free = false;
    return tail;
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
        to_insert->next_free = iter;
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

    for (MallocMetadata iter = mmap_head; iter; iter = iter->next)
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

    for (MallocMetadata iter = mmap_head; iter; iter = iter->next)
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
    _check_for_overflow();
    if (size == 0 || size > MAX_BLOCK_SIZE)
    {
        return NULL;
    }

    void *allocated_ptr;


    if (size >= MMAP_THRESHOLD)
    {
        MallocMetadata tmp;
        size_t real_size = _size_meta_data() + size;
        tmp =(MallocMetadata) mmap(NULL, real_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (tmp == (void *) -1)
        {
            return NULL;
        }

        _insert_to_mmap_list(tmp);
        tmp->size = size;
        tmp->cookie = random_cookie;
        return (void *) ((size_t) tmp + _size_meta_data());
    }
    // empty list
    if (!head)
    {
        head = (MallocMetadata) sbrk(_size_meta_data());
        if (head == (void *) -1)
        {
            head = NULL;
            return NULL;
        }
        head->cookie = random_cookie;
        head->size = size;
        head->prev = NULL;
        head->next = NULL;
        head->is_free = false;
        tail = head;

        allocated_ptr = sbrk(size);
        if (allocated_ptr == (void *) -1) // syscall failed
        {
            head = NULL;
            return NULL;
        }

        // successful first allocation
        return allocated_ptr;
    }

    // here we go
    MallocMetadata best_fit = _find_best_fit_in_free_list(size);


    if (best_fit) // found fit really
    {
        _remove_from_free_list(best_fit);
        _cut_if_needed(best_fit, size);
        best_fit->is_free = false;
        return (void *) ((size_t) best_fit + _size_meta_data());
    }

    best_fit = _increase_wilderness_size_if_needed(size);
    if (best_fit)
    {
        // increased wilderness and got it;
        return (void *) ((size_t) best_fit + _size_meta_data());
    }

    // must increase size of heap
    MallocMetadata allocated_metadata = (MallocMetadata) sbrk(_size_meta_data());
    if (allocated_metadata == (void *) -1)
    {
        return NULL;
    }
    _insert_to_block_list(allocated_metadata);
    allocated_metadata->cookie = random_cookie;
    allocated_metadata->is_free = false;

    allocated_ptr = sbrk(size);
    if (allocated_ptr == (void *) -1)
    {
        return NULL;
    }
    return allocated_ptr;

}

void *scalloc(size_t num, size_t size)
{
    _check_for_overflow();
    if (size == 0 || num * size > MAX_BLOCK_SIZE)
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
    _check_for_overflow();
    if (!p)
    {
        return;
    }
    MallocMetadata metadata_ptr = (MallocMetadata) ((size_t) p - _size_meta_data());
    if (_is_mmap_allocation(metadata_ptr))
    {
        _remove_from_mmap_list(metadata_ptr);
        munmap((void *) metadata_ptr, _size_meta_data() + metadata_ptr->size);
        return;
    }
    if (metadata_ptr->is_free)
    {
        return;
    }
    metadata_ptr->is_free = true;
    _insert_to_free_list(metadata_ptr);
    _coalesce_free_blocks(metadata_ptr);
}

void *srealloc(void *oldp, size_t size)
{
    _check_for_overflow();
    if (size == 0 || size > MAX_BLOCK_SIZE)
    {
        return NULL;
    }

    void *allocated_ptr;
    bool prev_state, next_state;
    MallocMetadata next_to_cut;
    if (!oldp) // NULL received
    {
        return smalloc(size);
    }

    /*---------------------------------- case a start----------------------------------*/
    MallocMetadata old_metadata = (MallocMetadata) ((size_t) oldp - _size_meta_data());

    if (size <= old_metadata->size)
    {
        old_metadata->is_free = false;

        next_to_cut = old_metadata->next;
        _cut_if_needed(old_metadata, size);
        _coalesce_free_blocks(next_to_cut);

        return oldp;
    }
    /*---------------------------------- case a done ----------------------------------*/
    /*---------------------------------- case b start----------------------------------*/

    if (old_metadata->prev && old_metadata->prev->is_free &&
        old_metadata->prev->size + old_metadata->size + _size_meta_data() >= size)
    {
        MallocMetadata prev = old_metadata->prev;
        // size of prev is enough
        if (old_metadata->next)
        {
            next_state = old_metadata->next->is_free;
            old_metadata->next->is_free = false;
        }
        _coalesce_free_blocks(old_metadata);
        if (old_metadata->next)
        {
            old_metadata->next->is_free = next_state;
        }

        _cut_if_needed(prev, size);
        _coalesce_free_blocks(old_metadata);

        allocated_ptr =  (void *) ((size_t) prev + _size_meta_data());
        memmove(allocated_ptr, oldp, old_metadata->size);

        return allocated_ptr;
    }
    if (old_metadata->prev && old_metadata->prev->is_free &&
        old_metadata->prev->size + old_metadata->size + _size_meta_data() < size)
    {
        if (old_metadata == tail)
        {
            MallocMetadata prev = old_metadata->prev;
            // size of prev is enough
            if (old_metadata->next)
            {
                next_state = old_metadata->next->is_free;
                old_metadata->next->is_free = false;
            }
            _coalesce_free_blocks(old_metadata);
            if (old_metadata->next)
            {
                old_metadata->next->is_free = next_state;
            }

            _increase_wilderness_size_if_needed(size);

            allocated_ptr =  (void *) ((size_t) prev + _size_meta_data());
            memmove(allocated_ptr, oldp, old_metadata->size);

            return allocated_ptr;
        }
    }
    /*---------------------------------- case b done ----------------------------------*/
    /*---------------------------------- case c start----------------------------------*/
    if (old_metadata == tail)
    {
        _increase_wilderness_size_if_needed(size);
        return (void *) ((size_t) old_metadata + _size_meta_data());
    }
    /*---------------------------------- case c done ----------------------------------*/
    /*---------------------------------- case d start----------------------------------*/
    if (old_metadata->next && old_metadata->next->is_free &&
        old_metadata->next->size + old_metadata->size + _size_meta_data() >= size)
    {
        // size of prev is enough
        if (old_metadata->prev)
        {
            prev_state = old_metadata->prev->is_free;
            old_metadata->prev->is_free = false;
        }
        _coalesce_free_blocks(old_metadata);
        if (old_metadata->prev)
        {
            old_metadata->prev->is_free = prev_state;
        }

        next_to_cut = old_metadata->next;
        _cut_if_needed(old_metadata, size);
        _coalesce_free_blocks(next_to_cut);

        return (void *) ((size_t) old_metadata + _size_meta_data());
    }
    /*---------------------------------- case d done ----------------------------------*/
    /*---------------------------------- case e start----------------------------------*/
    if (old_metadata->next && old_metadata->next->is_free && old_metadata->prev && old_metadata->prev->is_free &&
        old_metadata->next->size + old_metadata->size + old_metadata->prev->size + 2 * _size_meta_data() >= size)
    {
        MallocMetadata prev = old_metadata->prev;
        _coalesce_free_blocks(old_metadata);

        _cut_if_needed(prev, size);
        _coalesce_free_blocks(old_metadata);

        allocated_ptr =  (void *) ((size_t) prev + _size_meta_data());
        memmove(allocated_ptr, oldp, old_metadata->size);

        return allocated_ptr;
    }
    /*---------------------------------- case e done ----------------------------------*/
    /*---------------------------------- case f start----------------------------------*/
    if (old_metadata->next && old_metadata->next == tail && old_metadata->next->is_free)
    {
        if (old_metadata->prev && old_metadata->prev->is_free)
        {
            MallocMetadata prev = old_metadata->prev;
            _coalesce_free_blocks(old_metadata);

            _increase_wilderness_size_if_needed(size);

            allocated_ptr =  (void *) ((size_t) prev + _size_meta_data());
            memmove(allocated_ptr, oldp, old_metadata->size);

            return allocated_ptr;
        }

        if (old_metadata->prev)
        {
            prev_state = old_metadata->prev->is_free;
            old_metadata->prev->is_free = false;
        }
        _coalesce_free_blocks(old_metadata);
        if (old_metadata->prev)
        {
            old_metadata->prev->is_free = prev_state;
        }
        _increase_wilderness_size_if_needed(size);
        return (void *) ((size_t) old_metadata + _size_meta_data());
    }
    /*---------------------------------- case f done ----------------------------------*/
    /*---------------------------------- case g start----------------------------------*/
    MallocMetadata to_allocate = _find_best_fit_in_free_list(size);
    if (to_allocate)
    {
        to_allocate->is_free = false;
        allocated_ptr =  (void *) ((size_t) to_allocate + _size_meta_data());
        _remove_from_free_list(to_allocate);

        next_to_cut = to_allocate->next;
        _cut_if_needed(to_allocate, size);
        _coalesce_free_blocks(next_to_cut);

        memmove(allocated_ptr, oldp, old_metadata->size);
        sfree(oldp);
        return allocated_ptr;
    }
    /*---------------------------------- case g done ----------------------------------*/
    /*---------------------------------- case h start----------------------------------*/

    allocated_ptr = smalloc(size);
    if (!allocated_ptr)
    {
        return NULL;
    }

    std::memmove(allocated_ptr, oldp, old_metadata->size); // maybe bug here
    sfree(oldp);
    return allocated_ptr;

    /*---------------------------------- case h done ----------------------------------*/

}

