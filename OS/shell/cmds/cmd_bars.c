#include "../command.h"
#include "../shell.h"
#include "../../display.h"
#include "../../fbcon.h"
#include "../../services.h"

SHELL_COMMAND(cmd_bars, "bars", "draw the test pattern (bars, gradient)") {
    (void)argc; (void)argv;
    display_test_pattern();
    services_delay_ms(1500);
    fbcon_redo();
    shell_puts("test pattern drawn\n");
}
