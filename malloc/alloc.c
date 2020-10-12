/**
 * malloc
 * CS 241 - Fall 2020
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>

typedef struct meta_data { 
    void *ptr;
    size_t size;
    bool free;
    struct meta_data* next;
    struct meta_data* prev;
} meta_data;

static meta_data * head = NULL;

// inserts an itemn to the list AT HEAD (first!!!)
// add to list if the memory is AVALIABLE (free = true)
void list_add(meta_data* block) {
    if (!block) return;

    block->free = true;
    if (head == NULL) {
        head = block;
        head->next = NULL;
        head->prev = NULL;
    } else {
        block->next = head;
        head->prev = block;
        head = block;
    }
    
}

void list_remove(meta_data* block) {
    if (!block) return;
    
    block->free = false;
    if (block == head) {
        meta_data* temp = block->next;
        block = NULL;

        if (temp) {
            temp->prev = NULL;
        }

    }
}

void merge_blocks() {
    meta_data* md = head;
    while (md) {
        if (md->size < 32 && (md->prev != NULL || md->next != NULL)) {
            if (  md->next == NULL ) {
                // merge with the previous block
                meta_data* last = md->prev;
                last->size += (sizeof(meta_data) + md->size);
                last->next = NULL;
                md = NULL;
            } else if ( md->prev == NULL ) {    // if at head
                // merge with next block
                meta_data* second = md->next;
                md->next = second->next;
                if (second->next) second->next->prev = md;
                md->size += (second->size + sizeof(meta_data));
                second = NULL;
            } else {
                meta_data* block;
                // if (md->prev->size < md->next->size) {  // merge with previous block
                    block = md->prev;
                    md->next->prev = block;
                    block->next = md->next;
                    block->size += (md->size + sizeof(meta_data));
                    //md = block;


                // } else {        // merge with next block
                //     block = md->next;
                //     md->next = block->next;
                //     md->size += (block->size + sizeof(meta_data));
                //     block->next->prev = md;
                //     block = NULL;
                //     md = md->next;
                // }
            }
        }
        if (md->next && md == md->next) break;
        md = md->next;
    }

}

/**
 * Allocate space for array in memory
 *
 * Allocates a block of memory for an array of num elements, each of them size
 * bytes long, and initializes all its bits to zero. The effective result is
 * the allocation of an zero-initialized memory block of (num * size) bytes.
 *
 * @param num
 *    Number of elements to be allocated.
 * @param size
 *    Size of elements.
 *
 * @return
 *    A pointer to the memory block allocated by the function.
 *
 *    The type of this pointer is always void*, which can be cast to the
 *    desired type of data pointer in order to be dereferenceable.
 *
 *    If the function failed to allocate the requested block of memory, a
 *    NULL pointer is returned.
 *
 * @see http://www.cplusplus.com/reference/clibrary/cstdlib/calloc/
 */
void *calloc(size_t num, size_t size) {
    void* ptr = malloc(num*size);
    
    if (!ptr) return NULL;
    
    memset(ptr, 0, num*size);
    
    return ptr;
}

/**
 * Allocate memory block
 *
 * Allocates a block of size bytes of memory, returning a pointer to the
 * beginning of the block.  The content of the newly allocated block of
 * memory is not initialized, remaining with indeterminate values.
 *
 * @param size
 *    Size of the memory block, in bytes.
 *
 * @return
 *    On success, a pointer to the memory block allocated by the function.
 *
 *    The type of this pointer is always void*, which can be cast to the
 *    desired type of data pointer in order to be dereferenceable.
 *
 *    If the function failed to allocate the requested block of memory,
 *    a null pointer is returned.
 *
 * @see http://www.cplusplus.com/reference/clibrary/cstdlib/malloc/
 */
void *malloc(size_t size) {
    /* See if we have free space of enough size. */ 
    meta_data *p = head;
    meta_data *chosen = NULL;
    while (p != NULL) {
        if (p->size >= size) {
            if (chosen == NULL ||  p->size < chosen->size) {
                chosen = p;
                break;
            }
        }
        p = p->next; 
    }
    if (chosen) { 
        list_remove(chosen);
        return chosen->ptr; 
    }
    /* Add our entry to the metadata */ 
    chosen = sbrk(sizeof(meta_data));
    chosen->ptr = sbrk(0);

    if (sbrk(size) == (void*)-1) {
        return NULL; 
    }

    chosen->size = size;
    chosen->free = false;
    

    return chosen->ptr;
}

/**
 * Deallocate space in memory
 *
 * A block of memory previously allocated using a call to malloc(),
 * calloc() or realloc() is deallocated, making it available again for
 * further allocations.
 *
 * Notice that this function leaves the value of ptr unchanged, hence
 * it still points to the same (now invalid) location, and not to the
 * null pointer.
 *
 * @param ptr
 *    Pointer to a memory block previously allocated with malloc(),
 *    calloc() or realloc() to be deallocated.  If a null pointer is
 *    passed as argument, no action occurs.
 */
void free(void *ptr) {
    if(ptr == NULL) return; 
    meta_data* p = ptr - sizeof(meta_data);
    list_add(p);
    //merge_blocks();
}

/**
 * Reallocate memory block
 *
 * The size of the memory block pointed to by the ptr parameter is changed
 * to the size bytes, expanding or reducing the amount of memory available
 * in the block.
 *
 * The function may move the memory block to a new location, in which case
 * the new location is returned. The content of the memory block is preserved
 * up to the lesser of the new and old sizes, even if the block is moved. If
 * the new size is larger, the value of the newly allocated portion is
 * indeterminate.
 *
 * In case that ptr is NULL, the function behaves exactly as malloc, assigning
 * a new block of size bytes and returning a pointer to the beginning of it.
 *
 * In case that the size is 0, the memory previously allocated in ptr is
 * deallocated as if a call to free was made, and a NULL pointer is returned.
 *
 * @param ptr
 *    Pointer to a memory block previously allocated with malloc(), calloc()
 *    or realloc() to be reallocated.
 *
 *    If this is NULL, a new block is allocated and a pointer to it is
 *    returned by the function.
 *
 * @param size
 *    New size for the memory block, in bytes.
 *
 *    If it is 0 and ptr points to an existing block of memory, the memory
 *    block pointed by ptr is deallocated and a NULL pointer is returned.
 *
 * @return
 *    A pointer to the reallocated memory block, which may be either the
 *    same as the ptr argument or a new location.
 *
 *    The type of this pointer is void*, which can be cast to the desired
 *    type of data pointer in order to be dereferenceable.
 *
 *    If the function failed to allocate the requested block of memory,
 *    a NULL pointer is returned, and the memory block pointed to by
 *    argument ptr is left unchanged.
 *
 * @see http://www.cplusplus.com/reference/clibrary/cstdlib/realloc/
 */
void *realloc(void *ptr, size_t size) {
    // implement realloc!
    if (!ptr) return malloc(size);

    if (size == 0) {
        free(ptr);
        return NULL;
    }

    meta_data* entry = ((meta_data*)ptr) - 1;
    size_t entry_size = entry->size;

    if (entry->free)return ptr;
    if (size < entry_size*2 || size-entry_size < 1024) return ptr;

    void* newptr = malloc(size);

    if (newptr == (void*)-1) return NULL;

    size_t min_size = entry_size < size ? entry_size : size;
    memmove(newptr, ptr, min_size);
    free(ptr);
    return newptr;
}
