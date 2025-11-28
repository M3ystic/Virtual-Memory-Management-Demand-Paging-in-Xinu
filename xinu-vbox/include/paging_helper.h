#ifndef PAGING_HELPER_H
#define PAGING_HELPER_H


#define XINU_PAGE_DIRECTORY_LENGTH 8
#define PAGE_TABLE_ENTRIES 1024
#define PDPT_SIZE (PAGE_SIZE * (XINU_PAGE_DIRECTORY_LENGTH + 1)) // 8 page tables + 1 PD

//physical mem
#define PAGE_TABLE_AREA_START_ADDRESS 0x02000000  // 32 MB   //physical memory
#define PAGE_TABLE_AREA_END_ADDRESS   0x03FFFFFF  // 64 MB - 1

#define FFS_START  0x04000000   // 64MB
#define FFS_END    (FFS_START + MAX_FFS_SIZE*PAGE_SIZE - 1)


//virtual mem
#define USER_HEAP_START  0x02000000   //start at 32MB
#define USER_HEAP_END   0x12000000   //end start to end is 256MB

extern uint32 kernels_directory;

#endif