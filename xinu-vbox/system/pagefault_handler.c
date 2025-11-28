#include <xinu.h>

void pagefault_handler()
{

    uint32 faultaddr = read_cr2();

    struct procent *pr = &proctab[currpid];
    if (faultaddr < (uint32)pr->heapstart || faultaddr >= (uint32)pr->heapend) {
        kprintf("P<%d>:: SEGMENTATION_FAULT\n", currpid);
        kill(currpid);
        return;
    }

    // uint32 VPN = faultaddr >> 12;
    // uint32 pagedirmask = 0b11111111110000000000;
    // uint32 pagetablemask = !(0b11111111110000000000);
    // uint32 pagediri= pagedirmask >> 12;
    // uint32 pagetablei = pagetablemask;

    virt_addr_t* VPN  = (virt_addr_t*)&faultaddr;
    uint32 pagediri   = VPN->pd_offset;
    uint32 pagetablei = VPN->pt_offset;

    pd_t* pdbr = (pd_t*)proctab[currpid].pdbr;
    pd_t* pagedirentry = &pdbr[pagediri];

    if (pagedirentry->pd_pres == 0){
        kprintf("P<%d>:: SEGMENTATION_FAULT\n", currpid);
        kill(currpid);
        return;
    }

    pt_t* ptable = (pt_t*)(pagedirentry->pd_base << 12);
    pt_t* pagetablentry = &ptable[pagetablei];

    if (pagetablentry->pt_pres == 0 && pagetablentry->pt_avail == 0){
        kprintf("P<%d>:: SEGMENTATION_FAULT\n", currpid);
        kill(currpid);
        return;
    }

    if (pagetablentry->pt_pres == 0 && pagetablentry->pt_avail == 1){
        uint32 newphyframe = new_ffs_frame();
            if (newphyframe == (uint32)SYSERR){
                kill(currpid);
                return;
            }
        
        pagetablentry->pt_base = newphyframe >> 12;
        pagetablentry->pt_pres = 1;
        pagetablentry->pt_write = 1;
        pagetablentry->pt_user = 1;
        pagetablentry->pt_avail = 0;  //clear  
        
        memset((void*)newphyframe, 0, 4096);
        write_cr3(proctab[currpid].pdbr); //reloads it
    }

     kprintf("should never get here, process: %d\n", currpid);
    return;
}