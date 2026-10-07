#ifndef HOMEOS_SERVICES_H
#define HOMEOS_SERVICES_H

/*
 * Servizi di base del Core, lato supervisor.
 *
 * In S-Mode non si passa dalla tabella syscall: gli ecall andrebbero a OpenSBI
 * (M-Mode) invece che al Core. Si usano quindi le chiamate dirette di SysBase
 * e le istruzioni della CPU.
 */

#include <stdint.h>

struct Exec64InitContext;
struct ExecBase;

/* Frequenza del timer di sistema configurata dal Core sulla Milk-V Mars. */
#define HOMEOS_TIMER_FREQ_HZ 4000000UL

void services_init(const struct Exec64InitContext *ctx, int supervisor);

/*
 * SysBase del Core, oppure NULL se giriamo in U-Mode.
 * Serve ai comandi diagnostici che leggono strutture del Core (es. il
 * puntatore al device tree in ex_PlatformData).
 */
struct ExecBase *services_sysbase(void);

uint64_t services_ticks(void);
void services_delay_us(uint32_t us);
void services_delay_ms(uint32_t ms);

/* Memoria disponibile in byte. */
uint32_t services_avail_mem(void);

/* Alloca memoria per DMA: azzerata, allineata a pagina e dichiarata condivisa.
 * Il puntatore e' utilizzabile direttamente come indirizzo fisico (la RAM del
 * kernel e' mappata in identita'), quindi va passato cosi' com'e' al device. */
void *services_alloc_dma(uint32_t size);

/*
 * Chiede a OpenSBI un reset a freddo della macchina (estensione SBI SRST).
 * Dalla modalita' supervisor un ecall non va al Core ma a M-Mode: e' proprio
 * la via corretta per questa richiesta.
 * Ritorna 0 se il reset e' stato accettato, negativo se non e' disponibile —
 * nel qual caso la macchina e' ancora viva e il chiamante deve ripiegare.
 */
int services_system_reboot(void);

/* Interroga OpenSBI sull'esistenza di un'estensione (SBI BASE, probe). */
int services_sbi_probe(uint32_t extension);

#define SBI_EXT_SRST 0x53525354UL

#endif
