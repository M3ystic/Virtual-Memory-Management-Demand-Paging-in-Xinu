#include <xinu.h>


uint32 next_free_pt_frame = PAGE_TABLE_AREA_START_ADDRESS;
uint32 first_page_directory;
ffs_t *ffs = NULL;

uint32* get_next_free_pt_frame()
{
    // look for available page
    uint32 cnt = 0;
    for (cnt = 0; cnt < XINU_PAGES; cnt++) {
        
        if (pt_used[cnt] == FALSE) {
            pt_used[cnt] = TRUE;
            //kprintf("get frame %d at address %08X\n", cnt, (cnt * PAGE_SIZE) + PAGE_TABLE_AREA_START_ADDRESS);
            return (uint32*)((cnt * PAGE_SIZE) + PAGE_TABLE_AREA_START_ADDRESS);
        }
    }
    return NULL;
}

void init_pt_used(void) {
    memset(pt_used, FALSE, sizeof(pt_used));
}

// initialize ffs for all processes
void init_ffs()
{
    ffs = (ffs_t *)getmem(sizeof(ffs_t));
    ffs->base_frame = (PAGE_TABLE_AREA_END_ADDRESS + 1) >> 12;
    ffs->nframes    = MAX_FFS_SIZE;
    ffs->table      = (ffs_entry_t *)getmem(sizeof(ffs_entry_t) * MAX_FFS_SIZE);

    uint32 i;
    for (i = 0; i < ffs->nframes; i++) {
        ffs->table[i].fe_used  = 0;
        ffs->table[i].fe_dirty = 0;
        ffs->table[i].fe_type  = 0;
        ffs->table[i].fe_pid   = 0;
        ffs->table[i].fe_res   = 0;
    }
}
// allocate new page directory table
pd_t* alloc_new_pd()
{
    // get and initialize one new PD table for this process
    pd_t* pdbr =  (pd_t* )get_next_free_pt_frame();
    if (pdbr == NULL) {
        kprintf("alloc_new_pd: no free frame for PD\n");
        return NULL;
    }
    memset(pdbr, 0, PAGE_SIZE);

    // only 2^3 = 8 PD entries is used, the rest will be marked not present
    uint8 pd_index;
    for (pd_index = 0; pd_index < XINU_PAGE_DIRECTORY_LENGTH; pd_index++) {
        pt_t* page_table = (pt_t*)get_next_free_pt_frame();
        if (page_table == NULL) {
            kprintf("alloc_new_pd: no free frame for PT\n");
            return NULL;
        }
        //memset(page_table, 0, PAGE_SIZE);
        pdbr[pd_index].pd_pres = 1;
        pdbr[pd_index].pd_write = 1;
        pdbr[pd_index].pd_user = 1;
        pdbr[pd_index].pd_base = ((uint32)page_table) >> 12;
    }
    // clear out all other PD entries
    uint32 i = 0;
    for (i = XINU_PAGE_DIRECTORY_LENGTH; i < 1024; i++) {
        pdbr[i].pd_pres = 0;
        pdbr[i].pd_base = 0;
    }
    return pdbr;
}

void page_table_init()
{
    // get first address for the page directory that a user process will start from
    pd_t* pdbr =  (pd_t* )get_next_free_pt_frame();

    memset(pdbr, 0, PAGE_SIZE);

    first_page_directory =  (uint32)pdbr;

    // maps current kernel memory
    uint8 pd_index;
    for (pd_index = 0; pd_index < XINU_PAGE_DIRECTORY_LENGTH; pd_index++){

        pt_t* page_table =  (pt_t* )get_next_free_pt_frame();
        memset(page_table, 0, PAGE_SIZE); // clear the page table

        pdbr[pd_index].pd_pres = 1;
        pdbr[pd_index].pd_write = 1;
        pdbr[pd_index].pd_user = 0;
        pdbr[pd_index].pd_base = ((uint32)page_table) >> 12; 

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

    // map pt-area
    uint32 pd_start = PAGE_TABLE_AREA_START_ADDRESS >> 22;
    uint32 pd_end   = PAGE_TABLE_AREA_END_ADDRESS   >> 22;

    uint32 pdi;
    for (pdi = pd_start; pdi <= pd_end; pdi++) {

        // if already present (unlikely for pdi < XINU_PAGE_DIRECTORY_LENGTH) skip
        if (pdbr[pdi].pd_pres) {
            continue;
        }

        pt_t *page_table = (pt_t *) get_next_free_pt_frame();
        if (page_table == NULL) {
            kprintf("page_table_init: out of frames creating kernel PT for pdi=%u\n", pdi);
            return;
        }
        memset(page_table, 0, PAGE_SIZE);

        // Point PDE to this PT (kernel mappings — supervisor only)
        pdbr[pdi].pd_pres  = 1;
        pdbr[pdi].pd_write = 1;
        pdbr[pdi].pd_user  = 0;   /* supervisor */
        pdbr[pdi].pd_base  = ((uint32)page_table) >> 12;

        uint32 vbase = pdi << 22;   /* virtual base of this pde 4mb */
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

    // mark all other PDEs not present
    /*
    uint32 s;
    for (s = XINU_PAGE_DIRECTORY_LENGTH; s < 1024; s++) {
    }*/

    kprintf("page_table_init: kernel PD at %08X, mapped PDs 0..%u and PT-area PDs %u..%u\n",
            (uint32)pdbr, XINU_PAGE_DIRECTORY_LENGTH - 1, pd_start, pd_end);
}


void dump_pd() {
    pd_t *pd = (pd_t *)first_page_directory;
    uint32 i;
    for(i = 0; i < 1024; i++) {
        if(pd[i].pd_pres) {
            kprintf("PDE[%d] -> PT @ %08X\n", 
                     i, pd[i].pd_base << 12);
        }
    }
}

void dump_pt(void)
{
    pd_t *pd = (pd_t *)first_page_directory;
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

uint32 free_ffs_pages(){
    uint32 free = 0;
    uint32 i;

    for (i = 0; i < ffs->nframes; i++) {
        if (ffs->table[i].fe_used == 0) {
            free++;
        }
    }
    return free;
}
uint32 used_ffs_frames(pid32 pid){
    uint32 used = 0;
    uint32 i = 0;
    for (i = 0; i < ffs->nframes; i++) {
        if (ffs->table[i].fe_used == 1 &&
            ffs->table[i].fe_pid == pid)
        {
            used++;
        }
    }

    return used;
}
uint32 allocated_virtual_pages(pid32 pid){ 
    
    struct procent *prptr = &proctab[pid];

    uint32 heap_pages  = prptr->vhpnpages;

    uint32 stack_pages = (prptr->prstklen + PAGE_SIZE - 1) / PAGE_SIZE;

    return heap_pages + stack_pages;
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
