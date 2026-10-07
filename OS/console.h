#ifndef HOMEOS_CONSOLE_H
#define HOMEOS_CONSOLE_H

#include <stdint.h>

struct Exec64InitContext;

/* Va chiamata per prima in _start: senza contesto l'output usa SYS_KPUTS. */
void console_init(const struct Exec64InitContext *ctx, int supervisor);
int  console_is_supervisor(void);

void os_puts(const char *s);
void os_puthex64(uint64_t value);
void os_putu32(uint32_t value);
void os_puti32(int32_t value);

/* Legge un carattere dalla console seriale. Bloccante. */
char console_getchar(void);

/* Legge un carattere se disponibile, senza attendere: 0 se non c'e' nulla.
 * Legge direttamente la UART (NS16550A a 0x10000000), cosi' il loop della
 * shell puo' interrogare anche la tastiera USB. */
char console_trygetchar(void);

#endif
