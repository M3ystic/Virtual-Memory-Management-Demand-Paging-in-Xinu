#include <xinu.h>
#include <stdarg.h>

uint32 next_free_pt_frame = PAGE_TABLE_AREA_START_ADDRESS;
uint32 kernels_directory;
bool8 page_in_use[XINU_PAGES];      // this table is used to track phys page availability
bool8 ffs_in_use[MAX_FFS_SIZE];
bool8 ss_in_use[MAX_SWAP_SIZE];

//////////////////////////////////////
//////////////////////////////////////
uint32* get_next_free_page()
{
    // look for available page
    uint32 cnt = 0;
    for (cnt = 0; cnt < XINU_PAGES; cnt++) {
        if (page_in_use[cnt] == FALSE) {
            page_in_use[cnt] = TRUE;
            return (uint32*)((cnt * PAGE_SIZE) + PAGE_TABLE_AREA_START_ADDRESS);
        }
    }
    return NULL; 
}
/////////////////////////////////
/////////////////////////////////
void page_table_init()
{
    //get first address for the page directory that a user process will start from
    pd_t* pdbr =  (pd_t* )get_next_free_page();

    memset(pdbr, 0, PAGE_SIZE);

    kernels_directory =  (uint32)pdbr;

    // identity map kernel low-memory area
    uint8 pd_index;
    for (pd_index = 0; pd_index < XINU_PAGE_DIRECTORY_LENGTH; pd_index++){

        // allocate a page table for each pd entry
        pt_t* page_table =  (pt_t* )get_next_free_page();
        memset(page_table, 0, PAGE_SIZE);

        pdbr[pd_index].pd_pres = 1;
        pdbr[pd_index].pd_write = 1;
        pdbr[pd_index].pd_user = 0;
        pdbr[pd_index].pd_base = ((uint32)page_table) >> 12; 
        
        // fill page table entires with identity mappings
        uint16 pt_index;
        for (pt_index = 0; pt_index < PAGE_TABLE_ENTRIES; pt_index++)
        {   
            uint32 phy_frame = (pd_index * 1024 + pt_index) * PAGE_SIZE; // gives index into PT AREA
            page_table[pt_index].pt_pres = 1;
            page_table[pt_index].pt_write = 1;
            page_table[pt_index].pt_user = 0;
            page_table[pt_index].pt_base = ((uint32)phy_frame) >> 12; 
        }
    }

    
    // map null process table area
    uint32 pd_start = PAGE_TABLE_AREA_START_ADDRESS >> 22;
    uint32 pd_end   = PAGE_TABLE_AREA_END_ADDRESS   >> 22;

    uint32 pdi;
    for (pdi = pd_start; pdi <= pd_end; pdi++) {

        // if already present (unlikely for pdi < XINU_PAGE_DIRECTORY_LENGTH) skip
        if (pdbr[pdi].pd_pres) {
            continue;
        }

        pt_t *page_table = (pt_t *) get_next_free_page();
        if (page_table == NULL) {
            kprintf("page_table_init: out of frames creating kernel PT for pdi=%u\n", pdi);
            return;
        }
        memset(page_table, 0, PAGE_SIZE);

        // Point PDE to this PT (kernel mappings)
        pdbr[pdi].pd_pres  = 1;
        pdbr[pdi].pd_write = 1;
        pdbr[pdi].pd_user  = 0;   
        pdbr[pdi].pd_base  = ((uint32)page_table) >> 12;

        uint32 vbase = pdi << 22;   
        uint32 pti;
        for (pti = 0; pti < PAGE_TABLE_ENTRIES; pti++) {
            uint32 vaddr = vbase + (pti << 12);
            uint32 phys = vaddr;
            page_table[pti].pt_pres  = 1;
            page_table[pti].pt_write = 1;
            page_table[pti].pt_user  = 0;  
            page_table[pti].pt_base  = phys >> 12;
        }
    } 
}

