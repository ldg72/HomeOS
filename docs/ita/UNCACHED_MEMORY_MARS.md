# Memoria non cachata sulla Milk-V Mars — il muro degli 8 MB

**Stato**: analisi verificata nel codice del Core e di Exec64 OS.
**Data**: 2026-10-08.
**Ambito**: qualunque OS sulla Mars che debba far leggere un buffer a una
periferica via DMA. HomeOS ci ha sbattuto contro con il cursore hardware;
Exec64 OS lo incontrera' appena implementera' il cursore sulla Mars.

---

## 1. Il fatto: il DMA non e' coerente con la cache

Sulla JH7110 il percorso DMA che serve display e periferiche **non e' coerente
con la cache della CPU**. Se la CPU scrive un buffer nella RAM normale e poi lo
consegna a una periferica, la periferica puo' leggere dati vecchi: le scritture
sono ancora nella cache.

Questo non e' un difetto della scheda ne' una scelta di qualcuno: e' come e'
fatta. Va conosciuto, perche' determina *dove* possono stare i buffer delle
periferiche.

**Conseguenza**: ogni buffer che una periferica legge via DMA va scritto dalla
CPU attraverso un percorso **non cachato**. Non e' un'ottimizzazione: e' una
condizione di correttezza.

---

## 2. Come si ottiene la memoria non cachata

La scheda espone un **alias non cachato** della RAM bassa:

```
indirizzo non cachato  =  indirizzo fisico  +  0x400000000
```

Non e' una convenzione software: e' la decodifica degli indirizzi del SoC. La
RAM resta dov'e', e chi passa da quell'indirizzo non incontra la cache.

Perche' un OS possa usarlo, pero', quell'intervallo deve essere **mappato nelle
sue tabelle di pagine**. Ed e' qui che entra in gioco il Core.

---

## 3. Quanta memoria non cachata abbiamo

Il Core mappa l'alias **solo per la riserva framebuffer**:

| | |
|---|---|
| fisico | `MARSFB_PHYS` = `0x70000000` |
| alias della CPU | `MARSFB_UNCACHED` = `0x470000000` |
| dimensione | `MARSFB_BYTES` = 1920 x 1080 x 4 = **8.294.400 byte** |

Le costanti stanno in `exec64/marsfb_memory.h`, e **i due progetti le usano
entrambi**: `core/riscv64/arch/riscv64/cpu/mmu.c` per mappare, e
`os/devices/jh7110_display/jh7110_hw.c` per disegnare. La finestra e' la stessa
per chiunque.

La riserva e' dimensionata **esattamente** su 1920x1080: `8.294.400` byte e'
anche la misura del framebuffer a 1080p. Non e' un caso, ed e' la radice di
tutto.

---

## 4. Il muro: a 1080p non c'e' posto per nient'altro

| Risoluzione | Schermo visibile | Spazio libero nella finestra |
|---|---:|---:|
| 640x480 | 1.228.800 byte | 7.065.600 |
| 1280x720 | 3.686.400 byte | 4.608.000 |
| 1920x1080 | 8.294.400 byte | **0** |

A 1080p la finestra non cachata e' **piena**: qualunque buffer DMA aggiuntivo —
il cursore, un descrittore, qualunque cosa una periferica debba leggere — non
ha dove stare.

### 4.1 L'esempio concreto

Il piano cursore del DC8200 legge la propria immagine via DMA: servono **16 KiB**
in memoria non cachata (64x64 pixel ARGB8888). A 1080p non ci sono, e non si
aggira scegliendo il cursore piccolo: 4 KiB non entrano in zero.

| | |
|---|---|
| 640x480 | cursore hardware: **si** |
| 1280x720 | cursore hardware: **si** |
| 1920x1080 | cursore hardware: **no** — serve altra memoria non cachata |

### 4.2 Un secondo sintomo, da conoscere

Se l'immagine resta dove non c'e' piu' spazio — per esempio perche' la
risoluzione e' cambiata e la collocazione non e' stata rifatta — il suo
indirizzo finisce **dentro i pixel visibili**, e il controller legge come
cursore un pezzo di schermo. Si vede un **quadrato nero** che si muove al posto
del puntatore. E' successo passando a 1080p con il cursore gia' acceso.

---

## 5. Chi decide cosa

E' importante non confondere i due piani, perche' uno si puo' cambiare e
l'altro no.

| | |
|---|---|
| **hardware** | il DMA non e' coerente con la cache, quindi serve memoria non cachata. Non si aggira. |
| **Core** | *quanta* di quella memoria l'OS puo' usare: oggi esattamente la riserva framebuffer. **Si cambia in software.** |

Il controller non ha preferenze su dove leggere l'immagine del cursore:
qualunque indirizzo fisico che riesca a raggiungere va bene. Il limite non e'
dove la periferica *legge*, e' dove l'OS riesce a *scrivere* senza cache.

