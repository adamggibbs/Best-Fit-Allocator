// ==============================================================================
/**
 * bf-alloc.c
 *
 * A _best-fit_ heap allocator.  This allocator uses a _doubly-linked free list_
 * from which to allocate the best fitting free block.  If the list does not
 * contain any blocks of sufficient size, it uses _pointer bumping_ to expand
 * the heap.
 **/
// ==============================================================================



// ==============================================================================
// INCLUDES

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>

#include "safeio.h"
// ==============================================================================



// ==============================================================================
// TYPES AND STRUCTURES

/** The header for each allocated object. */
typedef struct header {

  /** Pointer to the next header in the list. */
  struct header* next;

  /** Pointer to the previous header in the list. */
  struct header* prev;

  /** The usable size of the block (exclusive of the header itself). */
  size_t         size;

  /** Is the block allocated or free? */
  bool           allocated;

} header_s;
// ==============================================================================



// ==============================================================================
// MACRO CONSTANTS AND FUNCTIONS

/** The system's page size. */
#define PAGE_SIZE sysconf(_SC_PAGESIZE)

/**
 * Macros to easily calculate the number of bytes for larger scales (e.g., kilo,
 * mega, gigabytes).
 */
#define KB(size)  ((size_t)size * 1024)
#define MB(size)  (KB(size) * 1024)
#define GB(size)  (MB(size) * 1024)

/** The virtual address space reserved for the heap. */
#define HEAP_SIZE GB(2)

/** Given a pointer to a header, obtain a `void*` pointer to the block itself. */
#define HEADER_TO_BLOCK(hp) ((void*)((intptr_t)hp + sizeof(header_s)))

/** Given a pointer to a block, obtain a `header_s*` pointer to its header. */
#define BLOCK_TO_HEADER(bp) ((header_s*)((intptr_t)bp - sizeof(header_s)))
// ==============================================================================


// ==============================================================================
// GLOBALS

/** The address of the next available byte in the heap region. */
static intptr_t free_addr  = 0;

/** The beginning of the heap. */
static intptr_t start_addr = 0;

/** The end of the heap. */
static intptr_t end_addr   = 0;

/** The head of the free list. */
static header_s* free_list_head = NULL;

/** The head of the allocated list. */
static header_s* allocated_list_head = NULL;
// ==============================================================================



// ==============================================================================
/**
 * The initialization method.  If this is the first use of the heap, initialize it.
 */

void init () {

  // Only do anything if there is no heap region (i.e., first time called).
  if (start_addr == 0) {

    DEBUG("Trying to initialize");
    
    // Allocate virtual address space in which the heap will reside. Make it
    // un-shared and not backed by any file (_anonymous_ space).  A failure to
    // map this space is fatal.
    void* heap = mmap(NULL,
		      HEAP_SIZE,
		      PROT_READ | PROT_WRITE,
		      MAP_PRIVATE | MAP_ANONYMOUS,
		      -1,
		      0);
    if (heap == MAP_FAILED) {
      ERROR("Could not mmap() heap region");
    }

    // Hold onto the boundaries of the heap as a whole.
    start_addr = (intptr_t)heap;
    end_addr   = start_addr + HEAP_SIZE;
    free_addr  = start_addr;

    // DEBUG: Emit a message to indicate that this allocator is being called.
    DEBUG("bf-alloc initialized");

  }

} // init ()
// ==============================================================================


// ==============================================================================
/**
 * Allocate and return `size` bytes of heap space.  Specifically, search the
 * free list, choosing the _best fit_.  If no such block is available, expand
 * into the heap region via _pointer bumping_.
 *
 * \param size The number of bytes to allocate.
 * \return A pointer to the allocated block, if successful; `NULL` if unsuccessful.
 */
void* malloc (size_t size) {

  // if heap hasn't yet been initialized, do it
  init();

  // if the requested block size is 0, return NULL because there is nothing to do
  if (size == 0) {
    return NULL;
  }

  
  // going to loop through the free blocks on free LL
  // create a pointer and point it to free LL head
  // create a pointer to store best fit, initially set to NULL
  header_s* current = free_list_head;
  header_s* best    = NULL;

  // start looping thru free LL until we reach the end
  while (current != NULL) {

    // if there is an allocated block on free LL, raise an error
    if (current->allocated) {
      ERROR("Allocated block on free list", (intptr_t)current);
    }

    // if there is no best block and current block is => requested size,
    // or if current block is closer to requested size than best block
    // then make current block the best block
    if ( (best == NULL && size <= current->size) ||
	 (best != NULL && size <= current->size && current->size < best->size) ) {
      best = current;
    }

    // if best block is the exact requested size, then break the loop
    // because we've found our prefect fit
    if (best != NULL && best->size == size) {
      break;
    }

    // move down LL to check next block in free LL
    current = current->next;
    
  }

  // create a pointer to eventually hold block pointer to be returned
  // intially set to NULL
  void* new_block_ptr = NULL;

  // if we found a best block, allocate it
  // 1) remove from free LL
  // 2) add it to the allocated LL
  // 3) create a block pointer from best pointer
  // 4) header pointer is best pointer and is stored in allocated LL
   if (best != NULL) {

    // remove best from LL by moving pointers
    // if prev of best is NULL then it is the head of free LL
    // so make next of best the new head of free LL
    // else have prev of best skip over best with its next pointer
    if (best->prev == NULL) {
      free_list_head   = best->next;
    } else {
      best->prev->next = best->next;
    }
    // if best is not the end of the free LL
    // then make the next of best skip over best with its prev pointer
    if (best->next != NULL) {
      best->next->prev = best->prev;
    }

    // add header to allocated list
    // make the next of best the current head of allocated LL
    best->next          = allocated_list_head;
    // make best the new head of alloacted LL
    allocated_list_head = best;
    // make prev of best NULL
    best->prev          = NULL;
    // if best is not the only header in LL, make best the prev of next header in LL
    if (best->next != NULL) {
      best->next->prev  = best;
    }
    // set best to be allocated
    best->allocated     = true;

    // set block pointer to be address after header--which is the pointer best
    new_block_ptr       = HEADER_TO_BLOCK(best);
    
  } else {

    // pad the address for double word alignment
    // since the header is 32 bytes, if we align for the header
    // then the block will be double word aligned as well
    free_addr = free_addr + (16 - free_addr % 16);

    // create a pointer for the header at the next free address space
    header_s* header_ptr = (header_s*)free_addr;
    // create a pointer for the block immediately after the header
    new_block_ptr = HEADER_TO_BLOCK(header_ptr);

    // add header to the allocated LL
    // make next for header_ptr be the current LL head
    header_ptr->next      = allocated_list_head;
    // make header_ptr the LL head
    allocated_list_head   = header_ptr;
    // make prev for header_ptr NULL since it's the beginning
    header_ptr->prev      = NULL;
    // if there was a block as the LL head, then make it's prev header_ptr
    if (header_ptr->next != NULL) {
      header_ptr->next->prev = header_ptr;
    }
    // store the size of the block in the header
    header_ptr->size      = size;
    // set to true that the block has been allocated
    header_ptr->allocated = true;

    // if new free_addr has surpassed the end of the memory space
    // then return NULL since there isn't enough space to allocate
    // else update free_addr
    intptr_t new_free_addr = (intptr_t)new_block_ptr + size;
    if (new_free_addr > end_addr) {

      return NULL;

    } else {

      free_addr = new_free_addr;

    }

  }

  // return pointer to block
  return new_block_ptr;

} // malloc()
// ==============================================================================



