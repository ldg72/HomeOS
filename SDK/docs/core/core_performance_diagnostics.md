# Exec64 Core Performance Diagnostics

**Stato**: diagnostica privata e reversibile
**Branch iniziale**: `codex/core-performance`
**Checkpoint stabile di partenza**: `bf12e4c`
**Aggiornato**: 2026-09-01

## Scopo

Questa tranche misura il costo del confine Core/OS prima di applicare
ottimizzazioni. Non cambia la syscall ABI pubblica, le strutture UAPI o lo SDK.

La superficie diagnostica usa un numero `ecall` privato e temporaneo, definito
soltanto in `core/include/internal/perf_diag.h`. Non deve essere usato da
applicazioni, librerie o driver distribuiti.

I contatori coprono:

- ingressi nello scheduler e cambi task effettivi
- attivazioni MMU, scritture `satp`, `sfence.vma` e `fence.i`
- creazione degli address space U-Mode
- invocazioni e durata di `SYS_DO_IO`

Il contatore `SYS_DO_IO nonzero results` include qualsiasi risultato diverso
da zero. Non equivale necessariamente a un errore: per esempio
`INPUT_READ_EMPTY` usa il valore `1` per indicare normalmente che la coda input
e vuota.

I tempi sono letti con `rdcycle`. Sono adatti a confronti sulla stessa macchina
e configurazione QEMU, non a confronti assoluti tra hardware diversi.

## Build

La diagnostica e attiva per default nel branch di misura:

```sh
make core-qemu
make os-qemu
```

Per verificare il Core senza strumentazione:

```sh
make core-qemu PERF_DIAGNOSTICS=0
```

Ricompilare senza override ripristina la configurazione diagnostica.

## Comando residente

`PERFDIAG` e residente nella shell, quindi la lettura dello snapshot non carica
un nuovo ELF e non crea un task applicativo che altererebbe la misura.

```text
PERFDIAG SHOW
PERFDIAG RESET
PERFDIAG SAMPLE 10
```

`PERFDIAG` senza argomenti equivale a `PERFDIAG SHOW`.
`PERFDIAG SAMPLE` stampa l'avviso, azzera i contatori, attende esattamente dieci
secondi e produce automaticamente lo snapshot. Accetta una durata esplicita da
1 a 300 secondi. Durante l'intervallo l'operatore puo lasciare il sistema in
idle oppure produrre continuamente l'attivita da misurare, senza introdurre il
ritardo umano necessario a digitare `SHOW`.

## Protocollo baseline

Eseguire ogni scenario dopo un boot pulito e senza rete QEMU quando la rete non
fa parte del test.

```text
PERFDIAG RESET
DIR SYS:C
PERFDIAG SHOW
```

Ripetere separatamente per `ls`, avvio applicazioni e sessione grafica. Non
accumulare scenari diversi nello stesso intervallo di misura.

Per un test grafico temporizzato:

```text
LOADGFX
PERFDIAG SAMPLE 10
```

Lasciare il puntatore fermo per il campione idle oppure muoverlo per tutta la
durata del campione attivo.

## Primo baseline QEMU

Configurazione: `qemu-system-riscv64`, un hart, TCG, profilo `protect`, display
headless e rete disattivata.

Subito dopo il boot:

- 1 CPU online
- 1.153 cambi task effettivi
- 1.191 attivazioni MMU
- 21.625.000 cicli complessivi nelle attivazioni MMU
- 1.191 scritture `satp`

Intervallo controllato con un solo `DIR SYS:C`:

- 32 cambi task effettivi
- 38 attivazioni MMU
- 344.000 cicli complessivi, media 9.052 e massimo 33.000
- 38 scritture `satp` e 38 `fence.i`
- nessuna creazione di address space

Questo non dimostra ancora che la MMU sia il collo di bottiglia principale.
Dimostra pero che anche un comando residente di sola directory attraversa molte
volte il confine di address space. Il prossimo confronto deve separare questo
costo da exFAT/BootResource e dal present grafico.

## Baseline grafico e mitigazione polling

Un campione manuale con sessione grafica attiva ha mostrato 1.356 cambi task,
2.340 attivazioni MMU e 488 `SYS_DO_IO`. Le 984 attivazioni MMU eccedenti i
cambi task corrispondono quasi esattamente ai due cambi di address space
eseguiti attorno a ogni `SYS_DO_IO`. Dei 488 risultati, 283 erano non-zero e
comprendevano il normale `INPUT_READ_EMPTY`.

