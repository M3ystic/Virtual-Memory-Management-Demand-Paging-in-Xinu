#ifndef PAGING_HELPER_H
#define PAGING_HELPER_H

#define PAGE_TABLE_AREA_START_ADDRESS 0x02000000  // 32 MB
#define PAGE_TABLE_AREA_END_ADDRESS   0x03FFFFFF  // 64 MB - 1


#define XINU_PAGE_DIRECTORY_LENGTH 8
#define PAGE_TABLE_ENTRIES 1024
#define PDPT_SIZE (PAGE_SIZE * (XINU_PAGE_DIRECTORY_LENGTH + 1)) // 8 page tables + 1 PD

#define VHEAP_START 0x02000000  // 64 MB
#define VHEAP_END   0X12000000  // 128 MB

extern uint32 first_page_directory;

//struct to keep track of each frame in the ffs region
typedef struct ffs_entry {
    unsigned int fe_used  : 1;     /* 1 = frame allocated, 0 = free */
    unsigned int fe_dirty : 1;     /* was this frame modified?       */
    unsigned int fe_type  : 2;     /* type of data stored (optional) */
    unsigned int fe_pid   : 12;    /* owner process (if any)         */
    unsigned int fe_res   : 16;    /* reserved / future use          */
} ffs_entry_t;

// ffs table
typedef struct ffs_table {
    unsigned int base_frame;       /* starting frame number of FFS region */
    unsigned int nframes;          /* number of frames in FFS region      */
    ffs_entry_t *table;            /* pointer to array of ffs_entry_t     */
} ffs_t;

bool8 pt_used[XINU_PAGE_DIRECTORY_LENGTH + 1]; // this table is used to track phys page availability
extern ffs_t *ffs;
#endif