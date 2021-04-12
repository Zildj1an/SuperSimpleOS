/*
 * `````````````````````````````````````````````````````````````````````````````
 *  This kernel has its own 4-level (4 KB Long Mode) page table entry system.
 *  author Carlos Bilbao, bilbao at vt.edu
 *
 * `````````````````````````````````````````````````````````````````````````````
*/
#include <page_table.h>
#include <flag.h>
#include <types.h>
#include <printf.h>

void __initialize_kernel_page_entries(page_tables_t *info_pages)
{
	int i = 0;
	struct page_pte   *aux_pte, *pagetable_entries;
	struct page_pde   *aux_pde, *pde;
	struct page_pdpe  *pdpe;
	struct page_pml4e *pml4e;

	pagetable_entries = info_pages->pagetable_entries;
	pde   = info_pages->pde;
	pdpe  = info_pages->pdpe;
	pml4e = info_pages->pml4e;

	for (; i < NUM_PT_ENTRIES; ++i) {

		/* Physical pte address (Each entry is 8 bytes) */
		pagetable_entries[i].page_address = i;
		pagetable_entries[i].present      = 1; /* P       */
		pagetable_entries[i].writable     = 1; /* R/W     */
		pagetable_entries[i].user_mode    = 0; /* U/S     */
		pagetable_entries[i].writethrough = 0; /* PWT     */
		pagetable_entries[i].pcd          = 0; /* PCD     */
 		pagetable_entries[i].accessed     = 0; /* A       */
		pagetable_entries[i].dirty        = 0; /* D       */
		pagetable_entries[i].pat          = 0; /* PAT     */
		pagetable_entries[i].global       = 0; /* G       */
		pagetable_entries[i].avail_1      = 0; /* AVL     */
		pagetable_entries[i].avail_2      = 0; /* AVL     */
		pagetable_entries[i].pke          = 0; /* PKE     */
		pagetable_entries[i].nonexecute   = 0; /* NXE     */
	}

	pde = (struct page_pde *) (pagetable_entries + NUM_PT_ENTRIES);

	for (i = 0; i < NUM_PAGE_TABLES; ++i) {

		aux_pte = pagetable_entries + PAGE_ENTRIES * i;
		/* Record the page address */
		pde[i].pte_address = (u64) aux_pte >> 12;
		pde[i].present       = 1;
		pde[i].writable      = 1;
		pde[i].user_mode     = 0;
		pde[i].writethrough  = 0;
		pde[i].pcd           = 0; 
		pde[i].accessed      = 0;
		pde[i].ignored_1     = 0;
		pde[i].ignored_2     = 0;
		pde[i].avail_1       = 0;
		pde[i].avail_2       = 0;
		pde[i].zero          = 0;
		pde[i].nonexecute    = 0;
	}

	pdpe = (struct page_pdpe *) (pde + NUM_PAGE_TABLES);

	for (i = 0; i < PAGE_ENTRIES; ++i){

		aux_pde = pde + PAGE_ENTRIES * i; 

		/* Record the directory address */
		pdpe[i].pde_address = (u64) aux_pde >> 12;
		pdpe[i].present       = (i > 4)? 0 : 1;
		pdpe[i].writable      = (i > 4)? 0 : 1;
		pdpe[i].user_mode     = 0;
		pdpe[i].writethrough  = 0;
		pdpe[i].pcd           = 0;
		pdpe[i].accessed      = 0;
		pdpe[i].zero          = 0;
		pdpe[i].ignored_1     = 0;
		pdpe[i].ignored_2     = 0;
		pdpe[i].avail_1       = 0;
		pdpe[i].avail_2       = 0;
		pdpe[i].nonexecute    = 0;
	}

	pml4e = (struct page_pml4e *) (pdpe + PAGE_ENTRIES);

	for (i = 0; i < PAGE_ENTRIES; ++i){

		pml4e[i].pdpe_address  = (i)? 0 : (u64) pdpe >> 12;
		pml4e[i].present       = (i)? 0 : 1;
		pml4e[i].writable      = (i)? 0 : 1;

		/* Give the last entry to user applications (1 GB) */
		pml4e[i].user_mode     = 0;
		pml4e[i].pcd           = 0;
		pml4e[i].writethrough  = 0;
		pml4e[i].accessed      = 0;
		pml4e[i].mbz_1         = 0;
		pml4e[i].mbz_2         = 0;
		pml4e[i].ignored_1     = 0;
		pml4e[i].avail_1       = 0;
		pml4e[i].avail_2       = 0;
		pml4e[i].nonexecute    = 0;
	}
}

void __update_cr3(page_tables_t *info_pages)
{
	asm __volatile__ ("mov %0, %%cr3" 
				      : /* no output registers */
				      : "r"(info_pages->pml4e)
				      : "memory"); 
}

