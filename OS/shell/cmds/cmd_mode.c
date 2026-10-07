#include "../command.h"
#include "../shell.h"
#include "../../display.h"
#include "../../fbcon.h"
#include "../../services.h"

static int u32_to_text(uint32_t value, char *out) {
    char tmp[11];
    int n = 0;
    if (value == 0) tmp[n++] = '0';
    while (value > 0) {
        tmp[n++] = (char)('0' + (value % 10u));
        value /= 10u;
    }
    for (int i = 0; i < n; i++) out[i] = tmp[n - 1 - i];
    out[n] = '\0';
    return n;
}

SHELL_COMMAND(cmd_mode, "mode", "resolution: 'mode' lists, 'mode N' switches") {
    if (argc == 1) {
        shell_puts("Available modes:\n");
        for (int i = 0; i < display_mode_count(); i++) {
            uint32_t w = 0, h = 0;
            display_mode_at(i, &w, &h);

            char resolution[16];
            int n = u32_to_text(w, resolution);
            resolution[n++] = 'x';
            n += u32_to_text(h, resolution + n);
            resolution[n] = '\0';

            shell_puts("  ");
            shell_putu32((uint32_t)(i + 1));
            shell_puts("  ");
            shell_puts(resolution);
            for (int pad = 10 - n; pad > 0; pad--) {
                shell_putc(' ');
            }
            shell_puts(display_mode_name(i));
            if (w == display_width() && h == display_height()) {
                shell_puts("   <- active");
            }
            shell_puts("\n");
        }
        shell_puts("usage: mode <number>   or   mode <width> <height>\n");
        return;
    }

    uint32_t w = 0, h = 0;

    if (argc == 2) {
        /* Un solo argomento: e' l'indice della modalita'. */
        uint32_t index = 0;
        for (int k = 0; argv[1][k] >= '0' && argv[1][k] <= '9'; k++) {
            index = index * 10u + (uint32_t)(argv[1][k] - '0');
        }
        if (index < 1 || index > (uint32_t)display_mode_count()) {
            shell_puts("number out of range (use 'mode' to list)\n");
            return;
        }
        display_mode_at((int)(index - 1), &w, &h);
    } else {
        for (int i = 1; i < argc && argv[i][0]; i++) {
            uint32_t value = 0;
            for (int k = 0; argv[i][k] >= '0' && argv[i][k] <= '9'; k++) {
                value = value * 10u + (uint32_t)(argv[i][k] - '0');
            }
            if (i == 1) w = value; else h = value;
        }
    }

    if (w == 0 || h == 0) {
        shell_puts("usage: mode <number>   or   mode <width> <height>\n");
        return;
    }
    if (w == display_width() && h == display_height()) {
        shell_puts("mode already active\n");
        return;
    }

    shell_puts("switching resolution: the monitor will resync\n");
    int rc = display_set_mode(w, h);
    if (rc != DISPLAY_OK) {
        shell_puts("switch failed (rc=");
        shell_putu32((uint32_t)(-rc));
        shell_puts("); previous mode kept\n");
        return;
    }

    /* Il contenuto del framebuffer e' da rifare: la console si ridisegna. */
    fbcon_init();
    shell_puts("new mode: ");
    shell_putu32(display_width());
    shell_puts("x");
    shell_putu32(display_height());
    shell_puts("\n");
}