// ==============================================================================
/**
 * Deallocate a given block on the heap.  Add the given block (if any) to the
 * free list.
 *
 * \param ptr A pointer to the block to be deallocated.
 */
void free (void* ptr) {

  // if pointer is NULL there is nothing to free, jsut return
  if (ptr == NULL) {
    return;
  }

  // get pointer to block from the header
  header_s* header_ptr = BLOCK_TO_HEADER(ptr);

  // if block is not allocated then it is already free, raise an error
  if (!header_ptr->allocated) {
    ERROR("Double-free: ", (intptr_t)header_ptr);
  }

  // remove header from allocated LL
  // if header is not the end of the allocated LL, adjust prev pointer of next
  if ( header_ptr->next != NULL) {
    header_ptr->next->prev = header_ptr->prev;
  }

  // if header is not head of allocated LL, adjust next pointer of prev
  // else make next pointer the head of allocated LL
  if ( header_ptr->prev != NULL ){
    header_ptr->prev->next = header_ptr->next;
  } else {
    allocated_list_head = header_ptr->next;
  }
  
  // add header to free LL
  // make next the current head of free LL
  header_ptr->next = free_list_head;
  // make freed header the new head of free LL
  free_list_head   = header_ptr;
  // make prev of freed header NULL
  header_ptr->prev = NULL;
  // if freed header is not the only pointer in LL, make prev pointer of next the freed header
  if (header_ptr->next != NULL) {
    header_ptr->next->prev = header_ptr;
  }
  // set the freed header to NOT allocated
  header_ptr->allocated = false;

} // free()
// ==============================================================================



// ==============================================================================
/**
 * Allocate a block of `nmemb * size` bytes on the heap, zeroing its contents.
 *
 * \param nmemb The number of elements in the new block.
 * \param size  The size, in bytes, of each of the `nmemb` elements.
 * \return      A pointer to the newly allocated and zeroed block, if successful;
 *              `NULL` if unsuccessful.
 */
void* calloc (size_t nmemb, size_t size) {

  // Allocate a block of the requested size.
  size_t block_size    = nmemb * size;
  void*  new_block_ptr = malloc(block_size);

  // If the allocation succeeded, clear the entire block.
  if (new_block_ptr != NULL) {
    memset(new_block_ptr, 0, block_size);
  }

  return new_block_ptr;
  
} // calloc ()
// ==============================================================================



// ==============================================================================
/**
 * Update the given block at `ptr` to take on the given `size`.  Here, if `size`
 * fits within the given block, then the block is returned unchanged.  If the
 * `size` is an increase for the block, then a new and larger block is
 * allocated, and the data from the old block is copied, the old block freed,
 * and the new block returned.
 *
 * \param ptr  The block to be assigned a new size.
 * \param size The new size that the block should assume.
 * \return     A pointer to the resultant block, which may be `ptr` itself, or
 *             may be a newly allocated block.
 */
void* realloc (void* ptr, size_t size) {

  // Special case: If there is no original block, then just allocate the new one
  // of the given size.
  if (ptr == NULL) {
    return malloc(size);
  }

  // Special case: If the new size is 0, that's tantamount to freeing the block.
  if (size == 0) {
    free(ptr);
    return NULL;
  }

  // Get the current block size from its header.
  header_s* header_ptr = BLOCK_TO_HEADER(ptr);

  // If the new size isn't an increase, then just return the original block as-is.
  if (size <= header_ptr->size) {
    return ptr;
  }

  // The new size is an increase.  Allocate the new, larger block, copy the
  // contents of the old into it, and free the old.
  void* new_block_ptr = malloc(size);
  if (new_block_ptr != NULL) {
    memcpy(new_block_ptr, ptr, header_ptr->size);
    free(ptr);
  }
    
  return new_block_ptr;
  
} // realloc()
// ==============================================================================
