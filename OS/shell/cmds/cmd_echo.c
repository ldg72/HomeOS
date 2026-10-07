#include "../command.h"
#include "../shell.h"

SHELL_COMMAND(cmd_echo, "echo", "print the arguments") {
    for (int i = 1; i < argc; i++) {
        shell_puts(argv[i]);
        if (i + 1 < argc) shell_putc(' ');
    }
    shell_putc('\n');
}
