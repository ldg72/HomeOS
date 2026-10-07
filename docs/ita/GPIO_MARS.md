# GPIO su Milk-V Mars (StarFive JH7110) — note di bring-up

**Stato**: driver scritto e compilato, **non ancora validato su hardware**.
Il self-test e' pronto: appena c'e' un pin libero, il primo comando dice se il
modello dei registri e' corretto.
**Data**: 2026-10-06.

Le informazioni qui raccolte vengono da due fonti, e la distinzione conta:

- **dal driver Linux** `gpio-starfive-jh7110.c` e `pinctrl-starfive-jh7110.c`:
  il layout dei registri, i campi e la loro semantica;
- **misurato sulla scheda**: solo il selettore di over-current, manovrato
  davvero durante il bring-up USB (sezione 3).

Tutto il resto e' da considerarsi **derivato e da verificare**.

---

## 1. Il blocco

Tutti i registri stanno nel **SYS IOMUX**, a base `0x13040000`. Lo stesso
blocco contiene sia il controllo dei pin sia la configurazione dei pad.

## 2. Mappa dei registri

| Registro | Indirizzo | Formato |
|---|---|---|
| `DOEN` | `0x000 + (pin & ~3)` | campo di **6 bit** a `8*(pin&3)` |
| `DOUT` | `0x040 + (pin & ~3)` | campo di **7 bit** a `8*(pin&3)` |
| `GPI` | `0x080 + (pin & ~3)` | campo di 7 bit a `8*(pin&3)` |
| `DIN` | `0x118` (pin 0-31) / `0x11c` (32-63) | 1 bit per pin |
| pad del pin | `0x120 + pin*4` per i pin 0..74 | vedi sotto |
| pad dei pin 89-94 | `0x284 + (pin-89)*4` | idem |

**Quattro pin per registro.** L'indirizzo dipende da `pin & ~3`, non dal pin:
il pin 25 usa il registro a `+0x18`, perche' `25 & ~3 = 24`. Il campo del pin
sta poi a `8 * (pin & 3)` bit. E' la struttura meno intuitiva del blocco, e
spiega perche' i campi sembrano a otto bit invece che a uno.

## 3. Semantica dei campi

### `DOEN` — attenzione, e' invertito

