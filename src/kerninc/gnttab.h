#ifndef __MINIOS_GNTTAB_H__
#define __MINIOS_GNTTAB_H__

#include <hypercall.h>
#include <grant_table.h>

/* Xen Grant tables (check at runtime) */
#define I_SHARE_DATA     (0)
#define OTHER_SIDE_DOMID (8) 
#define GRANT_REFERENCE  (511)
#define MAX_GNT_MSG      (299)

typedef struct 
{
	void *page_1;
	void *page_2;

} grant_pages_t;

extern grant_entry_v1_t *gnttab_table;

void init_gnttab(void);
grant_ref_t gnttab_alloc_and_grant(void **map);
grant_ref_t gnttab_grant_access(domid_t domid, unsigned long frame,int readonly);
grant_ref_t gnttab_grant_transfer(domid_t domid, unsigned long pfn);
unsigned long gnttab_end_transfer(grant_ref_t gref);
int gnttab_end_access(grant_ref_t ref);
void fini_gnttab(void);

#endif /* !__MINIOS_GNTTAB_H__ */
