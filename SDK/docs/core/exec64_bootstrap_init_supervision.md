# Exec64 - Bootstrap Supervisor e Init di Sistema

**Aggiornamento 2026-10-02:** Init/shell/programmi CLI supervisor sono ora
verificati in Performance su entrambe le CPU, con regressione Protect.
InitContext 1.2 consegna SysBase/servizi diretti solo al task supervisor;
Protect conserva il contesto 1.1. [Contratto e limiti](performance_boot_cli.md).


**Stato**: implementazione transizionale attiva
**Data**: 2026-08-24
**Ambito**: separazione funzionale tra Exec64 Core ed Exec64 OS

---

## 1. Obiettivo

Separare l'avvio minimo garantito dal Core dalla policy di avvio appartenente
al sistema operativo.

Il modello scelto conserva nel Core un supervisore minimale in `S-Mode` e
introduce un vero Init esterno in `U-Mode`:

```text
bootloader
  -> Exec64 Core
       -> BootstrapSupervisor (S-Mode)
            -> Init (U-Mode)
                 -> servizi Exec64 OS
                 -> DOS e filesystem
                 -> shell
```

Questa struttura consente allo stesso Exec64 Core di avviare Exec64 OS oppure,
in futuro, un altro sistema che fornisca un Init compatibile con la Core ABI.

### Nome pubblico universale

Il ruolo di boot si chiama sempre `Init`, ma la Core ABI non impone alcun nome
file o percorso DOS. Il bootloader carica un bundle in RAM; il bootstrap di
piattaforma valida il bundle e pubblica in `BootInfo` indirizzo, dimensione,
tipo e versione ABI del payload Init.

`Exec64Init` resta il nome sorgente dell'implementazione appartenente a Exec64
OS. Non e un nome cercato dal Core e non vincola un eventuale altro sistema
operativo, che puo organizzare liberamente il proprio filesystem.

## 2. Stato Attuale

L'attuale `BootstrapSupervisor` e interno a `core/kernel/kernel.c` e gira in
`S-Mode`. Il lifecycle della shell appartiene gia all'Init esterno. Init ABI
v1.1 gli consegna anche una Boot Resource opaca e il primo percorso OS-side
attiva:

- `exfat.handler` U-Mode per mount, directory e lettura
- `dos.library` U-Mode per assign, path e caricamento programmi

Il Core concentra ancora responsabilita transizionali appartenenti all'OS:

- inizializza DOS
- monta il filesystem exFAT di `SYS:`
- legge il profilo di sistema
- pre-registra alcune librerie Core necessarie al percorso corrente

Init non viene piu cercato in `SYS:`. Il Core riceve il descrittore
`BootInfo v1.1`, mappa il payload di boot in sola lettura e lo carica direttamente
dalla memoria con lo stesso loader ELF/W^X usato dal percorso normale.

Il Core non contiene piu il fallback diretto a `C:shell`: se Init manca entra
in recovery seriale minima. Il DOS/exFAT legacy resta inizializzato come
fallback e come write path temporaneo, quindi la separazione funzionale non e
ancora completa. Le implementazioni OS-side correnti sono linkate dentro Init;
non sono ancora moduli separati caricati da disco.

## 3. Responsabilita del BootstrapSupervisor

Il futuro `BootstrapSupervisor` resta nel Core e gira in `S-Mode`, ma deve
essere deliberatamente piccolo.

Deve soltanto:

- ricevere dal bootstrap la descrizione dell'immagine Init
- validare range, formato e compatibilita ABI
- creare lo spazio di indirizzamento `U-Mode` di Init
- avviare Init
- osservarne terminazione o fault
- applicare una policy minima e limitata di restart
- entrare in recovery, arrestare o riavviare il sistema dopo failure persistente

Non deve conoscere:

- nomi o path DOS
- exFAT o altri filesystem
- `C:shell`
- `S:startup-sequence`
- librerie, device o servizi specifici di Exec64 OS
- policy di restart della shell

## 4. Responsabilita di Init

`Init` appartiene all'OS selezionato e, nel profilo normale `protect`, gira in
`U-Mode`. L'implementazione corrente di Exec64 OS vive in
`os/init/exec64_init.c`.

Deve occuparsi di:

- ricevere dal Core handle e descrittori delle risorse di boot
- avviare i servizi OS necessari
- rendere disponibili DOS e gli handler filesystem
- montare `SYS:`
- determinare la configurazione OS ammessa dal Core
- caricare librerie e device previsti dal profilo
- eseguire la startup-sequence
- avviare e supervisionare la shell
- applicare retry, backoff e policy degradata della shell

Init non accede direttamente a CSR, MMIO, page table o strutture private del
kernel. Usa esclusivamente Core ABI, syscall, handle, porte e messaggi pubblici.

## 5. Supervisione Generica dei Task

Il Core dispone di un registro privato per notificare la terminazione di Init
al `BootstrapSupervisor`. Dal 2026-08-29 lo stesso meccanismo ha una prima ABI
pubblica e generica per task figli U-Mode, documentata in
[exec64_task_supervision_abi.md](exec64_task_supervision_abi.md).

