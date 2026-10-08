# Input USB su Milk-V Mars — tastiera e mouse (note di bring-up)

**Stato**: catena completa funzionante su hardware reale, tastiera e mouse.
**Data**: 2026-10-06, aggiornato il 2026-10-08 con il mouse.
**Ambito**: OS bare-metal che gira direttamente sull'hardware, senza
U-Boot/Linux sotto. Scritto per essere riusabile da qualunque OS.

Questo documento descrive cosa serve per arrivare da "scheda accesa" a
"i tasti e i movimenti del mouse arrivano all'OS", con i valori misurati sulla
scheda e gli inciampi reali. Non è una traduzione di documentazione: è quello
che abbiamo dovuto scoprire per farlo funzionare.

Il nome del file è rimasto quello del primo bring-up, che era solo la tastiera.
I capitoli 1-5 valgono per entrambi i dispositivi: cambia solo l'interfaccia da
cercare. Le trappole della tastiera sono nel capitolo 6, quelle del mouse nel
capitolo 11.

---

## 1. Fatto fondamentale: quale porta

La Milk-V Mars espone **quattro porte USB fisiche**:

- **una nera** → USB 2.0, collegata all'xHCI che si pilota con queste note;
- **tre blu** → USB 3.0, **altro controller** con altra base MMIO.

Se collegate la tastiera a una porta blu, con questa procedura non la vedrete
mai: `CONNECT` resterà a zero su tutte le porte dell'xHCI. È l'errore che costa
più tempo, perché sembra un problema di alimentazione.

Il controller USB 2.0 dichiara **2 porte logiche** (`HCSPARAMS1`), 32 slot
e 31 scratchpad.

---

## 2. Mappa dei blocchi

| Blocco | Base | Uso |
|---|---|---|
| xHCI | `0x10110000` | controller host |
| USB PHY | `0x10200000` | PHY USB 2.0 |
| STG CRG | `0x10230000` | clock e reset del dominio USB |
| STG SYSCON | `0x10240000` | modalità host |
| SYS CRG | `0x13020000` | clock di sistema (`SYSCLK_USB_125M` a `+0x17C`) |
| SYS SYSCON | `0x13030000` | split USB (`+0x18`) |
| SYS GPIO | `0x13040000` | pinmux VBUS e routing over-current |

Derivati dall'xHCI (`CAPLENGTH = 0x80`, `DBOFF = 0x3000`, `RTSOFF = 0x2000`):

| Registro | Indirizzo |
|---|---|
| Operational base | `0x10110080` |
| `PORT1SC` / `PORT2SC` | `0x10110480` / `0x10110490` (stride `0x10`) |
| Runtime interrupter 0 | `0x10112020` (`IMAN`), `+0x08` `ERSTSZ`, `+0x10` `ERSTBA`, `+0x18` `ERDP` |
| Doorbell array | `0x10113000` |

---

## 3. Sequenza elettrica

Va eseguita **in quest'ordine**. Ogni passo corrisponde a un test fisico.

### 3.1 Modalità host (STG SYSCON + 0x04)

```
mask  (7<<16) | (1<<19) | (0xF<<20)
value (1<<17) | (1<<19) | (1<<20) | (1<<22) | (1<<23)
```

Cioè: strap host, suspend in modalità host, bypass, PLL abilitata, refclk.

### 3.2 Clock USB (STG CRG + id*4, id 1..6)

Scrivere `0x80000000` (bit di enable) in ognuno. Gli id sono:

| id | clock |
|---|---|
| 1 | `usb0_apb` |
| 2 | `usb0_utmi_apb` |
| 3 | `usb0_axi` |
| 4 | `usb0_lpm` |
| 5 | `usb0_stb` |
| 6 | `usb0_app_125` |

### 3.3 Reset USB (STG CRG + 0x74)

Azzera i bit **7, 8, 9, 10**. Lo stato è in **`+0x78`**, e i bit sono a **1
quando il reset è rilasciato** — è invertito rispetto a come ci si aspetta.

### 3.4 VBUS (SYS GPIO 25)

Secondo il device tree Milk-V Mars, `usb0_pins` usa `sysgpio` pin 25 con
`GPOUT_SYS_USB_DRIVE_VBUS`.

