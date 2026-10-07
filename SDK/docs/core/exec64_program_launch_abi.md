# Exec64 Program Launch ABI v1

**Aggiornamento 2026-10-02:** il launcher e ora verificato anche in Performance:
Init carica l'immagine supervisor, crea il figlio nello stesso dominio e gli
consegna le interfacce dirette. L'ABI StartProgram non cambia. La qualificazione
corrente copre programmi CLI; applicazioni GUI e Mixed restano successivi.
[Checkpoint e prove](performance_boot_cli.md).


**Stato**: prima tranche OS-side implementata
**Data**: 2026-08-30
**Roadmap di origine**: `ROADMAP_DUALMODE`

> Questo documento descrive la prima tranche U-Mode gia implementata. Non e il
> contratto finale dei profili: la Foundation 1.2 richiede che il launcher
> selezioni U-Mode in Protect, S-Mode/EL1 in Performance e il dominio indicato
> dalla policy in Mixed. L'ABI pubblica di avvio deve restare comune.

## Obiettivo

Permettere a componenti U-Mode distinti, incluso `ImpulseDesktop`, di chiedere
l'avvio di un programma senza eseguire direttamente il suo entry point e senza
trasferire puntatori privati tra address space.

Il process manager iniziale e `Init`. Il Core fornisce soltanto il meccanismo
generico gia approvato di supervisione dei task U-Mode.

## Contratto pubblico

`sdk/include/dos/program.h` definisce
`Exec64ProgramLaunchRequest` ABI 1.0. Il record contiene esclusivamente valori
copiati e bounded:

- path dell'immagine
- argomenti
- directory corrente
- modalita CLI o GUI
- versione, dimensione e campi riservati

`dos.library` accoda `StartProgram()` alla propria interfaccia. Il layout
precedente non viene modificato e il test `dos_abi_contract.c` congela offset e
dimensioni.

## Flusso protect

1. il client prepara una richiesta locale
2. il binding DOS acquisisce una mailbox bounded nell'immagine condivisa di
   `Init` e crea il reply port nel task chiamante
3. il messaggio viene inviato alla porta posseduta da `Init`
4. `Init` copia e valida la richiesta, carica l'ELF e prepara un task U-Mode
5. `Init` registra il task con `AddSupervisedTask()` e risponde al chiamante
6. alla terminazione `Init` consuma l'evento con `TakeTaskExit()`, scarica
   l'immagine e libera task, stack e contesto

Il chiamante conserva la mailbox fino alla risposta e la rilascia poi con una
transizione atomica. Le mailbox sono state create prima dei task client e fanno
parte dell'immagine condivisa di `Init`: non dipendono dalla propagazione di
allocazioni heap sub-page verso address space gia esistenti. Il contesto del
figlio e invece allocato e liberato da `Init`.

## Policy iniziale

- massimo quattro programmi lanciati contemporaneamente
- applicazioni sempre U-Mode in questa tranche
- nessun restart automatico delle applicazioni
- un fault applicativo viene contenuto e le risorse vengono recuperate
- `epl_CurrentDirectory` e trasportata ma non e ancora applicata al binding DOS
- associazioni file/tipo e icone `.info` restano lavoro successivo

## Primo consumer

`ImpulseDesktop` usa `StartProgram()` sul doppio clic di una entry file. Il
primo eseguibile grafico definitivo e `C:about`, che apre una Window Intuition,
resta indipendente dal desktop e termina tramite il normale close gadget.
Il comando `run` espone lo stesso percorso dalla shell e usa `C:` come ricerca
predefinita per i nomi privi di path.

## Confini

Questa tranche non aggiunge syscall e non modifica il Core. Il futuro profilo
performance potra cambiare il binding di esecuzione conservando invariati
record e semantica pubblica del lancio.