La prima mitigazione OS-side mantiene gli IRQ VirtIO input disabilitati ma usa
polling adattivo: 10 ms durante l'attivita e 40 ms dopo tre letture vuote. Le
used ring vengono notificate soltanto quando almeno un descriptor e stato
riciclato. Intuition usa 32 ms in idle e 100 ms a schermo chiuso; Impulse usa
32 ms quando l'analisi file in background e disabilitata. Questa tranche non
modifica Core o ABI e deve essere confrontata con lo stesso protocollo
`PERFDIAG RESET`/`SHOW`.

Il confronto successivo sulla build corretta ha separato tre casi manuali:

- shell inattiva: 894 cambi task, 894 attivazioni MMU, nessuna `SYS_DO_IO`
- grafica inattiva: 919 cambi task, 1.225 attivazioni MMU e 153 `SYS_DO_IO`
- movimento continuo: 1.044 cambi task, 7.652 attivazioni MMU e 3.304
  `SYS_DO_IO`

Nel terzo caso `1044 + (2 * 3304) = 7652`: ogni lettura input sincrona entra e
lascia lo spazio kernel. La seconda mitigazione OS-side aggiunge quindi
`INPUT_CMD_READ_EVENTS` a `input.device` 2.1 e permette a Intuition di leggere
fino a 128 eventi con una sola `DoIO`. E un'estensione append-only del device,
non una modifica del Core o della syscall ABI. Le durate manuali sono
approssimative: i valori servono come confronto relativo e devono essere
ricampionati dopo la modifica.

Il ricampionamento automatico di dieci secondi con `PERFDIAG SAMPLE 10` ha
separato la pipeline grafica in tre scenari ripetibili:

| Scenario | switch | MMU activate | `SYS_DO_IO` | cicli `SYS_DO_IO` | media | massimo |
|---|---:|---:|---:|---:|---:|---:|
| desktop fermo | 1.001 | 1.413 | 206 | 11.187.000 | 54.305 | 140.000 |
| solo puntatore | 1.003 | 2.013 | 505 | 35.804.000 | 70.899 | 116.000 |
| trascinamento Window | 1.081 | 1.879 | 399 | 72.903.000 | 182.714 | 1.254.000 |

In tutti i casi vale esattamente `MMU activate = switch + 2 * SYS_DO_IO`.
Il desktop fermo produce soprattutto letture input vuote; il puntatore aumenta
le richieste brevi, mentre il trascinamento esegue meno richieste ma rende ogni
`DoIO` molto piu costosa. Questo identifica il `VGPU_CMD_PRESENT` sincrono,
dopo il compositing CPU della dirty region, come collo di bottiglia prioritario.

La tranche sperimentale successiva resta OS-side: `virtiogpu.device` accoda e
unisce i rettangoli `PRESENT`, mentre un worker del backend invia
`TRANSFER_TO_HOST_2D + RESOURCE_FLUSH` e attende l'interrupt VirtIO fuori dalla
syscall del task Intuition. Il fallback rimane sincrono finche il worker non e
pronto. Il confronto prima/dopo deve usare gli stessi tre scenari prima di
accettare la modifica.

Il confronto dopo l'introduzione del worker ha prodotto:

| Scenario | switch | MMU activate | `SYS_DO_IO` | cicli `SYS_DO_IO` | media | massimo |
|---|---:|---:|---:|---:|---:|---:|
| desktop fermo | 1.002 | 1.402 | 200 | 10.332.000 | 51.660 | 73.000 |
| solo puntatore | 1.003 | 2.011 | 504 | 34.509.000 | 68.470 | 102.000 |
| trascinamento Window | 1.229 | 2.681 | 726 | 52.583.000 | 72.428 | 152.000 |

Nel drag il costo medio della syscall scende del 60,4%, il massimo dell'87,9%
e il totale del 27,9%. Il numero di richieste cresce perche Intuition non resta
piu bloccata e riesce a produrre piu aggiornamenti. Il lavoro interno del worker
non attraversa `SYS_DO_IO` e non e incluso in questi cicli: i numeri misurano
la rimozione del blocco dal chiamante, non il costo GPU totale. La comparsa di
icone ed elenco risulta quasi immediata; il trascinamento resta limitato dal
compositing CPU e da una cadenza di frame non ancora regolare.

## Regole

- nessuna ottimizzazione deve essere accettata senza confronto prima/dopo
- il checkpoint `bf12e4c` resta il ritorno stabile
- la diagnostica non deve diventare ABI Core pubblica per inerzia
- niente log nel percorso caldo: si leggono soltanto snapshot aggregati
- `platforms/MARS_BOOT/` resta fuori da questa tranche