| Registro | Indirizzo | Valore |
|---|---|---|
| `DOUT` | `0x13040058` | campo pin 25 (bit 8-14) = **7** |
| `DOEN` | `0x13040018` | campo pin 25 (bit 8-13) = **0** |
| `FUNC` | `0x130402A0` | bit 15-16 = 0 |
| `PADCFG` | `0x13040184` | azzerare i bit 6,5,4,3,0 |

### 3.5 PHY USB 2.0

| Registro | Indirizzo | Bit |
|---|---|---|
| `SYSCLK_USB_125M` | `0x1302017C` | bit 31 (enable) |
| `USB_PHY_CLK_MODE` | `0x10200000` | bit 1 (`RX_NORMAL_PWR`) |
| `USB_PHY_LS_KEEPALIVE` | `0x10200004` | bit 4 (`LS_KEEPALIVE`) |
| `SYS_USB_SPLIT` | `0x13030018` | bit 17 (`USB_PDRSTN_SPLIT`) |

### 3.6 Over-current — **la trappola silenziosa**

`0x13040080` instrada le sorgenti di over-current interne. Il selettore per
`GPI_SYS_USB_OVERCURRENT` è il **GPI 2**, quindi il campo è nei **bit 22-16**
(`<< 8 * (2 % 4)`), maschera `0x7F << 16`.

Valori: **0 = costante bassa**, **1 = costante alta**.

Con il valore di reset (costante bassa) il controller segnala **`OCA` attivo su
entrambe le porte anche senza alcuna periferica collegata** — un falso
over-current. Sintomo: `PORTSC = 0x00100088`, `power=no`, `oca=si`.

Portando il selettore a **costante alta**, `OCA` passa a zero immediatamente.
Solo allora `PORT_POWER` resta alto. `OCC` resta a 1 come change bit storico:
è normale e non va cancellato.

### 3.7 Alimentazione e reset di porta

La scrittura va fatta in forma **neutral**: si preservano i bit di stato e i
bit read/write, e si aggiunge solo quello che interessa.

```
neutral = PORTSC & (CCS | OCA | SPEED | PLS | PP)
power:  neutral | PORT_POWER      (bit 9)
reset:  neutral | PORT_RESET      (bit 4)
```

Scrivere il bit nudo (`0x00000200`) **non funziona**: il readback resta
invariato. Il reset si considera finito quando `PORT_RESET` torna a 0; poi
`PORT_ENABLED` deve diventare 1.

**Valori osservati sulla scheda** (tastiera low-speed sulla porta 1):

| Momento | `PORTSC` | decode |
|---|---|---|
| porta vuota, alimentata | `0x001002A0` | `pls=5` (RxDetect) |
| tastiera collegata, prima del reset | `0x00120AE1` | `connect=1`, `pls=7` (Polling), `speed=2` (low) |
| dopo il reset, controller fermo | `0x00320A63` | `enabled=1`, `pls=3` (U3) |
| dopo il reset, controller attivo | `0x00220A03` | `enabled=1`, `pls=0` (U0) |

La differenza fra U3 e U0 dipende dallo stato del controller al momento del
reset. Entrambi sono valori validi.

---

## 4. xHCI: bootstrap minimo

1. **Halt** (`USBCMD = 0`) e attesa di `USBSTS.HCHalted`.
2. **Reset** (`USBCMD.RESET = 1`, bit 1) e attesa che il bit si azzeri e che
   `USBSTS.CNR` vada a 0.
3. **`CONFIG`** = numero di slot abilitati.
4. **`DCBAAP`** = indirizzo fisico della DCBAA (64 byte allineati).
5. **Event ring**: ERST con una voce `{indirizzo, numero TRB}`, poi `ERSTSZ = 1`,
   `ERSTBA`, e `ERDP` con il bit `EHB` (bit 3).
6. **`CRCR`** = indirizzo del command ring **| 1** (il bit 0 è il ciclo).
7. **Run** (`USBCMD.RUN = 1`, bit 0), attesa che `HCHalted` vada a 0.

Gli anelli sono array di TRB da 16 byte: `param` (8), `status` (4),
`control` (4). Un TRB di tipo **Link** chiude l'anello: va riscritto a ogni
giro con il ciclo corrente e `TC=1`, poi il ciclo si inverte.

I puntatori a 64 bit nei registri si scrivono **prima il dword basso, poi
l'alto** (prescrizione dell'xHCI).

---

