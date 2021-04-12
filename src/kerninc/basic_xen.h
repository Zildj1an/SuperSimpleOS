#ifndef __BASIC_XEN_H_
#define __BASIC_XEN_H_

#include <types.h>
#include <cpuid.h>
#include <printf.h>
#include <xen.h>
#include <hypercall.h>

#define _USE_XEN_HYPERVISOR     (0) /* VBox is an alternative */
#define HYP_VENDOR_SIGNATURE    (0x40000000)
#define XEN_NUMS1               (0x566e6558)
#define XEN_NUMS2               (0x65584d4d)
#define XEN_NUMS3               (0x4d4d566e)
#define XEN_MIN_LEAVES          (2)
#define PAGE_SHIFT              (12)
#define NSEC_PER_SEC            (1000000000ULL)

typedef __INT64_TYPE__          bmk_time_t;

shared_info_t *HYPERVISOR_shared_info;

/*
 * pvclock specific.
 */

/* Xen/KVM per-vcpu time ABI. */
struct pvclock_vcpu_time_info {
	uint32_t version;
	uint32_t pad0;
	uint64_t tsc_timestamp;
	uint64_t system_time;
	uint32_t tsc_to_system_mul;
	int8_t tsc_shift;
	uint8_t flags;
	uint8_t pad[2];
} __attribute__((__packed__));

/* Xen/KVM wall clock ABI. */
struct pvclock_wall_clock {
	uint32_t version;
	uint32_t sec;
	uint32_t nsec;
} __attribute__((__packed__));

int detect_xen(void);
uint32_t hypervisor_base(void);
int xen_init_shared(char *_minios_shared_info);
void print_xen_version(void);

void init_clock_stuff(void);
bmk_time_t xen_monotonic_clock(void);
bmk_time_t xen_wall_clock(void);

#endif