void init_pt_used(void) {
    memset(page_in_use, FALSE, sizeof(page_in_use));
}

/////////////////////////////////
/////////////////////////////////
// allocate new page directory table
pd_t* alloc_new_pd()
{
    pd_t* newpd = get_next_free_page();
    memset(newpd, 0, PAGE_SIZE);

    pd_t* kpd = (pd_t*)kernels_directory;

    // Copy ONLY the kernel PDE entries (typically 0–7, and maybe a few high ones)
    int i;
    for (i = 0; i < XINU_PAGE_DIRECTORY_LENGTH; i++) {
        newpd[i] = kpd[i];
        newpd[i].pd_user = 0;
    }
    for (i = XINU_PAGE_DIRECTORY_LENGTH; i < 1024; i++) {
        newpd[i].pd_pres = 0;
        newpd[i].pd_write = 0;
        newpd[i].pd_user = 0;
        newpd[i].pd_base = 0;
    }
    return newpd;
}
///////////////////////////////////
///////////////////////////////////
void init_heap(struct procent* proc)
{
    /*
    // start of heap
    struct memblk* heapstartedge = (struct memblk*)proc->heapstart;

    // initialize 
    struct memblk* startofheap = (struct memblk*)(proc->heapstart + PAGE_SIZE);

    proc->heapmlist = heapstartedge;
    heapstartedge->mnext = startofheap;
    heapstartedge->mlength = 0; //not used

    startofheap->mnext = NULL;
    startofheap->mlength = (uint32)(proc->heapend - (char*)startofheap);*/
    
    // kernel heap  
    proc->heapmlist = (struct memblk*) getmem(sizeof(struct memblk));
    proc->heapmlist->mlength = 0;
    proc->heapmlist->mnext = (struct memblk*) getmem(sizeof(struct memblk));

    // Free block representing the entire virtual heap
    struct memblk* freeblk = proc->heapmlist->mnext;
    freeblk->mlength = (uint32)(proc->heapend - proc->heapstart); // full heap size
    freeblk->mnext = NULL;

    freeblk->vaddr = proc->heapstart;  // add vaddr field to memblk

    kprintf("finished init heap\n");

}
///////////////////////////////////
///////////////////////////////////
void reservespace(uint32 va_start, uint32 pages, pid32 pid)
{
    pd_t* pdir = (pd_t*)proctab[pid].pdbr;

    uint32 i;
    for (i = 0; i < pages; i++){
        uint32 va = va_start + (i * PAGE_SIZE);

        virt_addr_t* vaddr = (virt_addr_t*)&va;

        uint32 pd_index = vaddr->pd_offset;
        uint32 pt_index = vaddr->pt_offset;

        pd_t* pagedirentry = &pdir[pd_index];
        if (pagedirentry->pd_pres == 0){
            pt_t* new_pt = (pt_t*)get_next_free_page();
            memset(new_pt, 0, PAGE_SIZE);

            pagedirentry->pd_pres = 1;
            pagedirentry->pd_write = 1;
            pagedirentry->pd_user = 1;
            pagedirentry->pd_base = ((uint32)new_pt) >> 12; 
        }
        else {
            pt_t *ptable = (pt_t*)(pagedirentry->pd_base << 12);
            pt_t* pagetableentry = &ptable[pt_index];

            pagetableentry->pt_pres  = 0;
            pagetableentry->pt_avail = 1;
            pagetableentry->pt_write = 1;
            pagetableentry->pt_user  = 1;
            pagetableentry->pt_base  = 0; //dont know
        }


    }
}

void freeffsframe(uint32 frameaddr)
{
    if (frameaddr < FFS_START || frameaddr >= FFS_END)
        return; 
    uint32 frameindex = (frameaddr - FFS_START) / PAGE_SIZE;
    ffs_in_use[frameindex] = FALSE;
}

