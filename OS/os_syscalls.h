#ifndef HOMEOS_OS_SYSCALLS_H
#define HOMEOS_OS_SYSCALLS_H

/*
 * Wrapper minimi verso le syscall del Core.
 * Numeri in exec/syscalls.h; convenzione RISC-V: numero in a7, argomenti in a0..
 */

#include <stdint.h>
#include <exec/syscalls.h>
#include <exec/arch_syscall.h>

static inline void sys_kputs(const char *s) {
    register uintptr_t a7 asm(EXEC_SC_NUMBER) = SYS_KPUTS;
    register uintptr_t a0 asm(EXEC_SC_ARG0) = (uintptr_t)s;
    asm volatile(EXEC_SC_TRAP :: "r"(a0), "r"(a7) : "memory");
}

static inline void sys_yield(void) {
    register uintptr_t a7 asm(EXEC_SC_NUMBER) = SYS_YIELD;
    asm volatile(EXEC_SC_TRAP :: "r"(a7) : "memory");
}

static inline void sys_exit(int32_t code) {
    register uintptr_t a7 asm(EXEC_SC_NUMBER) = SYS_EXIT;
    register uintptr_t a0 asm(EXEC_SC_ARG0) = (uintptr_t)code;
    asm volatile(EXEC_SC_TRAP :: "r"(a0), "r"(a7) : "memory");
    for (;;) { }
}

static inline void sys_delay_ms(uint32_t ms) {
    register uintptr_t a7 asm(EXEC_SC_NUMBER) = SYS_DELAY_MS;
    register uintptr_t a0 asm(EXEC_SC_ARG0) = (uintptr_t)ms;
    asm volatile(EXEC_SC_TRAP :: "r"(a0), "r"(a7) : "memory");
}

/* Bloccante: attende un carattere dalla console seriale. */
static inline int32_t sys_kgetc(void) {
    register uintptr_t a7 asm(EXEC_SC_NUMBER) = SYS_KGETC;
    register uintptr_t a0 asm(EXEC_SC_ARG0);
    asm volatile(EXEC_SC_TRAP : "=r"(a0) : "r"(a7) : "memory");
    return (int32_t)a0;
}

static inline uint64_t sys_avail_mem(uint32_t attributes) {
    register uintptr_t a7 asm(EXEC_SC_NUMBER) = SYS_AVAIL_MEM;
    register uintptr_t a0 asm(EXEC_SC_ARG0) = (uintptr_t)attributes;
    asm volatile(EXEC_SC_TRAP : "+r"(a0) : "r"(a7) : "memory");
    return (uint64_t)a0;
}

static inline uint64_t sys_get_tick(void) {
    register uintptr_t a7 asm(EXEC_SC_NUMBER) = SYS_GET_TICK;
    register uintptr_t a0 asm(EXEC_SC_ARG0);
    asm volatile(EXEC_SC_TRAP : "=r"(a0) : "r"(a7) : "memory");
    return (uint64_t)a0;
}

#endif
