# Exec64 — Status USB Keyboard Mars

**Aggiornato**: 2026-07-12
**Ambito**: bring-up tastiera USB HID sulla Milk-V Mars, come filone separato dal framebuffer Mars e dallo stack grafico.

**Design multipiattaforma approvato**:
[../design/input_backend_dma_architecture.md](../design/input_backend_dma_architecture.md)

---

## 1. Obiettivo reale

L'obiettivo attuale non e:

- creare uno stack USB completo
- supportare mouse
- supportare storage USB
- gestire hub complessi
- integrare subito `input.device`

L'obiettivo attuale e molto piu stretto:

- verificare che il controller USB host della Mars sia visibile
- leggere i registri capability xHCI
- preparare il percorso verso una tastiera USB HID boot keyboard
- arrivare, in seguito, a usare la tastiera Amiga 1200 tramite adattatore Amikey USB come input della shell

---

## 2. Stato attuale

### Gia disponibile

- comando diagnostico `marskbd`
- subcommand:
  - `marskbd help`
  - `marskbd status`
  - `marskbd probe`
  - `marskbd stg`
  - `marskbd stgfull`
  - `marskbd usb`
  - `marskbd poweron`
  - `marskbd prepare`
  - `marskbd op`
  - `marskbd xhciinit`
  - `marskbd ports`
  - `marskbd portpower`
  - `marskbd portreset <1|2>`
  - `marskbd oc`
  - `marskbd ochigh`
  - `marskbd oclow`
  - `marskbd vbus`
  - `marskbd vbuson`
  - `marskbd phy`
  - `marskbd phyon`
  - `marskbd test`
  - alias brevi `h`, `st`, `p`, `d`, `s`, `sf`, `u`, `po`, `prep`, `op`, `xi`, `ports`,
    `pp`, `pr`, `oc`, `ochigh`, `oclow`, `vb`, `vbon`, `phy`, `phyon`, `t`

### Stato pratico

- `marskbd` e un comando Mars-only, separato da `input.device`
- il comando usa MMIO supervisor-mediate e whitelistate
- il comando `marskbd poweron` scrive solo registri STG clock/reset/syscon
- il comando `marskbd portpower` scrive solo il bit `PORT_POWER` sui due
  `PORTSC` xHCI osservati
- il comando `marskbd vbus` legge solo i registri pinctrl del pin SYS GPIO25
- il comando `marskbd vbuson` configura solo SYS GPIO25 come nel DTS Milk-V Mars:
  `GPOUT_SYS_USB_DRIVE_VBUS`, `GPOEN_ENABLE`, `GPI_NONE`
- il comando `marskbd phy` legge solo i registri PHY USB2/JH7110 minimi
- il comando `marskbd phyon` applica solo i bit usati dal driver Linux
  `phy-jh7110-usb`
- `marskbd probe` legge solo i primi 32 byte capability xHCI e solo se la
  guardia clock/reset STG risulta pronta
- `marskbd op` legge solo `USBCMD`, `USBSTS`, `PAGESIZE`, usando
  `CAPLENGTH=0x80` come base degli operational register
- `marskbd xhciinit` esegue un solo test DMA No-op in polling e arresta/resetta
  xHC prima di rilasciare il buffer
- `marskbd ports` legge solo `PORTSC` delle due porte xHCI dichiarate da
  `HCSPARAMS1`
- non c'e ancora enumerazione USB
- non c'e ancora polling HID
- non c'e ancora integrazione con la shell
- primo test reale: `marskbd status` funziona
- primo test reale: `marskbd probe` si e bloccato sulla prima read xHCI
- conseguenza: le letture dirette del blocco USB controller/xHCI sono disabilitate finche non e nota la sequenza clock/reset corretta
- primo test reale: `marskbd stg` funziona e legge STG CRG/SYSCON senza freeze
- primo test reale: `marskbd stgfull` funziona e legge `STG_CRG 0x00..0x7C`
  e `STG_SYSCON 0x00..0x3C` senza freeze
- valori osservati nel primo dump breve:
  - `STG_CRG_10 = 0x00000002`
  - `STG_CRG_14 = 0x00000004`
  - `STG_CRG_1C = 0x00000002`
  - `STG_SYSCON_04 = 0x00FA2FFF`
  - `STG_SYSCON_08 = 0x00041018`
  - `STG_SYSCON_0C = 0x00000402`
