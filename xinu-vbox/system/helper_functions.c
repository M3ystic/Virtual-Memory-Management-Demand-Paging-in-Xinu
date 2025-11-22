#include <xinu.h>


uint32 next_free_pt_frame = PAGE_TABLE_AREA_START_ADDRESS;
uint32 first_page_directory;
bool8 page_in_use[XINU_PAGES];      // this table is used to track phys page availability

/*
uint32* get_next_free_pt_frame()
{
    uint32* addr = (uint32*)next_free_pt_frame;
    next_free_pt_frame = next_free_pt_frame + PAGE_SIZE;
    return addr;
}*/

uint32* get_next_free_pt_frame()
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

void page_table_init()
{
    //get first address for the page directory that a user process will start from
    pd_t* pdbr =  (pd_t* )get_next_free_pt_frame();

    memset(pdbr, 0, PAGE_SIZE);

    first_page_directory =  (uint32)pdbr;

    uint8 pd_index;
    for (pd_index = 0; pd_index < XINU_PAGE_DIRECTORY_LENGTH; pd_index++){

        pt_t* page_table =  (pt_t* )get_next_free_pt_frame();
        memset(page_table, 0, PAGE_SIZE);

        pdbr[pd_index].pd_pres = 1;
        pdbr[pd_index].pd_write = 1;
        pdbr[pd_index].pd_user = 1;
        pdbr[pd_index].pd_base = ((uint32)page_table) >> 12; 

        uint16 pt_index;
        for (pt_index = 0; pt_index < PAGE_TABLE_ENTRIES; pt_index++)
        {   
            uint32 phy_frame = (pd_index * 1024 + pt_index) * PAGE_SIZE; // gives index into PT AREA

            page_table[pt_index].pt_pres = 1;
            page_table[pt_index].pt_write = 1;
            page_table[pt_index].pt_user = 1;
            page_table[pt_index].pt_base = ((uint32)phy_frame) >> 12; 
        }
    }
}

// allocate new page directory table
void alloc_new_pd()
{
    // get and initialize new PD table for this process
    pd_t* pdbr =  (pd_t* )get_next_free_pt_frame();
    if (pdbr == NULL) {
        kprintf("no free frames for page directory\n");
    }
    memset(pdbr, 0, PAGE_SIZE);

    // only 8 PD entries is used, the rest will be marked as not present
    uint8 pd_index;
    for (pd_index = 0; pd_index < XINU_PAGE_DIRECTORY_LENGTH; pd_index++) {
        pt_t* page_table = (pt_t*)get_next_free_pt_frame();
        if (page_table == NULL) {
            kprintf("no free frames for page table at PDE[%d]\n", pd_index);
        }
        pdbr[pd_index].pd_pres = 1;
        pdbr[pd_index].pd_write = 1;
        pdbr[pd_index].pd_user = 1;
        pdbr[pd_index].pd_base = ((uint32)page_table) >> 12;
    }
}

void dump_pd() {
    pd_t *pd = (pd_t *)first_page_directory;
    int i;
    for(i=0; i<1024; i++) {
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
    return 0;
}
uint32 used_ffs_frames(pid32 pid){
    return 0;
}
uint32 allocated_virtual_pages(pid32 pid){
    return 0;
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
