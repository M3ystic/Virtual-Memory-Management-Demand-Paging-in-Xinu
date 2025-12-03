#include <xinu.h>

void pagefault_handler(void)
{
    uint32 faultaddr = read_cr2();
    struct procent *process = &proctab[currpid];

    uint32 page_base = faultaddr & ~(PAGE_SIZE - 1);

    virt_addr_t *virtaddr = (virt_addr_t *)&faultaddr;
    uint32 pd_index = virtaddr->pd_offset;
    uint32 pt_index = virtaddr->pt_offset;

    pd_t *pagedir      = (pd_t *)process->pdbr;
    pd_t *pagedirentry = &pagedir[pd_index];

    if (pagedirentry->pd_pres == 0) {
         kprintf("P%d:: SEGMENTATION_FAULT \n", currpid);
        kill(currpid);
        return;
    }

    pt_t *pagetable      = (pt_t *)(pagedirentry->pd_base << 12);
    pt_t *pagetableentry = &pagetable[pt_index];

    if (pagetableentry->pt_pres == 1) {
         kprintf("P%d:: SEGMENTATION_FAULT \n", currpid);
        kill(currpid);
        return;
    }

    if (pagetableentry->pt_avail == 0) {
        kprintf("P%d:: SEGMENTATION_FAULT \n", currpid);
        kill(currpid);
        return;
    }

    uint32 newframe = new_ffs_frame();
    if (newframe == (uint32)SYSERR) {
        kprintf("P%d:: SEGMENTATION_FAULT \n", currpid);
        kill(currpid);
        return;
    }

    pagetableentry->pt_base  = newframe >> 12;
    pagetableentry->pt_pres  = 1;
    pagetableentry->pt_write = 1;
    pagetableentry->pt_user  = 1;
    pagetableentry->pt_avail = 1;

    write_cr3(process->pdbr);

    memset((void *)page_base, 0, PAGE_SIZE);
}
