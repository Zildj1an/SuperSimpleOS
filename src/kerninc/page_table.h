/*
 * `````````````````````````````````````````````````````````````````````````````
 *  This kernel has its own 4-level (4KB pages) page table entry system.
 *
 * `````````````````````````````````````````````````````````````````````````````
*/

#ifndef __KPAGE_TABLE_H__
#define __KPAGE_TABLE_H__

typedef unsigned long long u64;

/* 4KB PTE (See AMD64 manual [1]) */
struct page_pte {

	u64 present:1;        /* Present Bit                            */
	u64 writable:1;       /* Read/Write Bit                         */
	u64 user_mode:1;      /* User/Supervisor Bit (0 for privilege)  */
	u64 writethrough:1;   /* Page Writethrough Bit                  */
	u64 pcd:1;		      /* Page-Level Cache Disable Bit           */
	u64 accessed:1;		  /* Accessed Bit                           */
	u64 dirty:1;		  /* Dirty Bit                              */
	u64 pat:1;		      /* Page Attribute Bit                     */
	u64 global:1;		  /* Global Page Bit                        */
	u64 avail_1:3;        /* Reserved                               */
	u64 page_address:40;  /* 40+12 = 52-bit physical address (max)  */
	u64 avail_2:7;        /* Reserved, should be zero               */
	u64 pke:4;            /* No MPK/PKE, should be zero             */
	u64 nonexecute:1;
};

/* 4KB PDE ([1]) */
struct page_pde {

	u64 present:1;        /* Present Bit                            */
	u64 writable:1;       /* Read/Write Bit                         */
	u64 user_mode:1;      /* User/Supervisor Bit                    */
	u64 writethrough:1;   /* Page Writethrough Bit                  */
	u64 pcd:1;		      /* Page-Level Cache Disable Bit           */
	u64 accessed:1;		  /* Accessed Bit                           */
	u64 ignored_1:1;	  /* Ignored Bit                            */
	u64 zero:1;		      /* Should be zero                         */
	u64 ignored_2:1;      /* Ignored  Bit                           */
	u64 avail_1:3;        /* Reserved                               */
	u64 pte_address:40;   /* Page Table Entry address               */
	u64 avail_2:11;       /* Reserved, should be zero               */
	u64 nonexecute:1;
};

/* 4KB PDPE ([1]) */
struct page_pdpe {

	u64 present:1;        /* Present Bit                            */
	u64 writable:1;       /* Read/Write Bit                         */
	u64 user_mode:1;      /* User/Supervisor Bit                    */
	u64 writethrough:1;   /* Page Writethrough Bit                  */
	u64 pcd:1;		      /* Page-Level Cache Disable Bit           */
	u64 accessed:1;		  /* Accessed Bit                           */
	u64 ignored_1:1;	  /* Ignored Bit                            */
	u64 zero:1;		      /* Must be zero                           */
	u64 ignored_2:1;      /* Ignored  Bit                           */
	u64 avail_1:3;        /* Reserved                               */
	u64 pde_address:40;   /* Page Directory Entry address           */
	u64 avail_2:11;       /* Reserved, should be zero               */
	u64 nonexecute:1;
};

/* 4KB PML4E ([1]) */
struct page_pml4e {

	u64 present:1;        /* Present Bit                            */
	u64 writable:1;       /* Read/Write Bit                         */
	u64 user_mode:1;      /* User/Supervisor Bit                    */
	u64 writethrough:1;   /* Page Writethrough Bit                  */
	u64 pcd:1;		      /* Page-Level Cache Disable Bit           */
	u64 accessed:1;		  /* Accessed Bit                           */
	u64 ignored_1:1;	  /* Ignored Bit                            */
	u64 mbz_1:1;		  /* Must be zero                           */
	u64 mbz_2:1;          /* Must be zero                           */
	u64 avail_1:3;        /* Reserved                               */
	u64 pdpe_address:40;  /* PDPE Entry address                     */
	u64 avail_2:11;       /* Reserved, should be zero               */
	u64 nonexecute:1;
};

/* I set my System Base memory on VirtualBox to 4096 MB (4 GB). 
   2048 PTEs + 4 PDEs + 1 PDPE + 1 PMLE4E = 2054 pages.
   2054 pages * 4 KB = 2054 * 2^12 = 8413184 Bytes (Inside boundaries!)
   2048 pages * 512 B = 1048576 page table entries.
   NUM_PAGE_TABLES and NUM_PT_ENTRIES are kernel specific.
*/
#define PAGE_ENTRIES    (512LU)
#define NUM_PAGE_TABLES (2048LU)
#define NUM_PT_ENTRIES  (NUM_PAGE_TABLES * PAGE_ENTRIES)

typedef struct __attribute__((__packed__)) {

	struct page_pte   pagetable_entries[NUM_PT_ENTRIES];
	struct page_pde   pde[NUM_PAGE_TABLES];
	struct page_pdpe  pdpe[PAGE_ENTRIES];
	struct page_pml4e pml4e[PAGE_ENTRIES];

} page_tables_t;

typedef struct {

	void *ubuffer;      /* Physical address of user code                      */
	int num_pages_code; /* Number of pages used for the uer code              */
	void *ustack_phys;  /* Physical address of user stack                     */

	void *upage_fault;  /* Extra page for Lazy Allocation                     */

	void *tls;          /* A page for the TLS                                 */

	void *uinfo_pages;  /* User page table                                    */

} ucode_info_t;

void *lazy_page; /* Extra page for Lazy Allocation (allocated at boot)  */

void *tss_stack_gbl;

void set_kpage_table(page_tables_t *info_pages);

int set_upage_table(page_tables_t *kinfo_pages,
					void *info_pages,  
				    ucode_info_t *ucode_info,
				    void **ustack_virtual,
				    void **ucode_virtual,
				    void **tls_virtual);

#endif

/*
  REFERENCES

  [1] AMD64 Architecture Programmer's Manual, Volume 2 System Programming
      (https://www.amd.com/system/files/TechDocs/24593.pdf)
*/