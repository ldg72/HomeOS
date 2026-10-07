#include "../command.h"
#include "../shell.h"
#include "../../fbcon.h"
#include "../../theme.h"

SHELL_COMMAND(cmd_theme, "theme", "visual theme: 'theme' lists, 'theme N' applies") {
    if (argc < 2) {
        shell_puts("Available themes:\n");
        for (int i = 0; i < theme_count(); i++) {
            const struct theme_colors *t = theme_at(i);
            shell_puts("  ");
            shell_putu32((uint32_t)(i + 1));
            shell_puts("  ");
            shell_puts(t->name);
            if (t == theme_current()) shell_puts("   <- active");
            shell_puts("\n");
        }
        shell_puts("usage: theme <number>\n");
        return;
    }

    uint32_t index = 0;
    for (int k = 0; argv[1][k] >= '0' && argv[1][k] <= '9'; k++) {
        index = index * 10u + (uint32_t)(argv[1][k] - '0');
    }
    if (index < 1u || !theme_set((int)(index - 1u))) {
        shell_puts("theme out of range (use 'theme' to list)\n");
        return;
    }

    /* Il tema cambia colori e geometria della console: si ridisegna. */
    fbcon_init();
    shell_puts("theme applied: ");
    shell_puts(theme_current()->name);
    shell_puts("\n");
}
