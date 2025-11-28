/* vmalloc.c - vmalloc*/

#include <xinu.h>

/*------------------------------------------------------------------------
 *  vmalloc  -  Create a process to start running a function on x86
 *------------------------------------------------------------------------
 */

char*	vmalloc(
	  uint32 size
	)
{
    if (size == 0) {
        return (char*)SYSERR;
    }

    struct procent *prptr = &proctab[currpid];

    size = (uint32)roundmb(size);

    struct memblk *prev = &prptr->vmemlist;
    struct memblk *curr = prptr->vmemlist.mnext;

    while (curr != NULL)
    {
        if (curr->mlength == size)
        {
            prev->mnext = curr->mnext;
            return (char *)curr;
        }

        if (curr->mlength > size)
        {
            struct memblk *next = (struct memblk *)((char*)curr + size);
            next->mnext = curr->mnext;
            next->mlength = curr->mlength - size;

            prev->mnext = next;
            curr->mlength = size;
            curr->mnext = NULL;

            return (char*)curr;
        }

        prev = curr;
        curr = curr->mnext;
    }

    return (char*)SYSERR;
}
