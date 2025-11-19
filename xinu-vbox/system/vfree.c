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
    return OK;
}
