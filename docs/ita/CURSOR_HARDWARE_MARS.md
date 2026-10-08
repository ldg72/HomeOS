# Cursore hardware sulla Milk-V Mars (DC8200)

**Stato**: funzionante e verificato su hardware reale.
**Data**: 2026-10-08.
**Ambito**: usare il piano cursore del display controller della Mars da un OS
bare-metal, senza passare da DRM, da un compositore o da una libreria grafica.

Questa non è una traduzione di documentazione: è quello che abbiamo dovuto
scoprire per farlo funzionare, con i valori letti dalla scheda e gli errori in
cui si inciampa.

---

## 1. Perché vale la pena

Il cursore hardware è un piano del display controller, **indipendente dal piano
primario** su cui l'OS disegna. Si sposta scrivendo la sua posizione in un
registro: **non costa un pixel di disegno**, nessun salvataggio e ripristino
dello sfondo, nessun ridisegno.

Sulla Mars questo non è un lusso. Il framebuffer si scrive attraverso l'alias
**non cachato**, quindi ogni pixel disegnato costa una scrittura in memoria:
un cursore software pagherebbe a ogni movimento, quello hardware no. È la
differenza fra un puntatore fluido e uno a scatti.

---

## 2. Cosa offre il controller

| | |
|---|---|
| piani | due (`CURSOR_PLANE_0`, `CURSOR_PLANE_1`), uno per pannello |
| formato | **ARGB8888** — a colori e con trasparenza, non monocromatico |
| dimensione | 32×32 o 64×64 |
| sovrapposizione | sopra tutto (z = 255) |
| scalatura | nessuna |

Il piano cursore è **sempre a colori**: non è il cursore a due colori di altre
epoche. L'immagine porta il suo colore e la sua trasparenza.

---

## 3. Mappa dei registri

Base del display controller: `DC = 0x29400000`. Tutti gli offset sono da quella.

| Offset | Nome | Uso |
|---:|---|---|
| `0x1468` | `DC_CURSOR_CONFIG` | abilitazione, dimensione, punto caldo |
| `0x146C` | `DC_CURSOR_ADDRESS` | indirizzo **fisico** dell'immagine |
| `0x1470` | `DC_CURSOR_LOCATION` | `x` nei bit 0-15, `y` nei bit 16-31 |
| `0x1474` | `DC_CURSOR_BACKGROUND` | colore di sfondo (retaggio) |
| `0x1478` | `DC_CURSOR_FOREGROUND` | colore di primo piano (retaggio) |
| `0x1484` | `DC_CURSOR_CLK_GATING` | definito dal driver, **mai usato**: qui non serve |
| `0x1080` | `DC_CURSOR_OFFSET` | da sommare per il secondo cursore |
| `0x24E8` | `DC_CURSOR_CONFIG_EX` | definito dal driver, mai usato |

### 3.1 I bit di `DC_CURSOR_CONFIG`

| Bit | Significato |
|---|---|
| 0-4 | campo di abilitazione: in scrittura valgono `0x0E` (bit 1, 2 e 3) |
| 5-7 | dimensione: **0 = 32×32, 1 = 64×64** |
| 8-15 | `hot_y` |
| 16-23 | `hot_x` |

La codifica della dimensione non è documentata nel driver: è dichiarata
nell'header, come enumerazione (`CURSOR_SIZE_32X32 = 0`, `CURSOR_SIZE_64X64`).
I bit bassi non hanno un nome, e non li abbiamo interpretati: si copia quello
che scrive il driver.

---

## 4. La sequenza, e la prima trappola

Accensione:

```
DC+0x1474 = 0x00000000        colore di sfondo
DC+0x1478 = 0x00FFFFFF        colore di primo piano
DC+0x146C = indirizzo fisico dell'immagine
DC+0x1470 = x | (y << 16)
DC+0x1468 = (1 << 5) | 0x0E   dimensione 64x64, abilitato
```

**La trappola: scrivere la posizione da sola non ha alcun effetto.**

È il primo errore che abbiamo fatto, e sembra tutto tranne un errore di
sequenza: si danno le coordinate e il puntatore non si muove; si manda un
qualunque altro comando che riscriva la configurazione, e il puntatore salta
alle coordinate che erano state date prima. Il blocco del cursore ricarica le
sue copie di lavoro quando si riscrive `DC_CURSOR_CONFIG`, quindi **ogni
spostamento deve riscrivere indirizzo, posizione e configurazione insieme** —
che è esattamente quello che fa il driver di riferimento a ogni commit, e che
noi avevamo "ottimizzato" scrivendo la sola posizione.

Conseguenza pratica: il costo di uno spostamento è di tre scritture di
registro, non una. È irrilevante, e non vale la pena provare a risparmiarlo.

---

## 5. L'immagine, e dove metterla

Serve un buffer **ARGB8888** di 64×64 (16 KiB) — conviene sempre la misura
massima, anche se si usa la modalità 32×32, così l'aspetto non dipende dal
campo dimensione. Il disegno sta in alto a sinistra e la punta del puntatore è
il pixel `(0,0)`, che è anche il punto caldo (`hot_x = hot_y = 0`).

