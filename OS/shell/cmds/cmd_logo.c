#include "../command.h"
#include "../shell.h"
#include "../../splash.h"
#include "../../fbcon.h"
#include "../../services.h"

SHELL_COMMAND(cmd_logo, "logo", "redraw the boot screen") {
    (void)argc; (void)argv;
    splash_draw();
    services_delay_ms(1500);
    fbcon_redo();
    shell_puts("logo redrawn\n");
}
