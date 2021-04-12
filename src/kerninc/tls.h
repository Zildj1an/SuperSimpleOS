#ifndef __THREAD_LOCAL_STORAGE_H__
#define __THREAD_LOCAL_STORAGE_H__

#define MSR_FS_BASE	0xc0000100 /* 64bit FS base (x86-64 specific MSRs) */

struct tls_block
{
	struct tls_block *myself;
	char padding[4096 - 8];
};

#endif