- interpretazione del dump esteso, incrociata con Linux JH7110:
  - `STG_CRG + 4 * clock_id` e il formato dei clock StarFive confermano che
    `usb0_apb`, `usb0_utmi_apb`, `usb0_axi` e `usb0_app_125` non hanno ancora
    il bit enable attivo
  - `usb0_lpm`, `usb0_stb` e `usb0_refclk` hanno divisori impostati, ma i clock
    gated non risultano ancora abilitati
  - `STG_CRG+0x74` e `STG_CRG+0x78` sono rispettivamente assert/status reset
    del dominio STG
  - i reset USB `usb0_axi`, `usb0_apb`, `usb0_utmi_apb`, `usb0_pwrup`
    risultano ancora asserted
  - `STG_SYSCON+0x04 = 0x00FA2FFF` indica gia una configurazione compatibile
    con host strap/suspend/refclk/pll, quindi il blocco critico successivo e
    clock/reset, non la modalita host di base
- primo test reale: `marskbd poweron` funziona
  - tutti i clock USB STG risultano `enabled=yes`
  - i reset `usb0_axi`, `usb0_apb`, `usb0_utmi_apb`, `usb0_pwrup` risultano
    `deasserted`
  - la Mars non si blocca durante il power-on STG
- primo test reale post-poweron: `marskbd probe` legge i capability register xHCI
  senza freeze
  - `CAPLENGTH/HCIVERSION = 0x01000080`
  - `HCSPARAMS1 = 0x02000820`
  - `HCSPARAMS2 = 0xFC000054`
  - `HCSPARAMS3 = 0x00040001`
  - `HCCPARAMS1 = 0x200073C8`
  - `DBOFF = 0x00003000`
  - `RTSOFF = 0x00002000`
  - `HCCPARAMS2 = 0x00000008`
  - decode operativo iniziale: xHCI 1.0, 32 slot, 8 interrupter, 2 porte
- primo test reale post-poweron: `marskbd op` legge gli operational register
  iniziali senza freeze
  - `USBCMD = 0x00000000`
  - `USBSTS = 0x00000011`
  - `PAGESIZE = 0x00000001`
  - decode: controller `halted=yes`, `reset=no`, `cnr=no`, `hse=no`,
    `port_change=yes`, page size 4K supportata
  - interpretazione: il controller e leggibile e non segnala controller-not-ready;
    il bit `port_change` suggerisce che il prossimo probe utile e lo stato porta
- primo test reale post-operational: `marskbd ports` legge i due `PORTSC` senza
  freeze; gli ultimi test sono stati eseguiti con tutte le porte fisiche vuote
  - `PORT1SC = 0x00100088`
  - `PORT2SC = 0x00100088`
  - decode: `connect=no`, `enabled=no`, `power=no`, `over_current=yes`,
    `link=Disabled(4)`, `speed=undefined(0)`, `occ=yes`
  - interpretazione: il controller espone due porte logiche xHCI; entrambe sono
    leggibili ma non alimentate a livello `PORT_POWER`; poiche `OCA/OCC` sono
    presenti anche senza carico, non si tratta di una sovracorrente causata da
    una periferica collegata
- primo test reale post-`portpower`: la write `PORT_POWER` viene accettata ma
  il readback resta invariato
  - prima: `PORT1SC = 0x00100088`, `PORT2SC = 0x00100088`
  - dopo il primo `marskbd portpower` con valore nudo `0x00000200`:
    `after=0x00100088`, `power=no`, `over_current=yes`
  - interpretazione: il problema non e solo il bit xHCI `PORT_POWER`; manca la
    configurazione board-specific del pin VBUS o resta attivo un segnale
    over-current/VBUS esterno
- verifica da DTS Linux Milk-V Mars:
  - `&usb0 { dr_mode = "host"; pinctrl-0 = <&usb0_pins>; }`
  - `usb0_pins` usa `sysgpio` pin 25 con `GPOUT_SYS_USB_DRIVE_VBUS`,
    `GPOEN_ENABLE`, `GPI_NONE`
