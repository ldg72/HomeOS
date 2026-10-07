#include "../command.h"
#include "../shell.h"
#include "../../fbcon.h"

SHELL_COMMAND(cmd_clear, "clear", "clear the screen") {
    (void)argc; (void)argv;
    fbcon_clear();
}
