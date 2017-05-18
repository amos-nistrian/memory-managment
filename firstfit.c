/*
    CSCI 352 Assignment 1
    Heap Management functions for first-fit & quick-fit strategy

    Amos Nistrian, April 25, 2016
    David Bover, April 2016

*/

#include <stdio.h>
#include <unistd.h>
#include "heap352.h"
#include "HeapTestEngine.h"
#include <stddef.h>

#define MINALLOC    64      // minimum request to sbrk(), Header blocks
#define ARRAYSIZE   9      // size of the quick-fit array of pointers
#define OFFSET      2       // offset relationship between index of quick_list and chunks in each index
#define MIN         2
//static const MIN = 2;
#define MAX         10
#define TEST   fprintf(stderr, "DB file[%s] line[%d]\n", __FILE__, __LINE__); // macro used for debugging only

typedef long Align;     // to force alignment of free-list blocks
                        // with worst-case data boundaries

// header for free-list entry
union header {
    struct {
        union header *next; // pointer to next block in free-list
        unsigned int size;  // size, in header units, of this block
    } data;

    Align alignment;        // not used, just forces boundary alignment
};

typedef union header Header;

// this is the free-list, initially empty
static Header *free_list = NULL;

// an array of pointers all set to null
static Header *g_quick_list[ARRAYSIZE] = {NULL};

static Header *malloc_quick_list(int units);

static void do_free (void *ptr);

void quick_fit(int size, Header *block);


/*  function display_block()
 *  displays one block of the free list
 *  parameter: curr, pointer to the block
 *  return: none
 */
static void display_block (Header *curr) {

    // address of this block
    unsigned long curr_addr = (unsigned long)curr;

    // address of next block in the free-list
    unsigned long next_addr = (unsigned long)curr->data.next;

    // address of next block in the heap, possibly an allocated block
    unsigned long next_mem  = curr_addr + (curr->data.size + 1) * sizeof(Header);

    printf("free block:0x%x, %4d units, next free:0x%x next block:0x%x\n",
           (int)curr_addr, curr->data.size + 1, (int)next_addr, (int)next_mem);
}


/*  function dump_freelist()
 *  logs the blocks of the free-list
 *  parameters: none
 *  return: none
 */
static void dump_freelist () {

    Header *curr = free_list;

    while (1) {
        display_block(curr);
        curr = curr->data.next;
        if (curr == free_list) break;
    }
    printf("\n");
}

/*  function display_quick_list_block()
 *  logs the blocks of the quick-list
 *  parameters: int units
 *  return: none
 */
/*static void display_quick_list_block(int units){


    // check address of this block
    if (g_quick_list[units-OFFSET] == NULL) {
      curr_addr = 0;
    }
    else{
      curr_addr = (unsigned long)(g_quick_list[units-OFFSET]);
    }

    // address of next block in the heap, possibly an allocated block
    unsigned long next_mem  = curr_addr + (g_quick_list[units-OFFSET]->data.size + 1) * sizeof(Header);

    printf("quick list chunk: %d, free block:0x%x, %4d units, next block:0x%x\n",
          (units), (int)curr_addr, g_quick_list[units-OFFSET]->data.size + 1, (int)next_mem);
  }
}*/

/*  function dump_quick_list()
 *  logs the blocks of the quick-list
 *  parameters: none
 *  return: none
 */
static void dump_quick_list(){
  for ( int i = MIN - OFFSET; i<= MAX - OFFSET; i++) {
    unsigned long curr_addr;
    unsigned long next_addr;
    unsigned long next_mem;
    Header *curr = g_quick_list[i];

    if (curr == NULL){
      printf("quick-list of unit size %d are empty\n", (i+OFFSET));
      i++;
    }
    else {
        curr_addr = (unsigned long)(curr);
        while (curr->data.next != NULL) {
          next_addr = (unsigned long)(curr->data.next);
          next_mem = curr_addr + (curr->data.size + 1) * sizeof(Header);
          printf("quick-list free block:0x%x, %4d units, next free:0x%x next block:0x%x\n",
                (int)curr_addr, curr->data.size + 1, (int)next_addr, (int)next_mem);
          curr = curr->data.next; // move over to next header structure
        }
        next_mem = curr_addr + (curr->data.size + 1) * sizeof(Header);
        printf("quick-list free block:0x%x, %4d units, no more free in quick-list, next block in mem:0x%x\n"
            ,(int)curr_addr, curr->data.size + 1, (int) next_mem);
            i++;
    }
  }
}

