#include "../command.h"
#include "../shell.h"
#include "../../console.h"
#include "../../display.h"
#include "../../services.h"
#include "../../version.h"

SHELL_COMMAND(cmd_info, "info", "version, board, display and profile") {
    (void)argc; (void)argv;

    shell_puts("HomeOS " HOMEOS_VERSION " \"" HOMEOS_CODENAME "\"\n");
    shell_puts("  architecture : " HOMEOS_ARCH "  (RISC-V 64-bit)\n");
    shell_puts("  board        : " HOMEOS_BOARD "\n");
    shell_puts("  profile      : ");
    shell_puts(console_is_supervisor() ? "supervisor (S-Mode)\n" : "protect (U-Mode)\n");

    shell_puts("  display      : ");
    shell_putu32(display_width());
    shell_puts("x");
    shell_putu32(display_height());
    shell_puts(" XRGB8888\n");

    shell_puts("  framebuffer  : physical=0x70000000 uncached=0x470000000\n");

    shell_puts("  ticks        : ");
    shell_puthex64(services_ticks());
    shell_puts("  (");
    shell_putu32((uint32_t)(services_ticks() / HOMEOS_TIMER_FREQ_HZ));
    shell_puts(" s since boot)\n");
}