## 5. Enumerazione e HID

Sequenza minima, dopo il bootstrap:

1. **Enable Slot** (TRB tipo 9) → il completion event porta lo slot ID nei bit
   24-31 del *control*.
2. **Address Device** (TRB tipo 11, `BSR=0`) con un input context contenente
   slot context + contesto EP0.
3. `GET_DESCRIPTOR` device (18 byte) → `idVendor`/`idProduct`.
4. `GET_DESCRIPTOR` config: prima 9 byte per `wTotalLength`, poi tutto.
   Si cercano l'interfaccia con classe 3 / sottoclasse 1 / protocollo 1
   (HID boot keyboard) e il suo endpoint interrupt IN.
5. `SET_CONFIGURATION`.
6. `HID SET_PROTOCOL` con valore 0 (boot) — così il report è fisso a 8 byte
   e non serve leggere il report descriptor.
7. `Configure Endpoint` (TRB tipo 12) per l'endpoint interrupt IN.
8. Un **Normal TRB** (tipo 1) da 8 byte su quell'endpoint, con `IOC`, e
   doorbell del device con il DCI dell'endpoint.

### Indirizzi degli endpoint (DCI)

`DCI = 2 * numero_endpoint + direzione`, con direzione 1 = IN.
Quindi EP0 = 1, EP1 OUT = 2, **EP1 IN = 3**.

### Report boot keyboard (8 byte)

| Byte | Contenuto |
|---|---|
| 0 | modificatori: `0x02`/`0x20` = shift sinistro/destro |
| 1 | riservato |
| 2-7 | fino a 6 keycode HID premuti contemporaneamente |

Per un uso da console servono solo i tasti **nuovi**: si confronta il report con
il precedente, altrimenti si genera autorepeat.

---

## 6. Le quattro trappole

Queste sono le cose che **non** si deducono dalla documentazione, e che ci
hanno fatto perdere più tempo. Se state scrivendo il vostro driver, leggete
questo capitolo prima degli altri.

### 6.1 L'ordine: controller, poi porta

`HCRST` (il reset del controller) **azzera lo stato delle porte**. Se resettate
la porta *prima* del controller, il reset viene cancellato e `Address Device`
fallisce anche se la porta sembrava a posto.

```
sbagliato:  reset porta  →  reset controller  →  slot
corretto:   reset controller  →  reset porta  →  slot
```

Sintomo: `Enable Slot` riesce (non dipende dalla porta), `Address Device`
fallisce. Ed è insidioso perché il `PORTSC` letto subito dopo il reset della
porta mostra `enabled=1`: sembra tutto a posto, ed è già stato cancellato.

### 6.2 `CErr = 3` nel contesto di EP0

Il campo `CErr` (Error Count) sta nei **bit 1-2** del dword 1 del contesto
endpoint. Per gli endpoint di controllo la specifica pretende il valore **3**.

Con `CErr = 0` il controller risponde con **Command Completion Code 11
(TRB Error)** al comando `Address Device`. Il resto del contesto può essere
perfetto: basta quel campo.

Contesto EP0 corretto per un low-speed:

```
dword 1 = (3 << 1) | (4 << 3) | (8 << 16)
            CErr     tipo        MaxPacketSize
                     (Control Bidirectional)
dword 2 = (indirizzo_anello & ~0xF) | 1      <- bit 0 = DCS, deve valere 1
dword 3 = indirizzo_anello >> 32
dword 4 = 8                                   <- Average TRB Length
```

### 6.3 Lo Slot ID va nel dword **3** del TRB di comando

Layout di un Command TRB:

| dword | contenuto |
|---|---|
| 0-1 | parametro (es. puntatore all'input context) |
| 2 | status — **per Address Device è 0** |
| 3 | bit 0 ciclo, bit 9 `BSR`, bit 10-15 tipo, **bit 24-31 slot ID** |

Mettere lo slot ID nel dword 2 **non genera un errore immediato**: il controller
legge il dword 3, ci trova 0, e 0 è lo slot riservato. Il risultato è ancora
**codice 11 (TRB Error)**.

L'insidia è che `Enable Slot` continua a funzionare — lì lo slot ID è zero
anche nel posto giusto. Il comando sbagliato non vi contraddice finché non
arriva il primo comando con uno slot reale.

### 6.4 Il campo `Interval` non è il `bInterval`

Nel contesto dell'endpoint il campo `Interval` (bit 16-23 del dword 0) **non
contiene il valore letto dal descrittore**: è l'esponente di una potenza di due
di microframe, e il periodo vale `2^(Interval-1) × 125 µs`.

Scrivere il `bInterval` grezzo di una tastiera (`10`) significa chiedere
un'interrogazione ogni **64 ms**. Sintomo: la tastiera *funziona*, ma
**digitando veloce o con due mani si perdono caratteri**, mentre con un dito
solo no — perché ogni pressione lenta dura più di un periodo di
campionamento, e le sovrapposizioni rapide no.

Per un `bInterval` di 10 il valore corretto è **3**, cioè circa 1 ms: si
prende l'esponente, `fls(bInterval) - 1`, non il valore.

Come riconoscerlo dai numeri: contando i report ricevuti. Con il valore
sbagliato se ne vedono circa **15 al secondo** anche durante una digitazione
fitta; con quello giusto il conteggio cresce con la digitazione.

---

## 7. DMA e coerenza della cache

Fatti verificati sulla JH7110:

- **I core U74 non espongono Zicbom**: `cbo.clean` va in trap
  (`MCAUSE` illegale). Non si può contare sulla manutenzione esplicita della
  cache.
- La coerenza per queste periferiche si ottiene con il **front-port coerente**
  della JH7110 e con le sole **barriere** (`fence`) prima e dopo ogni consegna
  al controller. Verificato con un round-trip reale di comando e completion.
- **La memoria DMA deve stare sotto i 4 GiB**: il controller indirizza a 32 bit.
- Gli indirizzi passati al controller devono essere **fisici**. Se il vostro
  OS ha un identity mapping per la RAM, il puntatore dell'allocatore va bene
  così com'è.

**Trappola negativa da conoscere**: su questa piattaforma esiste un *alias non
cachato* della RAM (`fisico + 0x400000000`), ma **non è una mappatura
generale**: nella configurazione di riferimento copre **solo gli 8 MB del
framebuffer**, pagina per pagina. Non si può usare per rendere coerenti i
buffer DMA.

---

## 8. Limiti attuali

Cosa questa implementazione **non** fa, per onestà:

- **niente hot-plug**: la porta si alimenta e si resetta una volta sola. Se la
  tastiera non è collegata all'avvio, serve un riavvio (o rieseguire l'intera
  sequenza). Non c'è un gestore del `Port Status Change Event` che rifaccia
  il bring-up;
- **niente hub**: solo dispositivo direttamente sulla porta del root hub;
- **solo USB 2.0**: le porte blu (USB 3.0) sono un altro controller;
- **keymap solo US**: niente accenti, niente layout nazionali;
- **niente mouse**: solo la tastiera in boot protocol.

---

## 9. Scheda di riferimento rapido

```
porta corretta ............. quella NERA (USB 2.0)
PORTCSC idle ............... 0x001002A0   pls=5
PORTSC con tastiera ........ 0x00120AE1   connect=1 pls=7 speed=2
PORTSC dopo reset .......... 0x00220A03   enabled=1 pls=0

over-current ............... 0x13040080   bit 22-16 = 1 (costante alta)
VBUS pin 25 ................ DOUT 0x13040058 = 7<<8
                            DOEN 0x13040018 = 0
PHY ........................ 0x10200000 bit1, 0x10200004 bit4
split USB .................. 0x13030018 bit17
SYSCLK_USB_125M ............ 0x1302017C bit31

xHCI op .................... 0x10110080
porta 1 / porta 2 .......... 0x10110480 / 0x10110490
doorbell 0 ................. 0x10113000

Command Completion ......... tipo 33, successo = codice 1, TRB Error = 11
Port Status Change ......... tipo 34
Transfer Event ............. tipo 32
```

## 10. Riferimenti esterni

- **xHCI Specification** (Intel) rev 1.1/1.2 — TRB, slot, contesti, anelli.
  È la fonte per la disposizione dei campi; senza di essa i tre bug del
  capitolo 6 sono invisibili.
- **USB 2.0** cap. 9 — descrittori e richieste standard.
- **USB HID 1.11** + HID Usage Tables — il boot report da 8 byte.
- **Linux**: `drivers/usb/host/xhci*.c` (`xhci-mem.c` per i contesti,
  `xhci-ring.c` per i TRB), `drivers/hid/usbhid/hid-core.c`.
  Utile come riferimento di confronto, non da copiare: il kernel è GPLv2.