- primo test reale `marskbd vbus`:
  - `SYS_GPIO25_DOEN = 0x00000000`, field `0`, atteso `0`
  - `SYS_GPIO25_DOUT = 0x00000700`, field `7`, atteso `7`
  - `SYS_GPIO25_FUNC = 0x00000000`, field `0`, atteso `0`
  - `SYS_GPIO25_PADCFG = 0x00000001`, solo input-enable ancora attivo
- primo test reale `marskbd vbuson`:
  - `PADCFG` passa da `0x00000001` a `0x00000000`
  - il pinctrl VBUS risulta conforme al DTS
  - `marskbd portpower` resta pero invariato: `PORTSC = 0x00100088`,
    `power=no`, `over_current=yes`
  - interpretazione: VBUS pinmux non era il blocco principale; il prossimo
    candidato e il PHY USB2 JH7110
- verifica da driver Linux `phy-jh7110-usb`:
  - `USB_PHY+0x00` deve avere `RX_NORMAL_PWR` (`BIT(1)`)
  - `USB_PHY+0x04` deve avere `LS_KEEPALIVE` (`BIT(4)`) in host mode
  - `SYS_SYSCON+0x18` deve avere `USB_PDRSTN_SPLIT` (`BIT(17)`)
  - `SYSCLK_USB_125M` deve essere abilitato

### Modifica core correlata

Per permettere il probe da un comando `U-Mode`, il dispatcher syscall RISC-V
accetta temporaneamente letture `SYS_MMIO_READ32` read-only sulle finestre STG/JH7110:

- `0x10230000` STG CRG
- `0x10240000` STG SYSCON

Questa modifica scaturisce dalla lavorazione di `ROADMAP_USB_KEYBOARD_MARS.md`.
E un ponte diagnostico temporaneo, non l'API definitiva per i driver USB.

Per il passo `marskbd poweron`, il dispatcher accetta temporaneamente anche write
strettamente whitelistate su:

- `0x10240004` STG USB mode/syscon
- `0x10230004`, `0x10230008`, `0x1023000C`, `0x10230010`,
  `0x10230014`, `0x10230018` clock USB STG
- `0x10230074` reset assert STG

Queste write sono state approvate come modifica core temporanea per questo
filone e vanno rimosse/sostituite quando esistera una API driver reale.

Per il passo successivo, il dispatcher accetta temporaneamente letture
`SYS_MMIO_READ32` solo sui capability register xHCI:

- `0x10110000..0x1011001F`

Questa finestra e intenzionalmente minima. `marskbd probe` la usa solo dopo aver
verificato che i clock USB STG siano abilitati e che i reset USB STG risultino
deasserted.

Per il passo operational iniziale, il dispatcher accetta temporaneamente letture
`SYS_MMIO_READ32` solo sui primi operational register xHCI osservati via
`CAPLENGTH=0x80`:

- `0x10110080` `USBCMD`
- `0x10110084` `USBSTS`
- `0x10110088` `PAGESIZE`

Questa finestra e intenzionalmente limitata a tre registri read-only. Non include
ancora reset/run del controller, doorbell, runtime registers, porte o strutture
ring.

Per il passo porta iniziale, il dispatcher accetta temporaneamente letture
`SYS_MMIO_READ32` solo sui `PORTSC` delle due porte dichiarate da `HCSPARAMS1`:

- `0x10110480` `PORT1SC`
- `0x10110490` `PORT2SC`

Questa finestra e read-only e non scrive i change bit (`CSC`, `PEC`, `WRC`,
`OCC`, `RC`, `PLC`, `CEC`). Non include ancora `PORTPMSC`, `PORTLI`,
`PORTHLPMC`, reset porta o power control.

Per il passo power porta iniziale, il dispatcher accetta temporaneamente write
`SYS_MMIO_WRITE32` solo sugli stessi due `PORTSC`:

- `0x10110480` `PORT1SC`
- `0x10110490` `PORT2SC`

Il comando `marskbd portpower` scrive solo `PORT_POWER` (`0x00000200`) e non
scrive reset porta, port link state, doorbell, runtime register, run/reset del
controller o strutture ring. Questa write e una micro-estensione temporanea e
va sostituita da una API driver quando lo stack USB Mars uscira dalla fase
diagnostica.

Per il passo VBUS board-specific, il dispatcher accetta temporaneamente
`SYS_MMIO_READ32` e `SYS_MMIO_WRITE32` solo sui registri pinctrl SYS GPIO25
necessari al pin `GPOUT_SYS_USB_DRIVE_VBUS` indicato dal DTS Milk-V Mars:

- `0x13040018` `SYS_GPIO25_DOEN`
- `0x13040058` `SYS_GPIO25_DOUT`
- `0x13040184` `SYS_GPIO25_PADCFG`
- `0x130402A0` `SYS_GPIO25_FUNC`

Il comando `marskbd vbuson` applica solo:

- `DOUT[GPIO25] = 7` (`GPOUT_SYS_USB_DRIVE_VBUS`)
- `DOEN[GPIO25] = 0` (`GPOEN_ENABLE`)
- `FUNC[GPIO25] = 0`
- clear di bias/input/schmitt/slew nel `PADCFG` del pin 25

Non configura altri GPIO, non tocca PMIC/I2C e non avvia ancora enumerazione USB.

Per il passo PHY USB2 JH7110, il dispatcher accetta temporaneamente
`SYS_MMIO_READ32` e `SYS_MMIO_WRITE32` solo sui registri minimi usati dal driver
Linux `phy-jh7110-usb`:

- `0x1302017C` `SYSCLK_USB_125M`
- `0x10200000` `USB_PHY_CLK_MODE`
- `0x10200004` `USB_PHY_LS_KEEPALIVE`
- `0x13030018` `SYS_USB_SPLIT`

Il comando `marskbd phyon` applica solo:

- `SYSCLK_USB_125M |= 0x80000000`
- `USB_PHY_CLK_MODE |= BIT(1)`
- `USB_PHY_LS_KEEPALIVE |= BIT(4)`
- `SYS_USB_SPLIT |= BIT(17)`

Non tocca ancora Cadence xECP, command ring, runtime register, doorbell o reset
porta.

Per diagnosticare `OCA/OCC` presenti su entrambe le porte anche a porte fisiche
vuote, il dispatcher accetta una nona micro-estensione temporanea esclusivamente
read-only, scaturita da `ROADMAP_USB_KEYBOARD_MARS.md`:

- `0x13040080` selettori `SYS_GPI` 0..3; il byte 2 instrada
  `GPI_SYS_USB_OVERCURRENT`
- `0x13040118` `SYS_GPIOIN0`
- `0x1304011C` `SYS_GPIOIN1`

Il comando `marskbd oc` decodifica il selettore come costante low/high oppure
`GPIO0..63`, mostra il livello grezzo e ristampa i due `PORTSC`. Non modifica
il routing, non cancella `OCC` e non forza alimentazione o reset.

Primo test reale `marskbd oc`, eseguito a porte fisiche vuote:

- `SYS_GPI_ROUTE_0_3 = 0x00000000`
- selettore `GPI_SYS_USB_OVERCURRENT = 0`
- sorgente decodificata: `constant-low`, livello `low`
- `SYS_GPIOIN0 = 0x00000000`
- `SYS_GPIOIN1 = 0x00000000`
- nello stesso campione `PORT1SC = PORT2SC = 0x00100088`, con `OCA/OCC`
  entrambi attivi

La correlazione dimostra che il falso over-current non proviene da una
periferica o da un GPIO fisico: l'ingresso interno USB over-current e lasciato
sulla costante bassa. Il prossimo test controllato dovra portare esclusivamente
quel selettore alla costante alta, preservando gli altri tre byte del registro,
e verificare prima `OCA`, poi `PORT_POWER`. Non va ancora cancellato `OCC`.

La write masked e stata approvata e implementata con due comandi separati:

- `marskbd ochigh` seleziona `constant-high`
- `marskbd oclow` ripristina `constant-low`

Entrambi modificano solo i bit `22:16` di `0x13040080`, preservano i selettori
degli altri ingressi, effettuano readback tramite `marskbd oc` e non cancellano
`OCC`. Il dispatcher aggiunge quindi una decima micro-estensione temporanea:
`SYS_MMIO_WRITE32` accetta esclusivamente l'indirizzo `0x13040080`.

Primo test reale `marskbd ochigh`, a porte fisiche vuote:

- write masked: `0x00000000 -> 0x00010000`
- readback: selettore `1`, `constant-high`, livello `high`
- `PORT1SC = PORT2SC = 0x00100080`
- `OCA` passa immediatamente da attivo a inattivo su entrambe le porte
- `OCC` resta attivo come change bit storico, come previsto
- `PORT_POWER` resta ancora inattivo perche questo test non lo scrive

Questo risultato conferma la causa: il selettore `constant-low`, e non un
carico USB reale, generava il falso over-current. Il prossimo test deve restare
a porte vuote e limitarsi a `marskbd portpower`, seguito da `marskbd ports`.

Primo test reale `marskbd portpower` dopo `marskbd ochigh`, ancora a porte
fisiche vuote:

- `PORT1SC = PORT2SC = 0x001002A0`
- `PORT_POWER=yes` resta stabile su entrambe le porte
- `OCA=no`
- link state da `Disabled(4)` a `RxDetect(5)`
- `connect=no`, coerente con le porte fisiche vuote
- `OCC=yes` resta soltanto come change bit storico

Il percorso minimo clock/reset, PHY, VBUS, routing over-current e alimentazione
porta e ora operativo. Il prossimo test puo collegare una tastiera a una porta
fisica per volta e usare il solo comando read-only `marskbd ports`, senza ancora
reset porta, controller run o enumerazione.

Primo rilevamento reale della tastiera USB, ottenuto sulla quarta porta fisica
provata (porta interna superiore):

- `PORT1SC = 0x00120AE1`
- `connect=yes`
- `power=yes`
- `over_current=no`
- `enabled=no`, perche non e ancora stato eseguito il reset USB della porta
- link `Polling(7)`
- speed `low(2)`, coerente con una tastiera HID low-speed
- `CSC=yes`, quindi il controller ha registrato il cambio di connessione
- `PORT2SC = 0x001002A0`, ancora vuota in `RxDetect(5)`

Questo conferma anche la mappatura iniziale: la porta fisica interna superiore
raggiunge la porta logica xHCI 1. Il prossimo passo non e ancora HID polling:
va prima introdotto un reset controllato della sola porta 1, preservando power
e change bit, e verificato se il controller richiede di essere avviato prima
che `PORT_ENABLED` possa diventare attivo.

Il comando `marskbd portreset <1|2>` e ora implementato senza modifiche ulteriori
al core: usa i `PORTSC` gia whitelistati, rifiuta porte disconnesse, non alimentate
o con OCA attivo, scrive solo `PORTSC_neutral | PORT_RESET`, attende fino a
100 ms e ristampa lo stato. Non cancella `CSC`, `OCC` o altri change bit.

Primo test reale `marskbd portreset 1` con tastiera collegata:

- write: `PORT1SC 0x00120AE1 <- 0x00000AF1`
- reset completato autonomamente dopo 7 ms
- readback `PORT1SC = 0x00320A63`
- `connect=yes`, `enabled=yes`, `power=yes`
- `reset=no`, `over_current=no`
- speed `low(2)` confermata
- `RC=yes`, quindi il controller ha registrato il completamento del reset
- link `U3(3)` con xHC ancora halted (`USBCMD.Run=0`)

Il bring-up elettrico della porta e quindi completato. Lo stato U3 successivo al
reset indica che il prossimo confine non e piu PHY/VBUS/porta: serve il bootstrap
xHCI minimo, con memoria DMA valida, DCBAA, command ring, event ring, runtime
interrupter e controller run, inizialmente in polling.

Decisione prima del bootstrap DMA:

- congelare il checkpoint `rev 0.110`
- mantenere un solo kernel core per QEMU e Mars
- non incorporare xHCI nel core kernel
- separare `input.device` comune dal backend VirtIO senza regressioni QEMU
- scegliere il backend staticamente in fase di build, senza vtable o registry:
  `input_core + virtinput` su QEMU, `input_core + input_mars` sulla Mars
- definire un contratto DMA/cache condiviso da VirtIO e xHCI
- continuare la validazione xHCI in `marskbd` fino a report HID stabili
- trasformare solo dopo il codice validato in backend USB HID di `input.device`

La separazione statica e ora implementata senza modifiche al kernel core. Le due
varianti di `input.device` compilano e il cambio `BOARD` rilinka sempre il modulo;
il backend Mars resta intenzionalmente inerte. Una build QEMU pulita arriva alla
shell. Anche la release Mars e stata validata su hardware reale: boot regolare,
shell attiva e input dal Mac via seriale, senza interferenze dal backend passivo.

