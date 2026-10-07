#include "../command.h"
#include "../shell.h"
#include "../../services.h"

SHELL_COMMAND(cmd_mem, "mem", "show available memory") {
    (void)argc; (void)argv;
    uint32_t free_bytes = services_avail_mem();
    shell_puts("free memory: ");
    shell_putu32((uint32_t)(free_bytes / 1024u));
    shell_puts(" KiB\n");
}
