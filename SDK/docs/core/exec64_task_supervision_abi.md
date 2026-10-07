# Exec64 Task Supervision ABI

**Stato:** prima slice implementata e validata su QEMU
**Data:** 2026-08-29
**Origine:** `ROADMAP_DUALMODE`, discussione e approvazione esplicita del
maintainer

## 1. Scopo

Questa ABI permette a un task U-Mode di avviare un task U-Mode gia preparato,
ricevere una notifica alla sua terminazione e consumare un evento bounded.

Il meccanismo appartiene al Core perche scheduling, teardown, fault containment
e consegna autorevole del motivo di uscita sono responsabilita del kernel. Non
contiene invece policy relative a shell, DOS, filesystem o desktop.

## 2. Contratto pubblico

L'header canonico e `core/include/uapi/exec64/task.h`.

`AddSupervisedTask(task, watch)`:

- accetta soltanto un task U-Mode gia preparato con `PrepareStack()`
- alloca nel Core un segnale appartenente al supervisore
- registra la relazione prima di rendere il figlio runnable
- restituisce in `watch` un handle opaco e la signal mask

`Wait(watch.etw_SignalMask)` sospende il supervisore fino alla notifica.

`TakeTaskExit(watch.etw_Handle, info)`:

- autorizza il consumo soltanto al supervisore che possiede l'handle
- restituisce `PENDING`, `READY` oppure `ERROR`
- copia motivo e dettaglio in una struttura versionata
- invalida la subscription e libera il segnale dopo il consumo

I motivi v1 sono `RETURN`, `SYS_EXIT`, `EXCEPTION` e `REMOVED`.

## 3. ABI RISC-V

L'estensione e append-only:

- `SYS_ADD_SUPERVISED_TASK = 54`
- `SYS_TAKE_TASK_EXIT = 55`
- `SYS_WAIT_SIGNAL = 56`

I nuovi metodi sono aggiunti in coda a `struct ExecInterface`. Il test
`sdk/tests/task_supervision_abi.c` congela dimensioni, offset e ordine.

## 4. Confini di sicurezza

- nessun puntatore kernel viene restituito come handle
- task, watch ed exit info devono essere puntatori U-Mode validi
- il segnale atteso deve appartenere al task chiamante
- un supervisore non puo consumare eventi posseduti da un altro task
- il Core consegna eventi, ma non decide restart, backoff o policy OS

## 5. Uso previsto

La prima applicazione reale sara Init, che potra separare la shell dal proprio
address space e applicare la policy di restart in U-Mode. Lo stesso contratto
permettera poi di avviare il server DOS U-Mode necessario alla navigazione
desktop nelle sottodirectory.

Il comando `taskwatchtest` valida il percorso minimo senza dipendenze OS:
prepara un figlio U-Mode, lo avvia sotto supervisione, attende il segnale e
verifica un evento `RETURN` con dettaglio zero.

Il 2026-08-29 il test ha completato tre cicli consecutivi su QEMU, tornando
ogni volta al prompt. Questo verifica anche il rilascio del segnale e il riuso
degli slot interni. Il Core Milk-V Mars compila con la stessa ABI; il gate su
hardware reale resta da eseguire prima di usare il contratto nel lifecycle
permanente di Init.