La validazione QEMU del backend reale e stata completata con il nuovo comando
U-Mode `inputprobe`, che usa esclusivamente `OpenDevice()`/`CloseDevice()` e non
chiama direttamente codice supervisor. Con una sola `virtio-keyboard-device`
MMIO, `input.device` avvia il proprio task e riceve un evento tastiera iniettato
dal monitor QEMU. Il caricamento di `graphics.library` e ora differito fino a un
evento puntatore, quindi il percorso tastiera non dipende dallo stack grafico.

La configurazione con mouse e tastiera VirtIO MMIO contemporaneamente ha invece
mostrato un fault durante l'inizializzazione del secondo device. `run.sh` mantiene
per ora la sola tastiera MMIO: il limite multi-device va corretto separatamente e
non blocca il bootstrap DMA xHCI dedicato alla tastiera Mars.

La separazione statica resta congelata nel checkpoint `rev 0.111`; la successiva
validazione reale della tastiera VirtIO QEMU e congelata nel checkpoint
`rev 0.112`.

Il bootstrap DMA minimo e ora implementato e validato sulla Mars:

- `SYS_DMA_SYNC=46` e append-only e accetta solo range privati posseduti dal task
- i range devono essere allineati a 64 byte e non superare 1 MiB
- Mars e QEMU usano fence di ordinamento sui rispettivi percorsi coerenti
- `marskbd xhciinit` alloca memoria `MEMF_SHARED | MEMF_ALIGNED` sotto 4 GiB
- il layout comprende DCBAA, array e buffer scratchpad, command ring, event ring
  ed ERST
- il test invia un solo No-op Command TRB e verifica un Command Completion Event
- non vengono ancora creati slot, endpoint, context device o descriptor USB
- il cleanup richiede xHC halted prima di liberare la memoria DMA

Il primo `marskbd xhciinit` reale ha allocato correttamente 144 KiB a
`0x60316000`, con 31 scratchpad e indirizzo interamente sotto 4 GiB. Il test si
e fermato prima della programmazione DMA con `Cause=2` sull'istruzione
`cbo.clean` (`EPC=0x4400A564`, `TVAL=0x0014A00F`), confermando che i core U74
non espongono Zicbom. Il backend e stato quindi corretto per usare il front-port
coerente JH7110 con soli fence; il No-op Command TRB resta da ritestare.

Il successivo test fence-only ha avviato xHC e suonato il doorbell 0 senza Guru,
ma non ha osservato il Command Completion Event entro 500 ms. Il controller si
e poi arrestato e resettato correttamente, quindi il timeout e rimasto isolato.
La helper dei registri pointer a 64 bit e stata corretta all'ordine xHCI
obbligatorio low-DWORD poi high-DWORD; al timeout vengono ora acquisiti stato,
pointer ring e primi TRB prima del cleanup.

Il test successivo ha ricevuto immediatamente un evento valido `type=34`,
`parameter=0x01000000`: e il Port Status Change Event della porta 1, non il
completion del No-op. Questo conferma che ERST ed event ring DMA funzionano. Il
consumer diagnostico ora mantiene indice e cycle state, registra e consuma gli
eventi porta, aggiorna `ERDP|EHB` e continua ad attendere `type=33`.

Per ridurre gli interventi manuali e stato aggiunto `marskbd prepare` (`prep`):
esegue in sequenza poweron, VBUS, PHY, routing over-current, port power, stato
porte e reset della porta 1. Verifica i confini critici e si ferma se la tastiera
non e connessa o la porta non raggiunge `connect+enabled+power` senza OCA.
`xhciinit` resta volutamente separato come confine DMA esplicito.

Il primo round-trip xHCI DMA e riuscito completamente su hardware reale:

- il consumer ha ricevuto e consumato il Port Status Change Event `type=34`
  della porta 1
- il No-op ha prodotto il Command Completion Event `type=33`, `code=1`
- il command pointer restituito e `0x60337000`, uguale al command ring allocato
- `marskbd` ha stampato `PASS: xHCI DMA No-op completed`
- xHC si e arrestato e resettato prima del rilascio del buffer

