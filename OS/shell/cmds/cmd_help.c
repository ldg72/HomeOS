#include "../command.h"
#include "../shell.h"

SHELL_COMMAND(cmd_help, "help", "list the available commands") {
    (void)argc; (void)argv;
    shell_puts("Available commands:\n");
    for (int i = 0; i < shell_command_count(); i++) {
        const struct shell_command *c = shell_command_at(i);
        shell_puts("  ");
        shell_puts(c->name);
        int len = 0;
        while (c->name[len]) len++;
        for (int i = len; i < 12; i++) shell_putc(' ');
        shell_puts(c->help);
        shell_puts("\n");
    }
}
