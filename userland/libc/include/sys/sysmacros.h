#ifndef _SYS_SYSMACROS_H
#define _SYS_SYSMACROS_H

/* Discitix: Linux-style device number encoding (major in bits 8-19 and
   32-63, minor in bits 0-7 and 20-31). */
#define major(dev)	((unsigned int)((((dev) >> 32) & 0xfffff000) | (((dev) >> 8) & 0xfff)))
#define minor(dev)	((unsigned int)((((dev) >> 12) & 0xffffff00) | ((dev) & 0xff)))
#define makedev(ma, mi) \
	((((unsigned long long)(ma) & 0xfffff000ULL) << 32) | \
	 (((unsigned long long)(ma) & 0x00000fffULL) << 8)  | \
	 (((unsigned long long)(mi) & 0xffffff00ULL) << 12) | \
	 (((unsigned long long)(mi) & 0x000000ffULL)))

#endif