Sono quindi validati DCBAA, scratchpad, command ring, event ring, ERST, doorbell
0, polling, coerenza DMA e cleanup. Il prossimo confine e `Enable Slot`; EP0,
descriptor USB e HID restano fuori da questo checkpoint.

Residuo separato: durante `FreeMem()` compare ancora
`[STALE-SHARED] ... found=0`. Non ha impedito completion, halt, reset o ritorno
alla shell; va verificato come bookkeeping shared-memory senza bloccare il
proseguimento xHCI.

La whitelist temporanea aggiunge soltanto:

- `USBCMD` `0x10110080`
- `CRCR` `0x10110098..0x1011009F`
- `DCBAAP` e `CONFIG` `0x101100B0..0x101100BB`
- `ERSTSZ`, `ERSTBA`, `ERDP` `0x10112028..0x1011203F`
- doorbell 0 `0x10113000`

Questa modifica core e stata discussa e approvata come passo derivato da
`ROADMAP_USB_KEYBOARD_MARS.md`; non costituisce ancora l'API DMA definitiva dei
driver dual-mode.

Nota xHCI: dopo il primo test `phyon`, `PORT_POWER` continua a non restare alto.
Il confronto con Linux `xhci_set_port_power()` ha mostrato che il valore corretto
da scrivere non e il bit nudo `0x00000200`, ma:

```text
portsc_neutral = PORTSC & (RO_BITS | RWS_BITS)
write_value    = portsc_neutral | PORT_POWER
```

Per `PORTSC=0x00100088`, questo significa scrivere `0x00000288`.
`marskbd portpower` e stato aggiornato a usare questa forma neutral-write senza
aggiungere nuovi registri e senza clear dei change bit.

---

## 3. Decisione architetturale corrente

Per il bring-up tastiera Mars non si parte da:

- `usb.device`
- `input.device` definitivo
- mouse
- GUI
- console framebuffer

Si parte invece da:

- un comando diagnostico dedicato
- nome operativo: `marskbd`
- probe read-only del controller

---

## 4. Prossimo passo

Il prossimo test su hardware reale e:

```text
SYS:> marskbd status
SYS:> marskbd probe
SYS:> marskbd stg
SYS:> marskbd stgfull
SYS:> marskbd usb
SYS:> marskbd poweron
SYS:> marskbd probe
SYS:> marskbd op
SYS:> marskbd ports
SYS:> marskbd portpower
SYS:> marskbd ports
SYS:> marskbd vbus
SYS:> marskbd vbuson
SYS:> marskbd phy
SYS:> marskbd phyon
SYS:> marskbd portpower
SYS:> marskbd ports
SYS:> marskbd oc
SYS:> marskbd ochigh
SYS:> marskbd portpower
SYS:> marskbd ports
SYS:> marskbd portreset 1
SYS:> marskbd xhciinit
```

Questa sequenza e stata validata fino a `marskbd ports` dopo `phyon`, `vbuson`
e `portpower`, incluso il probe `marskbd oc`. Il falso `OCA` e stato ricondotto
al selettore `GPI_SYS_USB_OVERCURRENT=0`, cioe `constant-low`. `marskbd ochigh`
e stato validato: il readback mostra `constant-high` e `OCA` passa a zero su
entrambe le porte. Anche `marskbd portpower` e ora validato: entrambe le porte
mantengono `power=yes`, `OCA=no` e passano a `RxDetect`. Il prossimo test reale
ha rilevato la tastiera sulla porta fisica interna superiore: la porta logica 1
mostra `connect=yes`, `Polling`, speed `low` e `CSC=yes`. Il prossimo passo e il
test reale `marskbd portreset 1`, che e riuscito in 7 ms e ha portato la porta a
`enabled=yes`, speed low e `RC=yes`. Il link resta U3 perche xHC e ancora halted.
Il bootstrap xHCI DMA in polling e ora implementato come `marskbd xhciinit`; il
prossimo passo e provarlo su hardware reale e verificare un completion event di
tipo 33 con completion code 1. `marskbd oclow` ripristina lo stato precedente.
Non e ancora enumerazione completa.

---

## 5. Sintesi

- il filone tastiera USB Mars e aperto
- il primo deliverable e osservabilita xHCI, non ancora input shell
- il mouse non e nel percorso critico
- l'obiettivo finale resta: shell testuale autonoma con framebuffer + tastiera USB
