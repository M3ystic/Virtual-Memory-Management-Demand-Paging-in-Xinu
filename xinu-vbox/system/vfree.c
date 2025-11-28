/* vfree.c - vmalloc*/

#include <xinu.h>

/*------------------------------------------------------------------------
 *  vfree  -  Create a process to start running a function on x86
 *------------------------------------------------------------------------
 */

syscall	vfree(
	  char* ptr,
      uint32 size
	)
{
    if (size == 0 || ptr == NULL) {
        return SYSERR;
    }

    struct procent *prptr = &proctab[currpid];

    size = (uint32) roundmb(size);

    uint32 heap_start = prptr->vhpbase;
    uint32 heap_end   = prptr->vhpbase + prptr->vhpnpages * PAGE_SIZE;

    if ((uint32)ptr < heap_start || (uint32)ptr >= heap_end)
        return SYSERR;

    struct memblk *block = (struct memblk*)ptr;
    block->mlength = size;

    struct memblk *prev = &prptr->vmemlist;
    struct memblk *curr = prptr->vmemlist.mnext;

    while (curr != NULL && curr < block)
    {
        prev = curr;
        curr = curr->mnext;
    }

    block->mnext = curr;
    prev->mnext = block;

    // combine
    if (curr != NULL &&
        (char*)block + block->mlength == (char*)curr)
    {
        block->mlength += curr->mlength;
        block->mnext = curr->mnext;
    }

    if (prev != &prptr->vmemlist &&
        (char*)prev + prev->mlength == (char*)block)
    {
        prev->mlength += block->mlength;
        prev->mnext = block->mnext;
    }

    return OK;
}