void set_kpage_table(page_tables_t *kinfo_pages)
{
	__initialize_kernel_page_entries(kinfo_pages);
	__update_cr3(kinfo_pages);
}

int __check_addresses(int *ustack, int *ucode, int *u_tls)
{
	int ret = 0;

	/* 2^64 - 2^13 (8 KB) = 0xffffffffffffe000 */ 
	if (ustack != (void*)0xffffffffffffe000){
		printf("Ustack addr failing (%p != 0xffffffffffffe000)\n",ustack);
		ret = 1;
	}

	/* 2^64 - 2^21 (2 MB) = 0xffffffffffe00000 */
	if (ucode != (void*)0xffffffffffe00000){
		printf("User code addr is wrong (%p != 0xffffffffffe00000)\n",ucode);
		ret = 1;
	}

	/* 2^64 - (2^13 + 2^12) (12 KB) = 0xffffffffffffd000 */
	if (ucode != (void*)0xffffffffffffd000){
		printf("User TLS addr is wrong (%p != 0xffffffffffffd000)\n",u_tls);
		ret = 1;
	}

	return ret;
}

int set_upage_table(page_tables_t *kinfo_pages,
					void *info_pages, 
				    ucode_info_t *ucode_info,
				    void **ustack_virtual,
				    void **ucode_virtual,
				    void **tls_virtual)
{
	int i = 0, ret;
	struct page_pte   *aux_pte, *pagetable_entries, *pages_kernel;
	struct page_pde   *aux_pde, *pde, *pde_kernel;
	struct page_pdpe  *pdpe, *pdpe_kernel;
	struct page_pml4e *pml4e, *pml4e_kernel;
	void *userbuffer  = ucode_info->ubuffer;
	int num_pages     = ucode_info->num_pages_code;
	void *ustack_phys = ucode_info->ustack_phys;
	void *tls_phys    = ucode_info->tls;

	pagetable_entries = info_pages;
	pages_kernel      = kinfo_pages->pagetable_entries;

	pde          = (struct page_pde *)   (pagetable_entries + PAGE_ENTRIES);
	pdpe         = (struct page_pdpe *)  (pde + PAGE_ENTRIES);
	pml4e        = (struct page_pml4e *) (pdpe + PAGE_ENTRIES);

	pde_kernel   = (struct page_pde *)   (pages_kernel + NUM_PT_ENTRIES);
	pdpe_kernel  = (struct page_pdpe *)  (pde_kernel + NUM_PAGE_TABLES);
	pml4e_kernel = (struct page_pml4e *) (pdpe_kernel + PAGE_ENTRIES);

	/* Start zero everything */

	for (; i < PAGE_ENTRIES; ++i) /* It's enough with one level 1,2 and 3 */
	{
		pagetable_entries[i].page_address = 0;
		pagetable_entries[i].present      = 0; /* P       */
		pagetable_entries[i].writable     = 0; /* R/W     */
		pagetable_entries[i].user_mode    = 0; /* U/S     */
		pagetable_entries[i].writethrough = 0; /* PWT     */
		pagetable_entries[i].pcd          = 0; /* PCD     */
 		pagetable_entries[i].accessed     = 0; /* A       */
		pagetable_entries[i].dirty        = 0; /* D       */
		pagetable_entries[i].pat          = 0; /* PAT     */
		pagetable_entries[i].global       = 0; /* G       */
		pagetable_entries[i].avail_1      = 0; /* AVL     */
		pagetable_entries[i].avail_2      = 0; /* AVL     */
		pagetable_entries[i].pke          = 0; /* PKE     */
		pagetable_entries[i].nonexecute   = 0; /* NXE     */
	}

	for (i = 0; i < PAGE_ENTRIES; ++i) 
	{
		pde[i].pte_address   = 0;
		pde[i].user_mode     = 0;
		pde[i].present       = 0;
		pde[i].writable      = 0;
		pde[i].writethrough  = 0;
		pde[i].pcd           = 0;
		pde[i].accessed      = 0;
		pde[i].zero          = 0;
		pde[i].ignored_1     = 0;
		pde[i].ignored_2     = 0;
		pde[i].avail_1       = 0;
		pde[i].avail_2       = 0;
		pde[i].nonexecute    = 0;
	}

	for (i = 0; i < PAGE_ENTRIES; ++i)
	{
		pdpe[i].user_mode      = 0;
		pdpe[i].present        = 0;
		pdpe[i].writable       = 0;
		pdpe[i].writethrough   = 0;
		pdpe[i].pcd            = 0;
		pdpe[i].accessed       = 0;
		pdpe[i].zero           = 0;
		pdpe[i].ignored_1      = 0;
		pdpe[i].ignored_2      = 0;
		pdpe[i].avail_1        = 0;
		pdpe[i].avail_2        = 0;
		pdpe[i].nonexecute     = 0;
		pdpe[i].pde_address    = 0;

		pml4e[i].user_mode     = 0;
		pml4e[i].present       = 0;
		pml4e[i].writable      = 0;
		pml4e[i].writethrough  = 0;
		pml4e[i].pcd           = 0;
		pml4e[i].accessed      = 0;
		pml4e[i].ignored_1     = 0;
		pml4e[i].avail_1       = 0;
		pml4e[i].avail_2       = 0;
		pml4e[i].nonexecute    = 0;
		pml4e[i].pdpe_address  = 0;
	}

	/* User code (Might be more than one page) */
	for (i = 0; i < num_pages; ++i)
	{
		pagetable_entries[i].user_mode    = 1;
		pagetable_entries[i].present      = 1; 
		pagetable_entries[i].writable     = 1; 
		pagetable_entries[i].page_address = ((u64)userbuffer / 0x1000) + (PAGE_ENTRIES * i);
	}

	/* Stack code */
	pagetable_entries[PAGE_ENTRIES  - 2].user_mode    = 1;
	pagetable_entries[PAGE_ENTRIES  - 2].present      = 1; 
	pagetable_entries[PAGE_ENTRIES  - 2].writable     = 1; 
	pagetable_entries[PAGE_ENTRIES  - 2].page_address = ((u64)ustack_phys / 0x1000);

	/* Lazy Allocation (Avoid General Protection Fault) */
	pagetable_entries[PAGE_ENTRIES  - 1].user_mode    = 1;
	pagetable_entries[PAGE_ENTRIES  - 1].writable     = 1;

	/* TLS page */
	pagetable_entries[PAGE_ENTRIES  - 3].user_mode    = 1;
	pagetable_entries[PAGE_ENTRIES  - 3].present      = 1; 
	pagetable_entries[PAGE_ENTRIES  - 3].writable     = 1; 
	pagetable_entries[PAGE_ENTRIES  - 3].page_address = ((u64)tls_phys / 0x1000);

	aux_pte = pagetable_entries;
	pde[PAGE_ENTRIES - 1].pte_address = (u64) aux_pte >> 12;
	pde[PAGE_ENTRIES - 1].user_mode   = 1;
	pde[PAGE_ENTRIES - 1].present     = 1;
	pde[PAGE_ENTRIES - 1].writable    = 1;

	aux_pde = pde;
	pdpe[PAGE_ENTRIES - 1].pde_address = (u64) aux_pde >> 12;
	pdpe[PAGE_ENTRIES - 1].user_mode = 1;
	pdpe[PAGE_ENTRIES - 1].present   = 1;
	pdpe[PAGE_ENTRIES - 1].writable  = 1;

	pml4e[PAGE_ENTRIES - 1].pdpe_address = (u64) pdpe >> 12;
	pml4e[PAGE_ENTRIES - 1].user_mode = 1;
	pml4e[PAGE_ENTRIES - 1].present   = 1;
	pml4e[PAGE_ENTRIES - 1].writable  = 1;

	/*  Initialize the first entry to the existing kernel portion */
	pml4e[0] = pml4e_kernel[0];

	/* Update ustack_virtual and ucode_virtual -------------------------------*/

	*ucode_virtual  = (void*) (0xffff000000000000	| 
				  (PAGE_ENTRIES    - 1) << 39		|
				  (PAGE_ENTRIES    - 1) << 30		|
				  (PAGE_ENTRIES    - 1) << 21		|
				  (0) << 12);

	*ustack_virtual = (void*) (0xffff000000000000	| 
				  (PAGE_ENTRIES    - 1) << 39		|
				  (PAGE_ENTRIES    - 1) << 30		|
				  (PAGE_ENTRIES    - 1) << 21		|
				  (PAGE_ENTRIES    - 2) << 12);

	*tls_virtual = (void*) (0xffff000000000000	    | 
				  (PAGE_ENTRIES    - 1) << 39		|
				  (PAGE_ENTRIES    - 1) << 30		|
				  (PAGE_ENTRIES    - 1) << 21		|
				  (PAGE_ENTRIES    - 3) << 12);

	if ((ret = __check_addresses(*ustack_virtual,*ucode_virtual, *tls_virtual)) == -1){
		goto error_upaging;
	}

	/* Make the user stack point to the end of the page (add 4096) */
	*ustack_virtual = *ustack_virtual + 0x1000;

	asm __volatile__ ("mov %0, %%cr3" 
				      : /* no output registers */
				      : "r"(pml4e)
				      : "memory"); 

	return 0;

error_upaging:

	return -1;
}


