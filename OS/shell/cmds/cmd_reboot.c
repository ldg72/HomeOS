#include "../command.h"
#include "../shell.h"
#include "../../fbcon.h"
#include "../../services.h"
#include "../../theme.h"

SHELL_COMMAND(cmd_reboot, "reboot", "restart the session; 'reboot sbi' asks OpenSBI") {
    /*
     * Tentativo di reset completo, solo su richiesta esplicita.
     *
     * Sulla Milk-V Mars questo percorso NON e' affidabile: la chiamata SBI
     * SRST fa cadere l'uscita video e la scheda non riparte, restando ferma
     * finche' non si toglie l'alimentazione. Il tentativo resta disponibile
     * perche' e' una prova utile, ma non e' il comportamento di default.
     */
    if (argc >= 2 && (argv[1][0] == 's' || argv[1][0] == 'S')) {
        int available = services_sbi_probe(SBI_EXT_SRST);
        shell_puts("OpenSBI SRST extension: ");
        shell_puts(available ? "advertised\n" : "not advertised\n");
        if (available) {
            shell_puts("WARNING: on this board the full reset may leave the machine halted.\n");
            shell_puts("requesting reset...\n");
            int rc = services_system_reboot();
            shell_puts("SBI reset returned: ");
            if (rc < 0) {
                shell_putc('-');
                shell_putu32((uint32_t)(-rc));
            } else {
                shell_putu32((uint32_t)rc);
            }
            shell_puts("\n");
        }
    }

    /* Riavvio della sessione: sempre sicuro, non tocca l'hardware. */
    shell_puts("restarting the session\n");
    fbcon_init();
    if (theme_is_retro()) fbcon_classic_banner();
}
