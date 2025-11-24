#include <xinu.h>
#include <stdarg.h>


uint32 next_free_pt_frame = PAGE_TABLE_AREA_START_ADDRESS;
uint32 kernels_directory;
bool8 page_in_use[XINU_PAGES];      // this table is used to track phys page availability

/*
uint32* get_next_free_pt_frame()
{
    uint32* addr = (uint32*)next_free_pt_frame;
    next_free_pt_frame = next_free_pt_frame + PAGE_SIZE;
    return addr;
}*/
////////////////////////////////////////
//////////////////////////////////////
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
/////////////////////////////////
/////////////////////////////////
void page_table_init()
{
    //get first address for the page directory that a user process will start from
    pd_t* pdbr =  (pd_t* )get_next_free_pt_frame();

    memset(pdbr, 0, PAGE_SIZE);

    kernels_directory =  (uint32)pdbr;

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
/////////////////////////////////
/////////////////////////////////
// allocate new page directory table
pd_t* alloc_new_pd()
{
    // get and initialize new PD table for this process
    pd_t* new_pdbr =  (pd_t* )get_next_free_pt_frame();
    if (new_pdbr == NULL) {
        kprintf("no free frames for page directory\n");
    }
    memset(new_pdbr, 0, PAGE_SIZE);

    pd_t* kernels_directory_casted = (pd_t*)(kernels_directory);   //cast  it into pd type

    // just need to copy kernel entries into users adress space
    uint8 pd_index;
    for (pd_index = 0; pd_index < XINU_PAGE_DIRECTORY_LENGTH; pd_index++) {
        new_pdbr[pd_index]= kernels_directory_casted[pd_index];    // now this new page diectory has xinus
    }

    return new_pdbr;
}
///////////////////////////////////
///////////////////////////////////
void init_heap(struct procent* proc)
{

     struct memblock* startofheap = (struct memblock*)proc->heapstart;
     startofheap->nextblock = NULL;
     startofheap->blocklength = (uint32)(proc->heapend - proc->heapstart);  //should be 128MB

     proc->heapmlist = startofheap;
}
/////////////
/////////////

// pid32 create_help (void *funcaddr, uint32 ssize, pri16 priority, char *name,
// uint32 nargs, va_list arguments)
// {
//     uint32 n[nargs];
//     int i; for (i = 0; i < nargs; i++) n[i] = va_arg(arguments, uint32);

//     pid32 returnpid = create()

// }      i dont know how to get by the variable arguments

pid32 vcreate (void *funcaddr, uint32 ssize, pri16 priority, char *name,
uint32 nargs, ...)
{
    //from stdargs
    // va_list arguments;
    // va_start(arguments, nargs);

    pid32 pid = create(funcaddr, ssize, priority, name, nargs, /*arguments*/   );   //create stack

    
    if (pid == SYSERR) return SYSERR;

    pd_t* new_page_dir = alloc_new_pd();
    proctab[pid].pdbr = (uint32)new_page_dir;

    proctab[pid].heapstart = (char*)USER_HEAP_START;
    proctab[pid].heapend = (char*)USER_HEAP_END;

    init_heap(&proctab[pid]);

    return pid;
}
/////////////////////////////////
//vmalloc


//vfree


//pagefault.c will be what actaully allocates physical frames





















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