/*  function more_heap()
 *  uses sbrk() system call to get more heap space
 *  parameter: nblocks, int, the number of units requested
 *  return: pointer to the new allocated heap-space
 */
static void *more_heap (unsigned nunits) {

    void *mem;
    Header *block;
    unsigned long mem_addr;

    // apply minimum request level for sbrk()
    if (nunits < MINALLOC) nunits = MINALLOC;

    // get sbrk() to provide more heap space
    mem = sbrk(nunits * sizeof(Header));
    mem_addr = (unsigned long) mem;
    printf("(sbrk %d 0x%x) ",
           (unsigned)(nunits * sizeof(Header)), (unsigned)mem_addr);
    if (mem == (void *)(-1)) return NULL;

    // set up a free-list block with the new space
    block = (Header *)mem;
    block->data.size = nunits -1;

    // call free352() to add this block to the free-list
    // initially, add it to the allocated count
    // the call to free352() will add it to the free-list
    do_free((void *)(block + 1));

    return mem;
}

/*  function coalesce()
 *  joins two adjacent free-list blocks into one larger block
 *  parameter: curr, Header *, address of the first (lower address) block
 *  return: int, 1 if the blocks were joined, 0 otherwise
 */
static int coalesce (Header *curr) {
    Header *pos = curr;
    Header *next_block = pos->data.next;
    unsigned long pos_addr = (unsigned long)pos;
    unsigned long next_addr = (unsigned long)next_block;

    // try to coalesce with the next block
    if (pos + pos->data.size + 1 == next_block) {
        printf("coalesce 0x%x (%d) with 0x%x (%d)\n",
               (int)pos_addr, pos->data.size+1,
               (int)next_addr, next_block->data.size+1);
        pos->data.size += next_block->data.size + 1;
        pos->data.next = next_block->data.next;
        return 1;
    }
    // the blocks were not adjacent, so coalesce() failed
    return 0;
}

/* function init_freelist()
 * initializes the free list
 */
static int init_freelist() {

    void *mem;                  // address of heap-space from sbrk() call
    Header *block;

    mem = sbrk(MINALLOC * sizeof(Header));
    if (mem == (void *)(-1)) {
        printf("sbrk() failed\n");
        return -1;
    }

    // set up a size 0 Header for the start of the free_list
    block = (Header *)mem;
    block->data.size = 0;
    block->data.next = block + 1;
    free_list = block;

    // set up the next block, containing the usable memory
    // size is nunits-2 since one block is used for the free-list
    // header and another for the header of this first block
    block = (Header *)(block + 1);
    block->data.size = MINALLOC - 2;
    block->data.next = free_list;

    dump_freelist();
    dump_quick_list();
    return 0;
}

/* function do_malloc()
 * the real heap allocation function
 * allocates (at least) nbytes of space on the heap
 * parameter: nbytes (int), size of space requested
 * return: address of space allocated on the heap
 */
 static void *do_malloc (int nbytes) {

    unsigned int nunits;        // the number of Header-size units required
    Header *curr, *prev;        // used in free-list traversal

    // from nbytes, calculate the number of Header block units
    nunits = (nbytes - 1) / sizeof(Header) + 1;

    // try and malloc chunk from chunk quick_list
    //if (0){
    if (nunits >= MIN && nunits <= MAX ) {

        Header * returned_block = malloc_quick_list(nunits); // check the quick_list for a free space of size nunits

        if (returned_block != NULL) { // malloc_quick_list either returned an address to a header or NULL

          fprintf( stderr, "line[%d]: returned_block address is:%p\n",  __LINE__ ,returned_block);
          return (void *)(returned_block + 1); // success! cast the return and make the appropriate calc to get the address of the first bit past the header
        }

    }


    // first-fit algorithm, free-list blocks are arrange in order of address
    // search through the free-list, looking for sufficient space
    prev = free_list;
    curr = prev->data.next;

    while (curr != free_list)  {

        // exact fit
        if (nunits == curr->data.size) {
            prev->data.next = curr->data.next;
            return (void *)(curr + 1);
        }

        // larger space than needed
        if (nunits < curr->data.size) {
            curr->data.size -= (nunits + 1);
            curr += curr->data.size + 1;
            curr->data.size = nunits;
            return (void *)(curr + 1);
        }

        // move along to next block
        prev = curr;
        curr = curr->data.next;
    }

    // sufficient space not found in any free-list block,
    // request more heap space and try the allocation request again
    if (more_heap(nunits)) {
        return do_malloc(nbytes);
      }
    else {
        return NULL;
      }
  }


  /* function malloc_quick_list()
   * allocates (at least) nunits of space on the heap
   * parameter: units (int), size of space requested in units of 16
   * return: either an address to a header block that can be used for malloc or NULL if no chunk is available
   */
  static Header *malloc_quick_list(int units){
      int nunits = units;

      if ( g_quick_list[nunits-OFFSET] != NULL){ // if there is a free block at the index

        Header *return_block = g_quick_list[nunits-OFFSET]; // get the first block at that index
        g_quick_list[nunits-OFFSET] = return_block->data.next; // point "root" past the node we are going to pop off

        if (return_block->data.next != NULL){  //if the node we are gonna rm is pointing to another node

          return_block->data.next = NULL;  //set the block we are goint to remove data.next to NULL

        return return_block; // return address of the header
        }
      }

      return NULL;
  }

