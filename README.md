# Virtual-Memory-Management-Demand-Paging-in-Xinu

## What This Project Does

This project adds a full virtual memory and paging system to the Xinu operating system.

The implementation gives each user process its own virtual address space, its own private virtual heap, and support for demand paging. Memory is allocated lazily, meaning physical frames are only assigned when a process actually accesses a virtual page. A custom page fault handler is added to map frames from a managed physical frame pool.

The project includes:

- **Per-process virtual heaps** backed by paging
- **vmalloc()** and **vfree()** for allocating and freeing virtual heap memory
- **Lazy allocation**: pages are reserved virtually but not backed until touched
- **Page fault handling** for bringing in heap pages or terminating illegal accesses
- **Initialization of page directory and page tables** for each process
- **A dedicated physical frame space (FFS)** used only for user heap pages
- **Shared kernel memory mappings** at the upper portion of the virtual address space
- **Debug helpers** to inspect frame usage and allocated virtual pages
- **Optional swapping** using a simple approximate LRU eviction policy

In short, the system extends Xinu with:
- A 4 KB paged virtual memory layout
- Separate user and kernel address regions
- Fully managed page directories and page tables
- Safe handling of invalid memory accesses
- Per-process memory isolation

This creates a working virtual memory subsystem capable of running user processes with independently managed heaps while maintaining stability, safety, and correct memory mapping behavior.

//changed
xinu.h
paging.h
paging_help.h
meminit.c
i386.c
control_reg.c
pagefault_handler_disp.S
initalize.c
prototypes.h
