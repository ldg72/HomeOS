#include "../command.h"
#include "../shell.h"
#include "../../usb/usb.h"

SHELL_COMMAND(cmd_usb, "usb", "USB input: status | init") {
    if (argc >= 2 && argv[1][0] == 'i') {
        shell_puts("starting USB input...\n");
        int rc = usb_input_start();
        shell_puts(rc ? "USB input ready\n" : "startup not completed\n");
        return;
    }

    shell_puts("USB input: ");
    switch (usb_input_kind()) {
        case USB_INPUT_KEYBOARD: shell_puts("keyboard, ready"); break;
        case USB_INPUT_MOUSE:    shell_puts("mouse, ready");    break;
        default:                 shell_puts("not ready");       break;
    }
    shell_puts("\n");

    uint32_t reports = 0, chars = 0, failed = 0;
    usb_input_stats(&reports, &chars, &failed);
    shell_puts("  reports received : ");
    shell_putu32(reports);
    shell_puts("\n  keys emitted     : ");
    shell_putu32(chars);
    shell_puts("\n  transfers failed : ");
    shell_putu32(failed);
    shell_puts("\n");

    /* Con 'usb reports' si vedono gli ultimi report: e' cosi' che si capisce
     * se un tasto non e' mai arrivato o se e' stato scartato in decodifica. */
    if (argc >= 2) {
        shell_puts("last reports (modifiers, reserved, 6 keycodes):\n");
        for (uint32_t i = 0; i < USB_KBD_HISTORY; i++) {
            const uint8_t *report = usb_input_history_at(i);
            if (!report) break;
            shell_puts("  ");
            for (int k = 0; k < 8; k++) {
                shell_puthex64(report[k]);
                shell_puts(" ");
            }
            shell_puts("\n");
        }
    }
}