void free_page_frame(uint32 frameaddr)
{
    if (frameaddr < PAGE_TABLE_AREA_START_ADDRESS || frameaddr >= PAGE_TABLE_AREA_END_ADDRESS)
        return; 
    uint32 frameindex = (frameaddr - PAGE_TABLE_AREA_START_ADDRESS) / PAGE_SIZE;
    page_in_use[frameindex] = FALSE;
}

///////////////////////////////////
///////////////////////////////////
pid32 create_help (void *funcaddr, uint32 ssize, pri16 priority, char *name,
uint32 nargs, va_list arguments)
{
    uint32 n[nargs];
    int i; for (i = 0; i < nargs; i++) n[i] = va_arg(arguments, uint32);
    
    switch(nargs){
        case 0: return create(funcaddr, ssize, priority, name, 0);
        case 1: return create(funcaddr, ssize, priority, name, 1, n[0]);
        case 2: return create(funcaddr, ssize, priority, name, 2, n[0], n[1]);
        case 3: return create(funcaddr, ssize, priority, name, 3, n[0], n[1], n[2]);
        case 4: return create(funcaddr, ssize, priority, name, 4, n[0], n[1], n[2], n[3]);
        case 5: return create(funcaddr, ssize, priority, name, 5, n[0], n[1], n[2], n[3], n[4]);
        case 6: return create(funcaddr, ssize, priority, name, 6, n[0], n[1], n[2], n[3], n[4], n[5]);
        case 7: return create(funcaddr, ssize, priority, name, 7, n[0], n[1], n[2], n[3], n[4], n[5], n[6]);
        case 8: return create(funcaddr, ssize, priority, name, 8, n[0], n[1], n[2], n[3], n[4], n[5], n[6], n[7]);
        case 9: return create(funcaddr, ssize, priority, name, 9, n[0], n[1], n[2], n[3], n[4], n[5], n[6], n[7], n[8]);
        case 10: return create(funcaddr, ssize, priority, name, 10, n[0], n[1], n[2], n[3], n[4], n[5], n[6], n[7], n[8], n[9]);
        default: return SYSERR;
    }
    return SYSERR;
}     
///////////////////////////////////
///////////////////////////////////
pid32 vcreate (void *funcaddr, uint32 ssize, pri16 priority, char *name,
uint32 nargs, ...)
{
    //from stdargs, read variable arguments
    va_list arguments;
    va_start(arguments, nargs);

    // create stack and process
    pid32 pid = create_help(funcaddr, ssize, priority, name, nargs, arguments);   
    va_end(arguments);
    
    if (pid == SYSERR) return SYSERR;

    // initialize heap memory list
    init_heap(&proctab[pid]);
    return pid;
}
///////////////////////////////////
///////////////////////////////////
char* vmalloc (uint32 nbytes)
{
    if (nbytes == 0)
        return NULL;

    // round nearest page 
    if (nbytes % PAGE_SIZE != 0)
        nbytes = ((nbytes / PAGE_SIZE) + 1) * PAGE_SIZE;

    struct procent* proc = &proctab[currpid];
    struct memblk* prev = proc->heapmlist;
    struct memblk* curr = prev->mnext;

    while (curr != NULL) {
        if (curr->mlength >= nbytes) {
            uint32 va_start = curr->vaddr;
            uint32 pages = nbytes / PAGE_SIZE;

            if (curr->mlength == nbytes) {
                prev->mnext = curr->mnext;
            } else {
                // split block
                struct memblk* newblk = (struct memblk*) getmem(sizeof(struct memblk));
                newblk->mlength = curr->mlength - nbytes;
                newblk->vaddr = curr->vaddr + nbytes;
                newblk->mnext = curr->mnext;
                prev->mnext = newblk;
                curr->mlength = nbytes;
            }
            
            // reserve space in page tables
            reservespace(va_start, pages, currpid);
            proc->allocvpages += pages;

            return (char*) va_start;
        }

        prev = curr;
        curr = curr->mnext;
    }
    // no block found
    return (char*) SYSERR;
         
}
///////////////////////////////////
///////////////////////////////////
syscall vfree(char *ptr, uint32 bytes)
{
    struct procent *process = &proctab[currpid];

    if (bytes == 0)
        return SYSERR;

    //round to page
    if (bytes % PAGE_SIZE != 0)
        bytes = ((bytes + PAGE_SIZE - 1) / PAGE_SIZE) * PAGE_SIZE;

    uint32 virtual_start = (uint32)ptr;
    uint32 virtual_end   = virtual_start + bytes;

    //check range and alignmnet
    if (virtual_start < (uint32)process->heapstart || virtual_end > (uint32)process->heapend)
        return SYSERR;

    if (virtual_start % PAGE_SIZE != 0)
        return SYSERR;

    uint32 pages = bytes / PAGE_SIZE;
    pd_t *pagedir = (pd_t *)process->pdbr;

    /* validate region */
    uint32 i;                        // moved out of loop
    uint32 virtual_address;          // moved out of loop
    virt_addr_t *virtaddr;           // moved out of loop
    pd_t *pagedirentry;              // moved out of loop
    pt_t *pagetable;                 // moved out of loop
    pt_t *pagetableentry;            // moved out of loop

    for (i = 0; i < pages; i++) {
        virtual_address = virtual_start + i * PAGE_SIZE;
        virtaddr = (virt_addr_t *)&virtual_address;

        pagedirentry = &pagedir[virtaddr->pd_offset];
        if (pagedirentry->pd_pres == 0)
            return SYSERR;

        pagetable = (pt_t *)(pagedirentry->pd_base << 12);
        pagetableentry = &pagetable[virtaddr->pt_offset];

        //not mapped or reserved
        if (pagetableentry->pt_pres == 0 && pagetableentry->pt_avail == 0)
            return SYSERR;
    }

    //insert free space and coalesce
    struct memblk *newblock = (struct memblk *)ptr;
    newblock->mlength = bytes;

    struct memblk *previousblock = process->heapmlist;
    struct memblk *currentblock = previousblock->mnext;

    while (currentblock != NULL && currentblock < newblock) {
        previousblock = currentblock;
        currentblock = currentblock->mnext;
    }

    uint32 prev_end = (uint32)previousblock + previousblock->mlength;
    uint32 curr_start = (currentblock == NULL ?
                         (uint32)process->heapend :
                         (uint32)currentblock);
    uint32 new_end = virtual_end;

    if (prev_end > virtual_start || new_end > curr_start)
        return SYSERR;

    newblock->mnext = currentblock;
    previousblock->mnext = newblock;

    //coalesce 
    if (new_end == curr_start) {
        newblock->mlength += currentblock->mlength;
        newblock->mnext = currentblock->mnext;
    }

    //coalesce with previous
    if (previousblock != process->heapmlist) {
        prev_end = (uint32)previousblock + previousblock->mlength;
        if (prev_end == virtual_start) {
            previousblock->mlength += newblock->mlength;
            previousblock->mnext = newblock->mnext;
            newblock = previousblock;
        }
    }

    //free frames 
    for (i = 0; i < pages; i++) {
        virtual_address = virtual_start + i * PAGE_SIZE;
        virtaddr = (virt_addr_t *)&virtual_address;

        pagedirentry = &pagedir[virtaddr->pd_offset];
        pagetable = (pt_t *)(pagedirentry->pd_base << 12);
        pagetableentry = &pagetable[virtaddr->pt_offset];

        if (pagetableentry->pt_pres == 1) {
            uint32 physical_frame_address = pagetableentry->pt_base << 12;
            if (physical_frame_address >= FFS_START && physical_frame_address < FFS_END)
                freeffsframe(physical_frame_address);
        }

        /* Correct lazy reset */
        pagetableentry->pt_pres  = 0;
        pagetableentry->pt_avail = 1;
        pagetableentry->pt_base  = 0;
    }

    process->allocvpages -= pages;

    return OK;
}
///////////////////////////////////
///////////////////////////////////
uint32 free_ffs_pages(){
    uint32 free_count = 0;
    uint32 frame;

    for (frame = 0; frame < MAX_FFS_SIZE; frame++){
        if (ffs_in_use[frame] == FALSE)
            free_count++;
    }

    return free_count; 
}
///////////////////////////////////
///////////////////////////////////
uint32 used_ffs_frames(pid32 pid){
    uint32 used_count = 0;
    uint32 pdentry;
    pd_t* pdir = (pd_t*)proctab[pid].pdbr;

     for (pdentry = 0; pdentry < 1024; pdentry++){
        if (pdir[pdentry].pd_pres == 0)
            continue;
    
        uint32 ptaddr = pdir[pdentry].pd_base << 12;
        pt_t* ptable= (pt_t*)ptaddr;

        uint32 ptentry;

        for (ptentry = 0; ptentry < 1024; ptentry++){
        if (ptable[ptentry].pt_pres == 0)
            continue;

        uint32 frameaddr = ptable[ptentry].pt_base << 12;
        
        if (frameaddr >= FFS_START && frameaddr < FFS_END)
            used_count++;
     }
    }
    return used_count;
}
///////////////////////////////////
///////////////////////////////////
uint32 allocated_virtual_pages(pid32 pid){
    return proctab[pid].allocvpages;
}
///////////////////////////////////
///////////////////////////////////
uint32 new_ffs_frame(){
    uint32 frame;
    for (frame = 0; frame < MAX_FFS_SIZE; frame++){
        if (ffs_in_use[frame] == FALSE){
            ffs_in_use[frame] = TRUE;
            return (frame * PAGE_SIZE) + FFS_START;
        }
    }
    return SYSERR;
}
///////////////////////////////////
///////////////////////////////////
void dump_pd() {
    pd_t *pd = (pd_t *)kernels_directory;
    int i;
    for(i=0; i<1024; i++) {
        if(pd[i].pd_pres) {
            kprintf("PDE[%d] -> PT @ %08X\n", 
                     i, pd[i].pd_base << 12);
        }
    }
}
///////////////////////////////////
///////////////////////////////////
void dump_pt(void)
{
    pd_t *pd = (pd_t *)kernels_directory;
    uint32 pd_index = 0;
    for (pd_index = 0; pd_index < XINU_PAGE_DIRECTORY_LENGTH; pd_index++) {

        if (pd[pd_index].pd_pres == 0)
            continue;

        uint32 pt_addr = pd[pd_index].pd_base << 12;
        pt_t *pt = (pt_t *)pt_addr;

        kprintf("\n--- Page Table for PDE[%d] at 0x%08X ---\n",
                pd_index, pt_addr);

        uint32 pt_index = 0;
        for (pt_index = 0; pt_index < PAGE_TABLE_ENTRIES; pt_index++) {

            uint32 frame_addr = pt[pt_index].pt_base << 12;
            
            // print every 128 entries
            if ((pt_index % 128) == 0) {
                kprintf("  PTE[%4d] -> frame 0x%08X (P=%d U=%d W=%d)\n",
                        pt_index,
                        frame_addr,
                        pt[pt_index].pt_pres,
                        pt[pt_index].pt_user,
                        pt[pt_index].pt_write
                );
            }
        }
    }
}

// 0x02000000  ------------------------+
//                                     |
//             PAGE DIRECTORY          |
//             1024 PDEs               |
//             (4 KB)                  |
//                                     |
// 0x02000FFF  ------------------------+
// 0x02001000  ------------------------+
//                                     |
//             PAGE TABLE 0            |
//             1024 PTEs               |
//             (4 KB)                  |
//                                     |
// 0x02001FFF  ------------------------+
// 0x02002000  ------------------------+
//                                     |
//             PAGE TABLE 1            |
//             (4 KB)                  |
//                                     |
// 0x02002FFF  ------------------------+
