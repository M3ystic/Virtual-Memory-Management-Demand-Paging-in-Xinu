/* kill.c - kill */

#include <xinu.h>

/*------------------------------------------------------------------------
 *  kill  -  Kill a process and remove it from the system
 *------------------------------------------------------------------------
 */
syscall	kill(
	  pid32		pid		/* ID of process to kill	*/
	)
{
	intmask	mask;			/* Saved interrupt mask		*/
	struct	procent *prptr;		/* Ptr to process's table entry	*/
	int32	i;			/* Index into descriptors	*/

	mask = disable();
	if (isbadpid(pid) || (pid == NULLPROC)
	    || ((prptr = &proctab[pid])->prstate) == PR_FREE) {
		restore(mask);
		return SYSERR;
	}

	if (--prcount <= 1) {		/* Last user process completes	*/
		xdone();
	}

	send(prptr->prparent, pid);
	for (i=0; i<3; i++) {
		close(prptr->prdesc[i]);
	}
	freestk(prptr->prstkbase, prptr->prstklen);

	switch (prptr->prstate) {
	case PR_CURR:
		prptr->prstate = PR_FREE;	/* Suicide */
		resched();

	case PR_SLEEP:
	case PR_RECTIM:
		unsleep(pid);
		prptr->prstate = PR_FREE;
		break;

	case PR_WAIT:
		semtab[prptr->prsem].scount++;
		/* Fall through */

	case PR_READY:
		getitem(pid);		/* Remove from queue */
		/* Fall through */

	default:
		prptr->prstate = PR_FREE;
	}


	if (prptr->pdbr != 0 && prptr->pdbr != kernels_directory){
        pd_t *pdir = (pd_t *)prptr->pdbr;

        int pd,pt;                  
        uint32 ptaddr, frameaddr;             
        pt_t *ptable;             

        for (pd = 0; pd < XINU_PAGE_DIRECTORY_LENGTH; pd++)
        {
            if (pdir[pd].pd_pres == 0)
                continue;

            ptaddr = pdir[pd].pd_base << 12;
            ptable = (pt_t *)ptaddr;

            for (pt = 0; pt < PAGE_TABLE_ENTRIES; pt++)
            {
                if (ptable[pt].pt_pres == 1)
                {
                    frameaddr = ptable[pt].pt_base << 12;

                    if (frameaddr >= FFS_START && frameaddr < FFS_END)
                        freeffsframe(frameaddr);
                }
            }

            free_page_frame(ptaddr);
        }

        free_page_frame(prptr->pdbr);

        prptr->pdbr        = 0;
        prptr->heapstart   = NULL;
        prptr->heapend     = NULL;
        prptr->heapmlist   = NULL;
        prptr->allocvpages = 0;
    }


	restore(mask);
	return OK;
}