Il contratto dovra permettere a un supervisore di:

- creare oppure adottare un task figlio
- ricevere un evento quando il task termina
- distinguere return, richiesta di exit, rimozione ed eccezione
- ottenere un dettaglio minimo del fault senza accedere alla `struct Task`
- rilasciare l'handle del task terminato
- crearne una nuova istanza secondo la propria policy

La struttura pubblica dell'evento e versionata e non espone puntatori kernel.
L'ABI v1 usa handle opachi owner-bound e i numeri syscall append-only 54-56.

## 6. Gestione dei Fault

Quando la shell genera un guru:

1. il Core intercetta l'eccezione in `S-Mode`
2. il Core ferma il task e ripulisce le risorse di propria competenza
3. il Core invia a `Exec64Init` un evento di terminazione
4. Init decide se e quando riavviare la shell

Se invece termina o va in fault `Exec64Init`:

1. il Core contiene il fault nel suo address space `U-Mode`
2. `BootstrapSupervisor` riceve l'evento
3. il Core tenta un restart limitato di Init
4. dopo failure persistente applica una policy minima di recovery

La policy di recovery di Init appartiene al Core. La policy relativa a shell e
servizi appartiene all'OS.

## 7. Relazione con il Dual-Profile

Il Core resta l'autorita finale sul privilege level dei componenti.

- `protect`: `Exec64Init` e i componenti OS partono in `U-Mode` per default
- `performance`: `Exec64Init`, componenti OS e applicazioni partono in `S-Mode`
  e usano binding diretti nello spazio flat condiviso
- profilo misto: ogni componente viene risolto al caricamento secondo policy,
  requisiti e autorizzazioni

In Performance `Exec64Init` appartiene alla Trusted Computing Base anche se il
suo lavoro non e un hot path: mantenere Init in USER trasformerebbe Performance
in una configurazione Mixed e introdurrebbe un confine non previsto dal profilo.
In Protect e Mixed Init non puo promuovere autonomamente altri componenti.

Il profilo iniziale non potra dipendere dalla lettura Core-side di
`SYS:Prefs/default.prefs` dopo l'estrazione di DOS/exFAT. Il default del Core
resta `protect`; un eventuale profilo iniziale diverso dovra arrivare tramite un
contratto di boot validato e diventare immutabile prima del caricamento dei
moduli supervisor.

## 8. Sequenza di Migrazione

La migrazione deve mantenere avviabili QEMU e Milk-V Mars dopo ogni passaggio:

1. [completato] definire il contratto generico di supervisione task
2. usare il nuovo contratto con l'attuale `InitSupervisor` S-Mode
3. [completato] aggiungere l'immagine separata `Init` e il relativo descrittore
   di boot
4. [completato] far avviare e sorvegliare Init al `BootstrapSupervisor`
5. [completato] spostare in Init il lifecycle della shell
6. [in corso] estrarre DOS come `dos.library` di Exec64 OS
7. [in corso] estrarre exFAT come `exfat.handler`
8. rimuovere i percorsi legacy solo dopo test positivi e negativi su entrambe le
   piattaforme

Il contratto di supervisione interno e l'handoff Init sono gia disponibili. La
tranche corrente valida ora il confine Boot Resource e il primo percorso di
lettura DOS/exFAT in U-Mode senza rimuovere il fallback legacy.

### Stato di implementazione - 2026-08-24

Il primo passo e stato introdotto come meccanismo esclusivamente interno al
Core:

- un task puo essere associato a un supervisore e a un segnale dedicato
- `RemTask()` deposita un evento con `reason` e `detail` prima del teardown
- il supervisore consuma esplicitamente l'evento dopo il wakeup
- l'attuale shell usa gia questo percorso senza cambiare la propria policy di
  restart

Il registro interno resta volutamente piccolo e privato. La prima ABI pubblica
e ora disponibile tramite handle opachi, evento versionato e syscall
append-only. Il 2026-08-29 `Exec64Init` e stato migrato dal lancio sincrono al
percorso supervisionato: crea una shell U-Mode separata, attende il solo
segnale assegnato, consuma l'evento e scarica l'immagine solo dopo la
terminazione. La policy di nuova sessione resta nell'OS.

Il comportamento transizionale e stato validato su entrambe le piattaforme:
QEMU ha completato due restart consecutivi della shell e Milk-V Mars reale ha
completato `exit` con evento `RETURN(1)` e avvio della shell generation 2.

La tranche successiva ha aggiunto `SYS:System/Exec64Init` come ELF appartenente
alla sola immagine OS. Prima dell'handoff di boot sono stati validati:

- caricamento/ritorno/unload del probe per due cicli consecutivi su QEMU e
  Milk-V Mars reale
- caricamento sincrono della shell da parte di Init
- restart di una nuova sessione shell dopo il normale comando `exit`
- hash invariato del Core dopo il rebuild della sola immagine OS

Il primo handoff di boot e ora implementato. Dopo il mount di SYS il
`BootstrapSupervisor` carica la stessa immagine come task U-Mode dedicato e la
registra nel meccanismo interno di supervisione. QEMU ha validato:

- handoff positivo da bootstrap a Init e arrivo al prompt
- nuova sessione shell dopo `exit`, sotto ownership di Init
- fallback legacy fino al prompt quando `System/Exec64Init` e assente

La shell ora viene eseguita come task U-Mode distinto supervisionato da Init.
Un fault della shell viene contenuto dal Core, consegnato a Init come evento
bounded e non termina Init; Init applica poi la propria policy di nuova
sessione. DOS, exFAT e mount restano transitoriamente nel Core. La validazione
del precedente handoff e stata completata anche su Milk-V Mars reale: boot fino
al prompt, comando `version`, ritorno della shell dopo `exit` e nuova sessione
creata da Init. Il gate runtime della nuova separazione shell/Init va eseguito
su QEMU e Mars.

La fase transitoria precedente ha reso universale il nome del ruolo nel
filesystem:

- `Makefile.init` produce `System/Init` e un alias transitorio `Exec64Init`
- il Core tenta prima `System/Init` e usa l'alias solo per compatibilita
- il task supervisionato si chiama `Init`
- il Core non conosce piu `C:shell` e non la avvia se Init manca
- Init riceve al massimo tre restart rapidi con backoff crescente
- dopo assenza o failure persistente il Core stampa la recovery sulla seriale e
  attende il reset senza fingere di avere avviato un OS

QEMU ha validato il nome primario, l'alias legacy, l'assenza completa di Init e
la terminazione ripetuta. In quest'ultimo test i tre backoff sono stati
`250/500/750 ms`, seguiti dalla recovery senza shell.

Questa neutralita e la base **POSIX-ready** del Core: rende possibile un futuro
Init differente senza inserire oggi utenti, permessi POSIX, namespace o policy
di un sistema specifico nel kernel.

Il passo successivo, approvato esplicitamente con il maintainer, ha eliminato
anche la dipendenza dal percorso filesystem:

- `BootInfo` e stato esteso in modo append-only da v1.0 a v1.1 con il descrittore
  del payload Init
- un bundle di boot versionato contiene sempre Init ELF64 e puo contenere anche
  l'immagine exFAT del sistema
- QEMU e Milk-V Mars usano lo stesso parser del bundle e lo stesso contratto
  Core, mantenendo producer board-specific separati
- il Core split non contiene piu le stringhe `SYS:System/Init`,
  `SYS:System/Exec64Init` o `C:shell`
- senza descrittore Init il Core stampa `NO_DESCRIPTOR`, non tenta alcuna shell
  e resta nella recovery seriale minima
- QEMU ha validato boot fino al prompt, `version`, ritorno da `exit` e nuova
  sessione, con Init assente dal filesystem SYS
- il test negativo QEMU senza bundle ha validato la recovery senza fallback

Il gate su Milk-V Mars reale del nuovo bundle e completato sia nel percorso
positivo sia nella recovery senza descrittore Init. DOS, exFAT, mount e lettura
del profilo restano ancora transitoriamente nel Core e costituiscono il prossimo
confine funzionale da estrarre.

La tranche Init ABI e ora arrivata alla v1.1. Il Core passa in `a0` un
`InitContext` versionato e privo di puntatori kernel; il campo aggiunto in coda
contiene una Boot Resource opaca, documentata in
[exec64_boot_resource_abi.md](exec64_boot_resource_abi.md).

Su QEMU e Milk-V Mars Init monta SYS tramite il nuovo `exfat.handler`, installa
il binding bootstrap di `dos.library` e ha validato `dir` e caricamenti ripetuti
di `hexdump`. Il passaggio read-only e quindi consolidato su entrambi i backend.
Le mutazioni non migrate delegano ancora al DOS/exFAT legacy; la scrittura
ripetuta `MakeDir` ha mostrato instabilita e resta un issue separato.

## 9. Vincoli

- nessun puntatore kernel esposto come handle pubblico permanente
- nessun nuovo accesso diretto U-Mode a CSR o MMIO
- nessuna regressione W^X o nell'isolamento per-task
- nessuna dipendenza del Core da `os/`
- nessuna policy Exec64 OS incorporata nel Core
- nessun caricamento S-Mode implicito o deciso dal modulo stesso
- ABI append-only e versionata
- comportamento corrente conservato finche il nuovo percorso non e validato

## 10. Criterio di Completamento

La separazione funzionale e completata quando:

- [x] il Core avvia un `Init` esterno senza conoscerne nome o percorso nel
  filesystem di SYS
- un fault di Init viene contenuto e gestito dal `BootstrapSupervisor`
- [x] Init U-Mode monta il sistema e avvia la shell su QEMU e Milk-V Mars
- [x] percorso implementato: un guru della shell viene notificato a Init, che
  la riavvia (gate runtime QEMU/Mars della nuova separazione ancora pendente)
- la ricostruzione di Init, DOS, exFAT o shell non cambia il binario Core
- lo stesso Core completa il percorso su QEMU e Milk-V Mars
