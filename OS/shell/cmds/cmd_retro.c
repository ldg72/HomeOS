#include "../command.h"
#include "../shell.h"
#include "../../display.h"
#include "../../fbcon.h"
#include "../../services.h"
#include "../../splash.h"
#include "../../theme.h"

SHELL_COMMAND(cmd_retro, "retro", "classic mode: 640x480 and retro theme") {
    (void)argc; (void)argv;
    shell_puts("switching to classic mode...\n");

    if (display_set_mode(640, 480) != DISPLAY_OK) {
        shell_puts("resolution switch failed\n");
        return;
    }
    theme_set(1);

    splash_draw();
    services_delay_ms(2500);
}