### 5.1 Deve stare in memoria non cachata

Il controller legge l'immagine via DMA. Sulla Mars l'unica zona con l'**alias
non cachato** è la riserva framebuffer (`0x70000000` fisico, `0x470000000` per
la CPU), quindi l'immagine va messa lì: subito dopo lo schermo visibile,
allineata a una pagina.

| Risoluzione | Schermo visibile | Posto per il cursore |
|---|---:|---|
| 640×480 | 1.228.800 byte | sì |
| 1280×720 | 3.686.400 byte | sì, ne avanzano 4,6 MB |
| 1920×1080 | 8.294.400 byte | **no: la riserva è esattamente piena** |

A 1080p la riserva framebuffer non ha un byte libero, quindi il cursore
hardware non ha dove mettere l'immagine. Due strade: chiedere al Core una
seconda finestra non cachata, oppure a quella risoluzione ripiegare sul
cursore software. Va deciso prima, non a runtime.

### 5.2 Attenzione al cambio di risoluzione

L'immagine sta **subito dopo** lo schermo visibile: se la risoluzione cambia e
l'immagine non viene ricollocata, il suo indirizzo finisce **dentro** i pixel
visibili e il controller legge come cursore un pezzo di console. Il sintomo è
un **quadrato nero** che si muove con il mouse invece del puntatore. È successo
passando a 1080p. La collocazione va rifatta a ogni cambio di modo.

### 5.3 I colori, per non dipendere dalla trasparenza

- i pixel trasparenti sono **zero puro** (`0x00000000`), che è anche il valore
  scritto in `DC_CURSOR_BACKGROUND`;
- il contorno è un **grigio molto scuro**, non nero pieno.

Il motivo è lo stesso in entrambi i casi: non sappiamo con certezza se il
controller usi il canale alfa o una **chiave di colore** presa dal registro di
sfondo. Se usasse la chiave, il nero pieno del contorno coinciderebbe con il
trasparente e l'immagine perderebbe il contorno; e i pixel trasparenti a zero
funzionano in entrambi i casi.

---

## 6. La seconda trappola: nascondere il puntatore

`DC_CURSOR_CONFIG`, su questa scheda, **non spegne il cursore**: non lo fa la
scrittura del driver di riferimento riprodotta alla lettera (azzera due bit e
ne imposta uno) e non lo fa nemmeno azzerare **tutto il campo basso**. Provato
entrambi.

Peggio: azzerare il campo basso **ferma anche il meccanismo che applica la
posizione**. Il cursore resta congelato sullo schermo dov'era, e da lì non si
sposta più. Il sintomo è insidioso — il comando dice "spento" e il puntatore
resta lì — ed è quello che ci è successo la prima volta.

Il modo che funziona, e che costa **una sola scrittura**:

1. la configurazione si scrive **sempre** nella forma "acceso", anche quando il
   puntatore è spento — è la riscrittura della configurazione che fa applicare
   la posizione, quindi non va toccata;
2. per spegnere, la **posizione** va fuori dallo schermo (`0xFFFF`).

La posizione vera la tiene il software, quindi riaccendendo il puntatore
ricompare dov'era senza salti. Accendere è una sola sequenza di registri, e
spegnere sono tre scritture.

---

## 7. Cosa è stato verificato

Tutto sull'hardware, non dedotto:

- il piano cursore è vivo e si vede;
- l'immagine viene letta **dal posto giusto**: la riserva framebuffer con
  l'alias non cachato fa il suo lavoro;
- la **mappatura delle coordinate è diretta**: portando il puntatore su punti
  noti, contrassegnati da mirini disegnati sul framebuffer, la punta cade
  esattamente sul punto richiesto, su tutti e quattro gli angoli provati.
  Nessuno scambio fra gli assi, nessuna scala;
- la punta del disegno **è** il punto selezionato;
- il campo dimensione a `1` (64×64) è quello giusto;
- il clock gating **non serve**: non lo abbiamo mai toccato;
- la posizione si applica riscrivendo la configurazione, non da sola.
- il registro di configurazione **non spegne** il cursore: per nasconderlo si
  usa un'immagine trasparente (§6).

---

## 8. Come provarlo

I comandi di HomeOS, tutti a caldo:

```text
cursor                  stato: attivo, posizione, registri, indirizzo immagine
cursor X Y              porta il puntatore in un punto
cursor on | off         accende e spegne
cursor test             lo fa camminare dentro il bordo dello schermo
cursor probe            quattro mirini in punti noti, per verificare la mappatura
cursor shape <0..2>     forma del puntatore
```

Il puntatore si guida anche con le **frecce della tastiera**, otto pixel per
pressione, con ripetizione: tiene premute due frecce e va in diagonale. Serve a
sviluppare le finestre prima che esista il mouse.

---

## 9. Cosa non è ancora fatto

- **Non si accende all'avvio**, per scelta: si attiva alla prima chiamata,
  quando la grafica comincia a servire;
- non c'è ancora il **clic**;
- a 1080p manca lo spazio per l'immagine (§5.1);
- il cursore **software** non esiste, quindi sulle configurazioni senza spazio
  non c'è ancora un ripiego.