/* function malloc352()
 * a wrapper for do_malloc, with log output
 * allocates (at least) nbytes of space on the heap
 * parameter: nbytes (int), size of space requested
 * return: address of space allocated on the heap
 */
void *malloc352 (int nbytes) {

    int units = (int)((nbytes - 1)/sizeof(Header) + 2);
    printf("malloc %d bytes %d units ",
           nbytes, units);

    void *allocated = do_malloc(nbytes);

    unsigned long address = (unsigned long) allocated;
    // log the outcome of the allocation
    // units total includes header
    printf("allocated at 0x%x\n", (unsigned) address);

    dump_freelist();
    dump_quick_list();

    return allocated;
}

/*  function do_free()
 *  adds a previously-allocated space to the free-list or
 *  if possible adds it to the quick-list
 *  parameter: ptr, void*, address of area being freed
 */
static void do_free (void *ptr) {

    static char bOnce = 1;

    Header *block, *curr;
    int coalesced;

    //sanity check
    if (bOnce) {
      bOnce= 0 ;
      fprintf(stderr, "DB - Memory location of quick_list=[%p]\n", g_quick_list);
    };

    block = (Header *)ptr - 1; // block points to the header of the freed space

    // calculate what size chunk it could potentially fit in
    int chunk_size = (block->data.size+1); // plus 1 to account for header

    if (chunk_size >= MIN && chunk_size <= MAX ) {  // we place the free block into the quick-list
      // remove the pointer to the block from the free-list

      quick_fit(chunk_size, block);

    }

    // the chunk wasnt the right size use first-fit
    else {
        // traverse the free-list, place the block in the correct
        // place, to preserve ascending address order
        curr = free_list;
        while (block > curr) {
            if (block < curr->data.next || curr->data.next == free_list) {

                // need to place block between curr and curr->data.next
                block->data.next = curr->data.next;
                curr->data.next = block;

                // attempt to coalesce with the previous block
                coalesced = 1;
                if (curr != free_list)
                    // attempt to coalesce with next neighbor
                    coalesced = coalesce(curr);

                if (!coalesced || curr == free_list)
                    curr = curr->data.next;

                coalesce(curr);
                break;
            }

            // move along the free-list
            curr = curr->data.next;
        }
    }
}


/*  function quick_fit()
 *  adds a previously-allocated space to the quick-list, g_quick_list, called from do_free()
 *  parameter: block1, Header*, header address of area being freed
 */
void quick_fit(int size1, Header *block1){

  int size = size1;

	Header *block = block1;

  if (g_quick_list[size-OFFSET] == NULL){
      g_quick_list[size-OFFSET] = block; // point to the current block
      block->data.next= NULL; // set that block to null so it doesnt point to a node in the free-list
  }
  // prepend the block
  else{
    (block)->data.next = g_quick_list[size-OFFSET]; // new block is pointing to what g_quick_list is pointing to
    g_quick_list[size-OFFSET] = block;// g_quick_list points to new block now
  }
}

/*  function free352()
 *  wrapper for a call to do_free()
 *  adds a previously-allocated space to the free-list
 *  parameter: ptr, void*, address of area being freed
 */
void free352 (void *ptr) {

    unsigned long ptr_address = (unsigned long) ptr;
    printf("free 0x%x\n", (unsigned)ptr_address);


    do_free(ptr);

    dump_freelist();
    dump_quick_list();

}

int main (int argc, char *argv[]) {

    init_heap_test();

    // initialize the free-list
    if (init_freelist() != 0) {
        printf("cannot initialize free-list, run aborted\n");
        return 1;
    }

    // run the heap test
    heap_test();

    return 0;

}
