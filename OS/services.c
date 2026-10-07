#include <stddef.h>
#include <exec64/init.h>
#include <exec/exec_base.h>
#include <exec/memory.h>

#include "services.h"
#include "os_syscalls.h"

static struct ExecBase *g_sysbase;

void services_init(const struct Exec64InitContext *ctx, int supervisor) {
    g_sysbase = (supervisor && ctx) ? ctx->ic_SysBase : NULL;
}

struct ExecBase *services_sysbase(void) {
    return g_sysbase;
}

uint64_t services_ticks(void) {
    uint64_t value;
    asm volatile("rdtime %0" : "=r"(value));
    return value;
}

void services_delay_us(uint32_t us) {
    uint64_t start = services_ticks();
    uint64_t delta = ((uint64_t)HOMEOS_TIMER_FREQ_HZ * us) / 1000000u;
    while (services_ticks() - start < delta) { }
}

void services_delay_ms(uint32_t ms) {
    while (ms-- > 0) services_delay_us(1000u);
}

uint32_t services_avail_mem(void) {
    if (g_sysbase && g_sysbase->ex_IExec && g_sysbase->ex_IExec->AvailMem) {
        return g_sysbase->ex_IExec->AvailMem(0);
    }
    return (uint32_t)sys_avail_mem(0);
}

void *services_alloc_dma(uint32_t size) {
    if (!g_sysbase || !g_sysbase->ex_IExec || !g_sysbase->ex_IExec->AllocMem) {
        return NULL;
    }
    void *memory = g_sysbase->ex_IExec->AllocMem(
        size, MEMF_CLEAR | MEMF_SHARED | MEMF_ALIGNED);
    if (!memory) return NULL;
    /* L'xHCI indirizza solo i primi 4 GiB. */
    uint64_t address = (uint64_t)(uintptr_t)memory;
    if (address + size > 0x100000000ULL) return NULL;
    return memory;
}

int services_system_reboot(void) {
    /* SBI System Reset (SRST): ext 'SRST', func 0 = system reset,
     * a0 = tipo (1 = riavvio a freddo), a1 = motivo (0 = nessuno). */
    register uintptr_t a7 asm("a7") = 0x53525354UL;
    register uintptr_t a6 asm("a6") = 0;
    register uintptr_t a0 asm("a0") = 1;
    register uintptr_t a1 asm("a1") = 0;
    asm volatile("ecall" : "+r"(a0) : "r"(a1), "r"(a6), "r"(a7) : "memory");
    /* Se l'ecall e' tornato, il reset non e' avvenuto: a0 contiene l'errore. */
    return (int)(intptr_t)a0;
}

int services_sbi_probe(uint32_t extension) {
    /* SBI BASE (0x10), funzione 3 = probe_extension: a0 = estensione da
     * interrogare; ritorna 0 se l'estensione e' disponibile. */
    register uintptr_t a7 asm("a7") = 0x10UL;
    register uintptr_t a6 asm("a6") = 3;
    register uintptr_t a0 asm("a0") = extension;
    asm volatile("ecall" : "+r"(a0) : "r"(a6), "r"(a7) : "memory");
    return (int)(intptr_t)a0 == 0;
}
