#ifndef HOMEOS_VERSION_H
#define HOMEOS_VERSION_H

#define HOMEOS_VERSION  "0.1"
#define HOMEOS_CODENAME "Mizar"
#define HOMEOS_ARCH     "riscv64"
#define HOMEOS_BOARD    "Milk-V Mars / StarFive JH7110"

/*
 * Core su cui giriamo.
 *
 * ATTENZIONE: sono valori MANTENUTI A MANO. Il Core stampa la propria versione
 * sulla seriale ("EXEC64 OS (v0.125 - 2026-08-24)") ma non la espone a Init:
 * non esiste un campo in ExecBase ne' nel contesto di avvio. Se il Core viene
 * aggiornato, questi due valori vanno aggiornati a mano — e vanno tenuti
 * coerenti con l'hash in CORE/SHA256SUMS.
 */
#define HOMEOS_CORE_NAME    "Exec64Core"
#define HOMEOS_CORE_VERSION "0.125"

#endif
