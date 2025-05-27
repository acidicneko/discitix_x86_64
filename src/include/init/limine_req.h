#ifndef LIMINE_REQUESTS_H
#define LIMINE_REQUESTS_H

#include <init/limine.h>

extern volatile struct limine_framebuffer_request framebuffer_request;
extern volatile struct limine_memmap_request memmap_request;
extern volatile struct limine_kernel_file_request kernel_file_request;
extern volatile struct limine_module_request module_request;
extern volatile struct limine_kernel_address_request kernel_addr_request;
extern volatile struct limine_hhdm_request hhdm_request;

#endif