- **Linux**: `phy-jh7110-usb`, i clock/reset StarFive, e il DTS Milk-V Mars
  per il pinmux VBUS.

I valori di registro di questo documento sono **misurati sulla scheda**, non
dedotti: dove un riferimento esterno e la scheda divergono, ha ragione la
scheda.

---

## 11. Il mouse: stessa catena, tre trappole in piu'

**Stato**: funzionante e verificato su hardware reale (2026-10-08).

Il mouse non ha richiesto nessuna modifica alla parte elettrica, all'xHCI o
all'enumerazione: **la catena dei capitoli 1-5 è la stessa**. Cambia
un'interfaccia da cercare, un formato di report e tre dettagli che con la
tastiera sola non si incontrano mai.

### 11.1 Quale interfaccia

La tastiera boot si dichiara classe 3, sottoclasse 1, **protocollo 1**. Il
mouse boot è la stessa classe e sottoclasse con **protocollo 2**. Il resto del
percorso — `SET_CONFIGURATION`, `SET_PROTOCOL(0)`, endpoint interrupt IN,
doorbell — è identico.

Se un dispositivo espone **due** interfacce — capita con i ricevitori
wireless, che sono tastiera e mouse insieme — conviene scegliere la
**tastiera**: è quella che serve per digitare, e il mouse si prende quando è
attaccato da solo, con una porta sola a disposizione.

### 11.2 Il report

Il mouse boot manda **tre** byte (qualcuno quattro, con la rotellina):

| byte | contenuto |
|---|---|
| 0 | pulsanti (bit 0 = sinistro) |
| 1 | spostamento X, **con segno** |
| 2 | spostamento Y, **con segno** |

Gli spostamenti sono **relativi**: si sommano alla posizione corrente, non la
sostituiscono. E in HID il positivo va verso il basso, quindi la Y si somma
direttamente.

### 11.3 Trappola: il pacchetto corto

**È il difetto che ci ha tenuto fermi una serata.** Un transfer event può
chiudersi in due modi: *Success* oppure ***Short Packet*** (codice 13), che
significa "il dispositivo ha mandato meno byte di quelli richiesti". In
**entrambi i casi i byte che sono arrivati sono validi**.

La tastiera manda 8 byte su un endpoint che ne accetta 8: un pacchetto corto
non capita **mai**, e trattare il codice 13 come errore non si nota. Il mouse
manda 4 byte su un endpoint che ne dichiara 7: **ogni report è un pacchetto
corto**, e classificarlo come errore butta via ogni movimento del mouse — in
silenzio, con la sola traccia di un contatore di errori che sale.

Da qui la regola: **entrambi i codici sono esito buono**, e la lunghezza
residua nel transfer event dice quanti byte sono realmente arrivati.

### 11.4 Trappola: la dimensione di EP0 dipende dalla velocità

Il contesto di EP0 va programmato con la dimensione massima del pacchetto di
controllo, che **dipende dalla velocità del dispositivo**: 8 byte per low e
full speed, **64 per high speed**. Un valore fisso a 8 va benissimo finché si
prova solo una tastiera low speed; un mouse high speed, con 8, non si
indirizza nemmeno.

### 11.5 Trappola: una dimensione dichiarata fuori specifica

La dimensione massima di un endpoint interrupt dev'essere una **potenza di
due**. Il nostro mouse ne dichiara **7**. Il valore va riportato a un valore
sensato (4, arrotondando per difetto) invece di passarlo al controller: il
kernel Linux fa lo stesso, e adesso si capisce perché.

### 11.6 La diagnostica che ha risolto

Tre stampe, aggiunte dopo aver perso una serata a indovinare, e valgono più di
qualunque ragionamento:

- **il descrittore di configurazione in esadecimale**, quando è piccolo: le
  interfacce, i protocolli e le dimensioni dei pacchetti sono lì dentro, e
  senza i byte veri si interpreta invece di leggere;
- **codice e byte ricevuti** dei primi report: distingue "non arrivano" da
  "arrivano storti";
- **il codice di errore** dei primi trasferimenti falliti: è quello che ci ha
  detto che il codice 13 non era un errore ma un esito.
