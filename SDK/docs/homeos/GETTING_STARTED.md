# HomeOS — Getting Started

Obiettivo: un OS minimale che il Core avvia come `Init`, con shell su HDMI e
accesso GPIO su Milk-V Mars.

## 1. Divisione delle responsabilita

| Componente | Responsabilita |
|---|---|
| `Exec64Core` | boot, validazione `BootInfo`/bundle, memoria, MMU, task, eccezioni, syscall |
| HomeOS (`Init`) | servizi OS, driver (display, GPIO), shell, policy di riavvio |

Il Core **non** conosce nomi di file, filesystem o policy della shell. HomeOS
riceve il controllo come primo task `Init` e da li decide tutto.

## 2. Entry point

Il payload `Init` e un ELF64 RISC-V che espone `_start`. Al primo ingresso:

- RISC-V: `a0` = puntatore a `struct Exec64InitContext`
- il contesto resta valido per tutta la vita del task Init e va trattato in
  sola lettura
- un ritorno da `_start` equivale a terminare Init (il Core applica la recovery)

```c
#include <exec64/init.h>

void main(struct ExecBase *sysbase, const struct Exec64InitContext *ctx);

void _start(const struct Exec64InitContext *ctx) {
    /* verifica magic e ABI */
    if (ctx->ic_Magic != EXEC64_INIT_CONTEXT_MAGIC) {
        for (;;) { }              /* halt: contesto non valido */
    }
    main(ctx->ic_SysBase, ctx);   /* solo profilo supervisor */
}
```

## 3. Profili e contesto

`Exec64InitContext` e versionato in modo append-only (`exec64/init.h`):

| Versione | Size | Contenuto aggiunto |
|---|---:|---|
| v1.0 | 64 B | magic, ABI, size, flag |
| v1.1 | 80 B | `ic_BootResource` (handle opaco) |
| v1.2 | 96 B | `ic_SysBase`, `ic_Services` (solo supervisor) |

- **Protect**: Init gira in U-Mode, riceve v1.1; usa le syscall.
- **Performance/supervisor**: Init gira in S-Mode flat, riceve v1.2 con
  `SysBase` reale e `Exec64InitServices` (ResourceInfo/Read/Write,
  LoadImage/UnloadImage).

Per un OS che implementa driver hardware, il profilo **supervisor** e il piu
diretto: hai `SysBase` reale e accesso all'hardware senza proxy syscall.
Verifica sempre `ic_AbiMajor`/`ic_AbiMinor` e `ic_StructSize` prima di usare i
campi in coda.

## 4. Syscall

La tabella canonica e in `exec/syscalls.h`; i binding inline in
`exec/arch_syscall.h`. Su RISC-V la convenzione e `ecall` con il numero in `a7`
e gli argomenti in `a0..a4`. Su un profilo supervisor molte operazioni sono
disponibili anche come chiamate dirette a `IExec` (`exec/exec_lib.h`).

## 5. Memoria e task

- Allocazione: `IExec->AllocMem(size, flags)` con i flag di `exec/memory.h`
  (`MEMF_PUBLIC`, `MEMF_CLEAR`, `MEMF_SHARED`, `MEMF_ALIGNED`, ...).
- Liste: `NewList`, `AddTail`, `RemHead`, ... come da `exec/exec_lib.h`
  (semantica AmigaOS-style, lista doppiamente linkata).
- Task e supervisione: handle opachi ed eventi in `exec64/task.h`.

## 6. Cosa serve al tuo OS

1. **Init**: `_start` + `main`, che inizializza i driver e avvia la shell.
2. **Driver display**: per l'HDMI (vedi `DISPLAY_AND_GPIO.md`).
3. **Driver GPIO**: per la domotica (vedi `DISPLAY_AND_GPIO.md`).
4. **Shell**: ciclo di lettura da tastiera/console e rendering sul framebuffer.
5. **Bundle**: impacchetta il tuo ELF in `HomeOS.img`
   (vedi `BOOT_IMAGE_FORMAT.md`).

## 7. Vincoli da rispettare

- W^X: nessuna pagina scrivibile ed eseguibile insieme.
- Il Core valida ogni range ricevuto dal firmware: non fidarti di indirizzi
  "di comodo".
- Non esporre memoria del Core a task non privilegiati.
- ABI append-only: le struct del Core crescono in coda, con size esplicito.

## 8. Build e boot

Compila il tuo Init con la toolchain del punto `SDK/README.md`, produci il bundle
`HomeOS.img` come descritto in `BOOT_IMAGE_FORMAT.md`, copialo sulla partizione
di boot insieme a `Exec64Core` e alle configurazioni in `CORE/boot-config/`, e
avvia la scheda. La console seriale (115200 8N1) e la tua unica finestra di
debug durante il bring-up. La catena completa (e il fatto che il nome del file
è libero) è in `BOOT_CHAIN.md`.
