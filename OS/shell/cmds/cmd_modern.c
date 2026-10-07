#include "../command.h"
#include "../shell.h"
#include "../../display.h"
#include "../../fbcon.h"
#include "../../theme.h"

SHELL_COMMAND(cmd_modern, "modern", "modern mode: 720p and modern theme") {
    (void)argc; (void)argv;

    if (display_set_mode(1280, 720) != DISPLAY_OK) {
        shell_puts("resolution switch failed\n");
        return;
    }
    theme_set(0);
    fbcon_init();
}
