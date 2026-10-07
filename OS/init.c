/*
 * HomeOS — scheletro di Init
 * ---------------------------------------------------------------------------
 * Questo e' il primo codice di HomeOS eseguito sulla Milk-V Mars: il Core lo
 * carica dal bundle di boot e gli passa il controllo con un
 * `struct Exec64InitContext` in a0.
 *
 * Fa esattamente tre cose, volutamente minime:
 *   1. valida il contesto ricevuto dal Core (magic / ABI / size);
 *   2. stampa un banner diagnostico sulla seriale, nel profilo corretto;
 *   3. entra nel loop di servizio: e' il punto in cui agganciare driver,
 *      input, GPIO e shell.
 *
 * Non e' ancora un OS: e' il gancio pulito su cui costruirlo.
 */

#include <stdint.h>
#include <exec64/init.h>

#include "boot.h"
#include "console.h"
#include "services.h"

#if !defined(ARCH_RISCV64)
#error "HomeOS Init skeleton: target RISC-V64 (Milk-V Mars)"
#endif

static const struct Exec64InitContext *g_ctx;
static int g_supervisor;   /* 1: profilo supervisor (S-Mode, SysBase reale) */

/* ------------------------------------------------------------------ */
/* Banner e hook di avvio                                             */
/* ------------------------------------------------------------------ */

static void print_context_summary(void) {
    os_puts("[HomeOS] Init started\n");

    os_puts("[HomeOS] Init ABI: ");
    os_putu32(g_ctx->ic_AbiMajor);
    os_puts(".");
    os_putu32(g_ctx->ic_AbiMinor);
    os_puts("  struct_size=");
    os_putu32(g_ctx->ic_StructSize);
    os_puts("\n");

    os_puts("[HomeOS] profile: ");
    os_puts(g_supervisor ? "supervisor (S-Mode)\n" : "protect (U-Mode)\n");

    if (g_ctx->ic_Flags & EXEC64_INIT_HAS_BOOT_RESOURCE) {
        os_puts("[HomeOS] boot resource handle: ");
        os_puthex64(g_ctx->ic_BootResource);
        os_puts("\n");
    } else {
        os_puts("[HomeOS] no boot resource handed over\n");
    }

    if (g_supervisor) {
        os_puts("[HomeOS] SysBase: ");
        os_puthex64((uint64_t)(uintptr_t)g_ctx->ic_SysBase);
        os_puts("  services: ");
        os_puthex64((uint64_t)(uintptr_t)g_ctx->ic_Services);
        os_puts("\n");
    }
}

/*
 * Punto di aggancio dell'OS vero.
 * Qui andranno: driver display (HDMI), input, GPIO, servizi e shell.
 */
static void homeos_main(void) {
    homeos_boot();
    os_puts("[HomeOS] entering the service loop\n");

    /*
     * Ci si arriva solo se il boot non ha preso il controllo con la shell.
     * In S-Mode niente ecall: si dorme sull'istruzione wfi.
     */
    for (;;) {
        asm volatile("wfi");
    }
}

/* ------------------------------------------------------------------ */
/* Entry point: il Core salta qui con `a0 = Exec64InitContext*`        */
/* ------------------------------------------------------------------ */

void _start(const struct Exec64InitContext *context) {
    g_ctx = context;
    g_supervisor = 0;

    /*
     * Validazione minima. Contesto assente o incoerente: fermiamo l'hart
     * senza stampare, perche' non possiamo fidarci nemmeno dell'output.
     */
    if (!context ||
        context->ic_Magic != EXEC64_INIT_CONTEXT_MAGIC ||
        context->ic_AbiMajor != EXEC64_INIT_CONTEXT_ABI_MAJOR ||
        context->ic_StructSize < EXEC64_INIT_CONTEXT_V1_0_SIZE) {
        for (;;) { }
    }

    /* Il profilo supervisor consegna SysBase solo con contesto >= 1.2. */
    if (context->ic_AbiMinor >= 2 &&
        context->ic_StructSize >= EXEC64_INIT_CONTEXT_V1_2_SIZE &&
        (context->ic_Flags & EXEC64_INIT_SUPERVISOR) &&
        context->ic_SysBase != 0) {
        g_supervisor = 1;
    }

    console_init(context, g_supervisor);
    services_init(context, g_supervisor);

    print_context_summary();
    homeos_main();

    /* Un ritorno da _start equivale a terminare Init: il Core applica la
     * propria policy di recovery. Non dovrebbe accadere (homeos_main cicla).
     * In supervisor un ecall andrebbe a OpenSBI, quindi si resta qui. */
    for (;;) {
        asm volatile("wfi");
    }
}
