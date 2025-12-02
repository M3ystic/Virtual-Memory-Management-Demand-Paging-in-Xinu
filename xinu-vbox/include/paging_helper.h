#ifndef PAGING_HELPER_H
#define PAGING_HELPER_H


#define XINU_PAGE_DIRECTORY_LENGTH 1024
#define PAGE_TABLE_ENTRIES 1024
#define PDPT_SIZE (PAGE_SIZE * (XINU_PAGE_DIRECTORY_LENGTH + 1)) // 8 page tables + 1 PD

//physical mem
#define PAGE_TABLE_AREA_START_ADDRESS 0x20000000
#define PAGE_TABLE_AREA_END_ADDRESS   0x21FFFFFF   // +32 MB

#define FFS_START (PAGE_TABLE_AREA_END_ADDRESS + 1)
#define FFS_END    (FFS_START + MAX_FFS_SIZE*PAGE_SIZE - 1)

// virtual mem: move heap above PT area
#define USER_HEAP_START  0x02000000
#define USER_HEAP_END    0x1FFFFFFF   // big heap, ends in the 0x1*** range

bool8 pt_used[XINU_PAGE_DIRECTORY_LENGTH + 1]; // this table is used to track phys page availability

extern uint32 kernels_directory;

#endif