# Limiti del Core visti da un altro OS

**Data**: 2026-10-06.
**Contesto**: HomeOS è un OS minimale che gira come payload `Init` del Core
Exec64 sulla Milk-V Mars, scritto **senza** usare nulla di Exec64 OS: niente
`dos.library`, niente `graphics.library`, niente device, niente interfacce,
niente modello Amiga.

Scrivere un OS che ignora deliberatamente quel modello è servito come sonda:
ha fatto emergere assunzioni del Core che Exec64 OS non incontra, perché è
stato costruito attorno a esse. Queste note sono per chi mantiene il Core.

---

## 1. In supervisor non esistono le syscall

Il Core intercetta gli `ecall` dei task **U-Mode**. Un payload caricato in
**S-Mode** (profilo `performance`) che esegue `ecall` non parla con il Core:
parla con **OpenSBI**, in M-Mode. Il sintomo è una raffica di:

```
sbi_ecall_handler: Invalid error 104 for ext=0x2 func=0x10000000
```

dove `104` è il carattere `h` che l'utente ha appena digitato — perché il
valore di ritorno viene interpretato come codice d'errore.

**Conseguenza**: in S-Mode vanno usate le chiamate dirette e l'accesso
diretto ai registri. Nel caso nostro:

| U-Mode (syscall) | S-Mode (diretto) |
|---|---|
| `SYS_KPUTS` | `SysBase->ex_DebugPutS` |
| `SYS_KGETC` | `SysBase->ex_DebugGetC` |
| `SYS_AVAIL_MEM` | `SysBase->ex_IExec->AvailMem` |
| `SYS_GET_TICK` | `rdtime` |
| `SYS_YIELD` | `wfi` |
| `SYS_MMIO_*` | accesso MMIO diretto |

Non è un difetto: è la definizione di "OS supervisor". Ma **non è documentato
da nessuna parte**, e si scopre solo sbagliando. Vale la pena scriverlo nel
contratto del profilo.

---

## 2. La memoria: 512 MB, e non è una scelta dell'OS

```c
#define RAM_START         0x40000000
#define RAM_SIZE          (4096ULL * 1024 * 1024)   /* 4GB — costante!   */
#define KERNEL_HEAP_START (RAM_START + 0x10000000)  /* 0x50000000        */
#define KERNEL_HEAP_SIZE  (512 * 1024 * 1024)       /* 512 MB            */
```

Tre osservazioni.

**`RAM_SIZE` è una costante di compilazione e vale 4 GiB.** Il Core *ha* il DTB
— lo usa per trovare il range dell'initrd — ma non ne legge il nodo `/memory`.
Su una scheda con 8 GB il Core ne dichiara 4, e la RAM oltre non è mappata.
Per usarla serve leggere `/memory` dal device tree e mappare di conseguenza.

**L'heap è 512 MB, diviso in due pool da 256**: `CodeRAM` (eseguibile) e
`DataRAM`. La divisione è fissa al 50%.

**Il framebuffer sta immediatamente dopo l'heap**, e c'è un `_Static_assert`
che lo impone:

```
0x50000000  CodeRAM  256 MB
0x60000000  DataRAM  256 MB
0x70000000  MARSFB_PHYS — riserva framebuffer, 8 MB
```

Quindi **non si può ingrandire l'heap senza spostare la riserva video** — e
quell'indirizzo è programmato dentro il DC8200 come base dello scanout, quindi
la modifica ricade su tutti i driver display, Core e OS.

Un'architettura che eviterebbe il problema: un pool basso "DMA-safe" sotto i
4 GiB (dove stanno heap e framebuffer) e il resto della RAM come pool generale
mappato a parte.

**Vincolo da non perdere**: le periferiche JH7110 indirizzano solo i primi
4 GiB. La RAM sopra i 4 GiB va bene per kernel e applicazioni, non per i
descrittori DMA.

---

## 3. L'alias non cachato non è generale

Esiste un alias non cachato della RAM (`fisico + 0x400000000`), ma nella
configurazione attuale copre **solo gli 8 MB del framebuffer**, pagina per
pagina. Non è utilizzabile per rendere coerenti buffer DMA arbitrari.

Su questa piattaforma la coerenza per USB/VirtIO si ottiene con il front-port
coerente e le sole barriere — i core U74 **non** espongono Zicbom, quindi
`cbo.clean` va in trap. Vale la pena che sia scritto, perché è controintuitivo:
chi viene da altre piattaforme cerca la manutenzione esplicita della cache, e
qui non c'è.

---

## 4. Il profilo di Init passa da un file exFAT nascosto nel bundle

Il profilo (`protect` / `performance`) **non** si sceglie con il catalogo di
boot. Il Core monta la risorsa **exFAT** del bundle e legge:

```
SYS:Prefs/default.prefs      →      system-profile=performance
```

Se il file non c'è, il default è `protect` (Init in U-Mode) e un payload che si
aspetta il supervisor riceve invece un contesto U-Mode.

Due note:

- il percorso `SYS:Prefs/...` è un riferimento al modello Amiga/Exec64 OS
  dentro il Core. La *policy* dovrebbe appartenere al bundle, come dice il
  contratto stesso: oggi è un file su un filesystem di un OS specifico;
- per avviare un OS **completamente estraneo** serve comunque fornire
  un'immagine exFAT con dentro quel solo file. Nel nostro caso il bundle
  contiene 2 MB di exFAT per un file di 30 byte.

---

## 5. Il framebuffer "lasciato da U-Boot": una pista che porta fuori strada

All'avvio il Core stampa:

```
Exec: Milk-V Mars Detected. Activating Static U-Boot Framebuffer Override.
Exec: Found fb! Base: 0x00000000FE000000, W: 1920, H: 1080
Exec: Framebuffer filled (Stripes).
```

Sembra che esista un framebuffer già pronto. In realtà U-Boot, prima di cedere
il controllo, **spegne il display** (la sua routine `sf_vop_remove` ferma lo
scanout, asserisce i reset e disabilita clock e alimentazione). Le strisce
vengono scritte in una memoria che **nessuno sta scandendo**: non si vedono, e
non si sono mai viste nei nostri test.

È un percorso diagnostico interno al Core che contraddice il resto del modello
(la riserva vera è a `0x70000000` con alias non cachato a `0x470000000`).
Chi scrive un OS lo trova fuorviante: è la prima cosa che si prova a usare, e
non funziona per un motivo che non è nel log.

---

## 6. Il refuso nel font del Core

Il font 8×16 dentro il Core (`font_modern.c`) ha il glifo `}` scritto come:

```c
{0x00,0x30,0x08,0x08,0x04,0x08,0x08,030,0x00,...}
                                     ^^^
```

`030` in C è un **letterale ottale**: vale 24 (`0x18`), non `0x30`. Nella copia
OS-owned dello stesso font (`os/libraries/graphics/font_impulse.h`) il valore
è corretto. Effetto: nella console del Core la parentesi graffa chiusa esce
sbagliata.

---

## 7. La versione del Core non e' leggibile dall'OS

Il Core annuncia la propria versione sulla seriale:

```
   EXEC64 OS (v0.125 - 2026-08-24)
```

ma **non la espone a Init**: non c'e' un campo in `ExecBase`, non c'e' nulla nel
contesto di avvio, e `EXEC64_VERSION_STR` e' una costante interna al Core.

Conseguenza pratica: un OS che vuole mostrare "girando su <Core> versione X"
deve **tenere quel numero scritto a mano** e ricordarsi di aggiornarlo — e non
ha modo di accorgersi se è sbagliato. In HomeOS e' in `OS/version.h` con un
commento che lo dice.

Un OS ha interesse legittimo a sapere su cosa gira: serve per la diagnostica,
per i messaggi di errore, e per rifiutare combinazioni non testate. Sarebbe
sufficiente un campo nel contesto di `Init` (magari major/minor numerici) o in
`SysBase`, e sarebbe coerente con il resto del contratto di avvio, che e' gia'
versionato e validato dal Core.

## 8. Il reset via SBI non e' utilizzabile su questa scheda

Da OS supervisor l'unica via per chiedere un riavvio completo e' l'estensione
SBI **SRST** (`0x53525354`), che da S-Mode arriva a OpenSBI in M-Mode. Sulla
Milk-V Mars **non funziona**: la chiamata fa cadere l'uscita video e la scheda
non riparte, restando ferma finche' non si toglie l'alimentazione.

L'osservazione che rende la diagnosi solida: dopo la caduta dello schermo **non
compare il nostro messaggio di ripiego**, che avrebbe ridisegnato la console.
Questo dice che la CPU non e' piu' arrivata a eseguire codice dell'OS — e' il
firmware a essersi fermato, non l'OS a essersi perso l'output.

Conseguenza pratica per chi scrive un OS: **non mettere il reset SBI su un
percorso automatico o di default.** La funzione esiste ed e' quella corretta
per l'architettura, ma su questo firmware va trattata come un tentativo
esplicito, con la consapevolezza che puo' richiedere un intervento fisico.

Un riavvio *sicuro* della sola sessione — svuotare la console e ripresentare
la schermata di avvio — non tocca l'hardware e funziona sempre. E' quello il
comportamento da mettere come default.

## 9. Cosa ha funzionato bene

Per equilibrio, vale la pena dirlo: il contratto di boot è **solido**. Bundle
validato con magic e ABI, Init consegnato con un contesto versionato, rifiuto
deterministico con motivo sulla seriale, nessun nome di file cablato nel Core.
Passare da un OS all'altro è stato questione di sostituire un file: nessuna
modifica al binario del Core, e il messaggio di errore sulla seriale è sempre
stato sufficiente a capire cosa non andava.

Anche il profilo supervisor è risultato completo: `SysBase`, i servizi, la
mappatura del framebuffer e l'accesso MMIO hanno reso possibile scrivere
driver veri (display, USB) senza toccare il Core.
