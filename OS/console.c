/*
 * HomeOS — console seriale.
 *
 * In profilo supervisor si usa il SysBase consegnato dal Core (DebugPutS);
 * altrimenti si passa da SYS_KPUTS. Nessuna dipendenza da Exec64OS.
 */

#include <stdint.h>
#include <stddef.h>
#include <exec64/init.h>
#include <exec/exec_base.h>

#include "console.h"
#include "mmio.h"
#include "os_syscalls.h"

/* UART0 della JH7110: registri a 4 byte, LSR bit 0 = dato pronto. */
#define UART0_BASE 0x10000000UL
#define UART_RHR   0x00UL
#define UART_LSR   0x14UL
#define UART_LSR_DATA_READY 0x01U

static const struct Exec64InitContext *g_ctx;
static int g_supervisor;

void console_init(const struct Exec64InitContext *ctx, int supervisor) {
    g_ctx = ctx;
    g_supervisor = supervisor;
}

int console_is_supervisor(void) {
    return g_supervisor;
}

void os_puts(const char *s) {
    if (g_supervisor && g_ctx && g_ctx->ic_SysBase &&
        g_ctx->ic_SysBase->ex_DebugPutS) {
        g_ctx->ic_SysBase->ex_DebugPutS(s);
    } else {
        sys_kputs(s);
    }
}

void os_puthex64(uint64_t value) {
    static const char digits[] = "0123456789ABCDEF";
    char buffer[19];
    buffer[0] = '0';
    buffer[1] = 'x';
    for (int i = 0; i < 16; i++) {
        buffer[2 + i] = digits[(value >> ((15 - i) * 4)) & 0xF];
    }
    buffer[18] = '\0';
    os_puts(buffer);
}

void os_putu32(uint32_t value) {
    char buffer[11];
    int pos = 10;
    buffer[pos] = '\0';
    if (value == 0) {
        buffer[--pos] = '0';
    } else {
        while (value > 0 && pos > 0) {
            buffer[--pos] = (char)('0' + (value % 10u));
            value /= 10u;
        }
    }
    os_puts(&buffer[pos]);
}

void os_puti32(int32_t value) {
    if (value < 0) {
        os_puts("-");
        os_putu32((uint32_t)(-value));
    } else {
        os_putu32((uint32_t)value);
    }
}

/*
 * Input seriale.
 *
 * In supervisor si usa ex_DebugGetC del SysBase: chiamata diretta, bloccante.
 * Il ramo a syscall resta solo per il profilo U-Mode, dove l'ecall arriva
 * davvero al Core. Da S-Mode un ecall andrebbe a OpenSBI: e' l'errore
 * "Invalid error ... for ext=0x2" che si vedeva digitando.
 */
char console_getchar(void) {
    if (g_supervisor && g_ctx && g_ctx->ic_SysBase &&
        g_ctx->ic_SysBase->ex_DebugGetC) {
        return g_ctx->ic_SysBase->ex_DebugGetC();
    }
    return (char)sys_kgetc();
}

char console_trygetchar(void) {
    if ((mmio_read32(UART0_BASE + UART_LSR) & UART_LSR_DATA_READY) == 0) return 0;
    return (char)(mmio_read32(UART0_BASE + UART_RHR) & 0xffU);
}