---

## 6. Le strade, con costi e rischi

### (a) Allargare la riserva — la piu' piccola

64 KiB in piu' bastano per il cursore, e ne restano per altri usi. Il vincolo
dichiarato dal Core stesso e' solo che la riserva resti sotto
`RAM_START + 1 GB`: l'assert e' in `jh7110_hw.c`, e la riserva oggi finisce a
`0x707E9000`, molto prima di `0x80000000`.

**Verificato**: nel codice del Core non risulta **nulla** di mappato fra
`0x707E9000` e `0x80000000`, e quella zona e' dentro il range di RAM che il
Core stesso dichiara (`RAM_START`, `RAM_SIZE`). Non possiamo dimostrare che
nessun altro la usi — OpenSBI, per esempio, non e' codice che leggiamo — ma
nessuna parte del Core la tocca.

**Costo**: poche righe nel Core. **Rischio**: basso, ed e' confinato al Core.

### (b) Una seconda finestra non cachata — la piu' generale

Invece di allargare la riserva, mappare un altro intervallo di alias altrove,
di dimensioni scelte.

**Vantaggio**: non e' una soluzione per il cursore, e' una soluzione per *tutti*
i buffer DMA — cursore, descrittori USB, qualunque periferica futura. Oggi
l'unico modo di avere memoria non cachata e' litigare per lo spazio del
framebuffer, il che e' un incentivo sbagliato.

**Costo**: un po' piu' di (a) nel Core, piu' una regola da scrivere nel
contratto: dove sta questa finestra, quanto e' grande, come si alloca.

### (c) Cursore software a 1080p — nessuna modifica al Core

Il puntatore si disegna nel framebuffer e si nasconde salvando e rimettendo i
pixel sotto.

**Vantaggio**: si fa interamente lato OS, senza toccare nessuno.
**Costo**: si paga a ogni movimento — scritture sul framebuffer non cachato,
cioe' esattamente quello che il cursore hardware esiste per evitare — e porta
con se' tre complicazioni che il cursore hardware non ha: salvare i pixel sotto,
ridisegnarsi quando la console (o una finestra) scrive nella zona che copre, e
lampeggiare senza lasciare scie.

### (d) L'OS mappa l'alias da solo — sconsigliata

Da supervisor mode l'OS puo' scrivere nelle tabelle di pagine e aggiungere una
mappatura per l'alias di un buffer proprio. Non tocca il sorgente di nessuno.

**Perche' no**: mette l'OS dentro strutture che non sono sue, a runtime. Se il
Core le ricostruisce, o le percorre con assunzioni diverse, si rompe qualcosa in
un modo difficile da diagnosticare — e quel qualcosa sarebbe il display, cioe'
l'unico strumento che abbiamo per vedere cosa sta succedendo.

---

## 7. Cosa serve ai due progetti

**A chi sviluppa il Core / Exec64 OS**

- il device display di Exec64 OS usa le stesse costanti, quindi **ha la stessa
  finestra**: la decisione e' una sola e vale per tutti;
- sulla Mars quel device **non dichiara ancora il supporto al cursore**
  (`os/devices/jh7110_display/jh7110_device.c`: *"no cursor support advertised
  yet"*): e' terreno non battuto, e le misure di
  [CURSOR_HARDWARE_MARS.md](CURSOR_HARDWARE_MARS.md) sono il punto di partenza;
- con (a) o (b) il problema sparisce per entrambi gli OS in una volta sola.

**A HomeOS**

- il codice **gestisce gia' l'assenza**: a 1080p `cursor on` riporta che la
  riserva e' piena invece di mostrare spazzatura, e il cursore resta spento;
- quando la finestra ci sara', cambia solo il calcolo di dove mettere
  l'immagine: nessun'altra parte del driver.

---

## 8. Cosa e' verificato e cosa no

**Verificato leggendo il codice e misurando sulla scheda**

- i due progetti usano le stesse costanti di memoria (`exec64/marsfb_memory.h`);
- la finestra non cachata e' 8.294.400 byte, esattamente il framebuffer a 1080p;
- a 1080p non c'e' spazio: il cursore hardware e' stato rifiutato dal nostro
  stesso codice, che calcola la collocazione prima di scrivere i registri;
- con l'indirizzo dell'immagine dentro lo schermo visibile si vede un quadrato
  nero al posto del puntatore;
- fra la fine della riserva e `0x80000000` il Core non mappa nulla.

**Non verificato**

- che nessun altro componente (OpenSBI, firmware) usi quella zona: il Core non
  la usa, ma non e' codice nostro;
- quanto costerebbe davvero un cursore software a 1080p in termini di fluidita':
  non l'abbiamo scritto, quindi non l'abbiamo misurato.
