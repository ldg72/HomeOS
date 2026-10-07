#ifndef HOMEOS_SHELL_H
#define HOMEOS_SHELL_H

/*
 * Shell di HomeOS.
 *
 * Ora legge e scrive sulla seriale. I comandi non chiamano mai os_puts()
 * direttamente: usano shell_put*(), cosi' quando l'output passera' anche al
 * framebuffer cambia solo questo livello.
 */

#include <stdint.h>
#include "command.h"

void shell_run(void);

void shell_putc(char c);
void shell_puts(const char *text);
void shell_putu32(uint32_t value);
void shell_puthex64(uint64_t value);

/* Elenco dei comandi registrati: lo usa 'help'. */
int shell_command_count(void);
const struct shell_command *shell_command_at(int index);

#endif
