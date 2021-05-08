#include "drivers/pit.h"
#include "arch/x86_64/irq.h"
#include "libk/utils.h"

volatile uint64_t pit_ticks = 0;

void pit_handler(register_t* regs){
    pit_ticks++;
    log(INFO, "In pit_handler()\npit_ticks: %ul\n", pit_ticks);
    send_eoi(regs->int_no);
}

void pit_install(){
    irq_install_handler(0, pit_handler);
    log(INFO, "PIT initialised\n");
}

void pit_wait(int ticks){
    uint64_t eticks;
    eticks = pit_ticks + ticks;
    while(pit_ticks < eticks){
        asm volatile("sti;hlt;cli");
    }
}

uint64_t get_ticks(){
    return pit_ticks;
}