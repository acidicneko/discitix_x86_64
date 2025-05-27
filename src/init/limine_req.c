#include <init/limine.h>
#include <init/limine_req.h>

__attribute__((
    used, section(".limine_requests"))) static volatile LIMINE_BASE_REVISION(3);

__attribute__((
    used,
    section(".limine_requests"))) volatile struct limine_framebuffer_request
    framebuffer_request = {.id = LIMINE_FRAMEBUFFER_REQUEST, .revision = 0};

__attribute__((
    used, section(".limine_requests"))) volatile LIMINE_REQUESTS_START_MARKER;

__attribute__((used,
               section(".limine_reqs"))) volatile struct limine_memmap_request
    memmap_request = {.id = LIMINE_MEMMAP_REQUEST, .revision = 0};

__attribute__((
    used, section(".limine_reqs"))) volatile struct limine_kernel_file_request
    kernel_file_request = {.id = LIMINE_KERNEL_FILE_REQUEST, .revision = 0};

__attribute__((used,
               section(".limine_reqs"))) volatile struct limine_module_request
    module_request = {.id = LIMINE_MODULE_REQUEST, .revision = 0};

__attribute__((used,
               section(".limine_reqs"))) volatile struct limine_hhdm_request
    hhdm_request = {.id = LIMINE_HHDM_REQUEST, .revision = 0};

// Request kernel address info
__attribute__((
    used,
    section(".limine_reqs"))) volatile struct limine_kernel_address_request
    kernel_addr_request = {.id = LIMINE_KERNEL_ADDRESS_REQUEST, .revision = 0};
