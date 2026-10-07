/*
 * HomeOS — sequenza di avvio.
 *
 * Fase 2: probe. Risponde a due domande con lo stesso boot:
 *   1. i registri del display sono raggiungibili in S-Mode?
 *   2. il framebuffer lasciato da U-Boot e' mappato e scrivibile?
 *
 * Ogni accesso potenzialmente pericoloso e' preceduto da una stampa: se il log
 * si ferma, si sa esattamente quale accesso ha faultato. Le letture MMIO su un
 * blocco non alimentato possono bloccare il bus, quindi l'ordine conta.
 */

#include <stdint.h>

#include "boot.h"
#include "console.h"
#include "display.h"
#include "fbcon.h"
#include "services.h"
#include "splash.h"
#include "shell/shell.h"
#include "theme.h"
#include "usb/usb.h"

/* Primo obiettivo: schermo 640x480 bianco pieno, cosi' il framebuffer e'
 * verificabile a occhio senza ambiguita' di colore. Il byte alto e' l'alpha:
 * deve essere 0xFF, altrimenti in ARGB il pixel risulta trasparente. */
/* Risoluzione di avvio: si sceglie a compilazione (make BOOT_W=1920 BOOT_H=1080).
 * Il framebuffer riservato dal Core copre fino a 1920x1080.
 * Nota: i nomi non devono collidere con la guardia di boot.h, che si chiama
 * HOMEOS_BOOT_H — definire quella macro da riga di comando salterebbe l'header. */
#ifndef HOMEOS_BOOT_WIDTH
#define HOMEOS_BOOT_WIDTH 640u
#endif
#ifndef HOMEOS_BOOT_HEIGHT
#define HOMEOS_BOOT_HEIGHT 480u
#endif
#define BOOT_WIDTH  ((uint32_t)HOMEOS_BOOT_WIDTH)
#define BOOT_HEIGHT ((uint32_t)HOMEOS_BOOT_HEIGHT)
#define BOOT_COLOR  0xFFFFFFFFu

void homeos_boot(void) {
    os_puts("[HomeOS] phase 2: display bring-up ");
    os_putu32(BOOT_WIDTH);
    os_puts("x");
    os_putu32(BOOT_HEIGHT);
    os_puts("\n");

    int rc = display_bringup(BOOT_WIDTH, BOOT_HEIGHT, BOOT_COLOR);

    if (rc == DISPLAY_OK) {
        os_puts("[HomeOS] display up: drawing the boot screen\n");
        splash_draw();
        os_puts("[HomeOS] boot screen drawn\n");

        /*
         * 3 secondi, non 1,5: dopo il cambio di timing il monitor impiega
         * anche piu' di un secondo a riagganciare, e quel tempo si mangia
         * l'attesa. Cosi' restano almeno ~2 secondi di logo effettivamente
         * visibili anche con un monitor lento.
         */
        services_delay_ms(3000);
        /* Nel tema classico la console e' gia' pronta con il banner scritto
         * come testo: non va ridisegnata, o il banner sparirebbe subito. */
        if (!theme_is_retro()) fbcon_init();

        /* La tastiera USB si tenta subito; se non risponde si resta sulla
         * seriale, che e' sempre disponibile. */
        os_puts("[HomeOS] USB keyboard: starting\n");
        if (usb_keyboard_start()) {
            os_puts("[HomeOS] USB keyboard active\n");
        } else {
            os_puts("[HomeOS] USB keyboard not available, continuing on serial\n");
        }

        shell_run();   /* non ritorna: il prompt resta attivo */
        return;
    }

    os_puts("[HomeOS] display FAILED rc=");
    os_puti32(rc);
    os_puts("  reg=");
    os_puthex64((uint64_t)display_failed_reg());
    os_puts(" expected=");
    os_puthex64(display_expected());
    os_puts(" read=");
    os_puthex64(display_observed());
    os_puts("\n");

    /* Fail-soft: senza display si resta sulla seriale, ma la shell parte
     * comunque. La console su framebuffer non e' inizializzata, quindi i suoi
     * output vengono semplicemente ignorati. */
    os_puts("[HomeOS] continuing without display, serial only\n");
    shell_run();
}