| valore | significato |
|---|---|
| **0** | **uscita** (il pin e' pilotato) |
| **1** | ingresso |

Sembra al contrario di come ci si aspetta: "DOEN" suggerirebbe "output
enable", e invece zero significa proprio che il pin e' pilotato. Il driver
Linux fa esattamente cosi', e la stessa convenzione e' stata usata per il
VBUS USB.

### `DOUT` — sorgente d'uscita

| valore | significato |
|---|---|
| 0 | livello basso |
| 1 | livello alto |
| > 1 | sorgente interna, es. `7` = `GPOUT_SYS_USB_DRIVE_VBUS` |

Non e' un bit di livello: e' un **selettore**. Per pilotare un pin come GPIO
servono 0 o 1.

### `GPI` — dove va l'ingresso

Instrada il segnale del pin verso una periferica interna. `0` e `1` sono le
costanti bassa e alta; `2` e', per esempio,
`GPI_SYS_IOMUX_U0_CDN_USB_OVERCURRENT_N_IO`.

**Questo campo e' l'unico misurato direttamente**: durante il bring-up USB il
pin dell'over-current era lasciato sulla costante bassa, e il controller
segnalava una sovracorrente **senza alcuna periferica collegata**. Portandolo
sulla costante alta il falso allarme e' sparito.

### `DIN` — lettura

Un bit per pin, che riflette il livello sul pad. Perche' la lettura funzioni
serve il **buffer d'ingresso abilitato** nel pad (bit 0 della configurazione).

### Configurazione del pad

| bit | significato |
|---|---|
| 0 | abilita il buffer d'ingresso |
| 1-2 | forza di pilotaggio (2/4/8/12 mA) |
| 3 | pull-up |
| 4 | pull-down |
| 5 | slew rate |
| 6 | schmitt trigger |

Un pin in uscita che si vuole anche **rileggere** deve avere il bit 0 a 1:
serve al self-test, e costa nulla.

## 4. Il registro che non esiste

Durante il bring-up USB, per il pin del VBUS era stata usata una scrittura a
`0x130402A0` con maschera `3 << 15`, scambiata per il registro di funzione del
pin. **Quell'indirizzo non corrisponde a niente** in questa mappa: sarebbe il
pin 96, fuori intervallo. La scrittura era quindi **un no-op silenzioso**, e il
pin funzionava comunque perche' era gia' in funzione GPIO.

Due cose che vale la pena scrivere:

- su questo bus una scrittura a un indirizzo inesistente **non produce alcun
  errore**: non c'e' modo di accorgersene se non leggendo la mappa per bene;
- nelle note del progetto di origine quel registro e' chiamato `FUNC` in piu'
  punti: e' un nome fuorviante da non riprendere.

## 5. Connettore a 40 pin

Fonte: documentazione ufficiale Milk-V. **Non e' la numerazione del
Raspberry Pi**: il pin fisico 3 qui e' `GPIO58`, sul Pi e' `GPIO2`. La
compatibilita' meccanica non dice nulla sulla numerazione.

| fisico | GPIO | fisico | GPIO |
|---:|---:|---:|---:|
| 3 | 58 | 24 | 49 |
| 5 | 57 | 26 | 56 |
| 7 | 55 | 27 | 45 |
| 8 | 5 | 28 | 40 |
| 10 | 6 | 29 | 37 |
| 11 | 42 | 31 | 39 |
| 12 | 38 | 32 | 46 |
| 13 | 43 | 33 | 59 |
| 15 | 47 | 35 | 63 |
| 16 | 54 | 36 | 36 |
| 18 | 51 | 37 | 60 |
| 19 | 52 | 38 | 61 |
| 21 | 53 | 40 | 44 |
| 22 | 50 | | |

Alcuni pin hanno una funzione alternativa dichiarata (I2C sui 3-5, UART sugli
8-10, SPI sui 19-23, LCD sui 12/29/31/36). Quelli **senza** funzione
alternativa sono i candidati migliori per il primo esperimento: `GPIO55`
(fisico 7) e `GPIO44` (fisico 40).

**Da evitare**: il pin 25, che e' il VBUS USB, gia' configurato dal driver
tastiera.

## 6. Come si verifica senza strumenti

Il driver espone un self-test che non richiede ne' multimetro ne' LED:

    gpio test <pin>

Il procedimento: il pin viene messo in uscita, portato basso e riletto da
`DIN`, poi portato alto e riletto. Su un pin libero il livello letto **deve
seguire** quello guidato.

Se il livello segue, **l'intera catena e' corretta**: il campo `DOEN` in
uscita, il selettore `DOUT`, il pad e il registro `DIN`. E' il modo onesto di
validare una cosa dedotta dal driver invece che misurata, e l'unico possibile
senza apparecchiature.

Se invece non segue, il pin e' occupato da qualcosa o ha un carico: guidarlo
non fa danno, e si prova un altro pin.

## 7. Cosa manca

Il driver attuale copre l'uso base: uscita alta e bassa, ingresso, lettura,
self-test. **Non** copre:

- **interrupt sui pin**: il blocco ha registri dedicati (rilevamento di
  fronte o livello, maschere, stato) a partire da `0x0dc`;
- **funzioni alternative**: sono selezionate dallo stesso meccanismo
  `DOUT`/`GPI`, ma per usarle servono i valori corretti per ogni periferica;
- **pull-up e pull-down configurabili dai comandi**: i bit sono noti, manca la
  superficie di comando. Sono due bit in un registro gia' mappato.

## 8. Riferimenti

- **Linux**: `drivers/gpio/gpio-starfive-jh7110.c` — layout e semantica dei
  campi `DOEN`/`DOUT`/`DIN`;
- **Linux**: `drivers/pinctrl/starfive/pinctrl-starfive-jh7110.c` — mappa
  `SYS IOMUX`, offset dei registri e disposizione dei pad;
- **Linux**: `include/dt-bindings/pinctrl/starfive,jh7110-pinfunc.h` — i
  valori dei selettori `GPI`/`GPOUT`/`GPOEN`;
- **Milk-V**: documentazione ufficiale del connettore a 40 pin.
