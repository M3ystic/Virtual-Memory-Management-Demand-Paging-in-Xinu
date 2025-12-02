/* resched.c - resched, resched_cntl */

#include <xinu.h>

struct  defer   Defer;

/*------------------------------------------------------------------------
 *  resched  -  Reschedule processor to highest priority eligible process
 *------------------------------------------------------------------------
 */
void    resched(void)       /* Assumes interrupts are disabled    */
{
    struct procent *ptold;  /* Ptr to table entry for old process */
    struct procent *ptnew;  /* Ptr to table entry for new process */

    if (Defer.ndefers > 0) {
        Defer.attempt = TRUE;
        return;
    }

    ptold = &proctab[currpid];

    if (ptold->prstate == PR_CURR) {
        if (ptold->prprio > firstkey(readylist)) {
            return;
        }

        ptold->prstate = PR_READY;
        insert(currpid, readylist, ptold->prprio);
    }

    currpid = dequeue(readylist);
    ptnew = &proctab[currpid];
    ptnew->prstate = PR_CURR;
    preempt = QUANTUM;

    /* === REQUIRED ADDITION: switch page directory === */
   /* switch PD only for user processes */
	if (ptnew->user_process) {
		write_cr3(ptnew->pdbr);
	} else {
		write_cr3(kernels_directory);
	}

    ctxsw(&ptold->prstkptr, &ptnew->prstkptr);

    return;
}

/*------------------------------------------------------------------------
 *  resched_cntl  -  Control whether rescheduling is deferred or allowed
 *------------------------------------------------------------------------
 */
status  resched_cntl(       /* Assumes interrupts are disabled    */
          int32  defer      /* Either DEFER_START or DEFER_STOP   */
        )
{
    switch (defer) {

        case DEFER_START:
            if (Defer.ndefers++ == 0) {
                Defer.attempt = FALSE;
            }
            return OK;

        case DEFER_STOP:
            if (Defer.ndefers <= 0) {
                return SYSERR;
            }
            if ((--Defer.ndefers == 0) && Defer.attempt) {
                resched();
            }
            return OK;

        default:
            return SYSERR;
    }
}
