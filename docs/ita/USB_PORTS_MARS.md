# Le porte USB della Milk-V Mars — due controller, non uno

**Data**: 2026-10-06
**Stato**: ricostruito dal device tree **che la scheda carica davvero**
all'avvio, e verificato leggendo la configurazione di boot sulla microSD
(capitolo 5). Non e' una deduzione: e' il file che il firmware passa al Core.
**Ambito**: capire dove sono attaccate le quattro porte USB, e cosa serve per
usarle da un OS bare-metal.

---

## 1. Il fatto che cambia le cose

La Milk-V Mars espone **quattro porte**, ma non sono quattro porte dello
stesso controller:

| Porta | Colore | Controller |
|---|---|---|
| 1 | nera | controller USB del SoC (Cadence USB3) |
| 2, 3, 4 | blu | un chip USB 3.0 **dietro PCIe** |

Questo spiega un'osservazione che ci ha fatto perdere tempo all'inizio: con la
tastiera su una porta blu, `CONNECT` restava a zero su tutte le porte
dell'xHCI. Non era alimentazione: era un altro controller, che non stavamo
guardando.

---

## 2. Il controller del SoC (porta nera)

Dal device tree della scheda:

```text
/soc/usbdrd                     compatible = "starfive,jh7110-cdns3"
  /soc/usbdrd/usb@10100000      compatible = "cdns,usb3"
      reg-names = "otg", "xhci", "dev"
      interrupts = <0x64 0x6c 0x6e>   host, peripheral, otg
      clock-names = "125m", "app", "lpm", "stb", "apb", "axi", "utmi"
      reset-names = "pwrup", "apb", "axi", "utmi"
      maximum-speed = "super-speed"
      dr_mode = "host"
      starfive,usb2-only
```

Tre cose da notare.

**L'xHCI a `0x10110000` non e' un controller a se'.** E' la *finestra* `xhci`
del controller Cadence, che occupa tre finestre da 64 KiB:

| Finestra | Base | Uso |
|---|---|---|
| `otg` | `0x10100000` | registri OTG |
| `xhci` | `0x10110000` | host controller: e' quello che pilotiamo |
| `dev` | `0x10120000` | lato device |

**`starfive,usb2-only`**: il SoC e' configurato per una porta in USB 2.0.
E' la ragione per cui il controller dichiara due porte logiche e la scheda ne
collega una sola. Da qui non si ricavano altre porte.

**La sequenza elettrica che avevamo misurato a mano** (VBUS su SYS GPIO 25,
clock STG `usb0_*`, reset invertiti) e' esattamente quella del nodo `usbdrd`:
`clock-names` e `reset-names` corrispondono uno a uno. Il device tree e' quindi
la fonte con cui verificare il driver, non solo un documento.

---

## 3. Le tre porte blu stanno dietro PCIe

Il JH7110 ha **due controller PCIe** (PLDA XpressRICH3-AXI) e nella scheda
sono entrambi attivi:

| | `pcie@2B000000` | `pcie@2C000000` |
|---|---|---|
| bar | `0x2B000000` | `0x2C000000` |
| config space | `0x94000000` | `0x9C000000` |
| PHY | `pcie0-phyctrl@10210000` | `pcie1-phyctrl@10220000` |
| reset PERST | GPIO 26 (`0x1a`) | GPIO 28 (`0x1c`) |
| clock-names | `noc`, `tl`, `axi_mst0`, `apb` | idem |
| resets | 6: `rst_mst0`, `rst_slv0`, `rst_slv`, `rst_brg`, `rst_core`, `rst_apb` | idem |

Uno dei due va allo slot M.2, l'altro al chip USB 3.0 (un controller a 4
porte, di cui la scheda ne espone 3). Quale dei due sia quale **non e'
scritto nel device tree**: si scopre enumerando il bus PCI. Lo stesso schema
si trova sulla VisionFive 2, che monta lo stesso SoC.

**Cosa serve per usare le porte blu**, in ordine:

1. abilitare i clock e rilasciare i 6 reset del controller PCIe;
2. configurare la PHY (registri `phyctrl`) e la `stg_syscon`;
3. asserire e rilasciare PERST, attendere il link training;
4. enumerare il bus PCI dalla config space e assegnare le BAR;
5. pilotare l'xHCI del chip trovato: il driver xHCI che abbiamo gia' scritto
   e' in gran parte riusabile, perche' e' un xHCI standard.

I punti 1-4 sono il lavoro grosso: molto piu' di un driver periferico.

---

## 4. Conseguenza pratica: come si collega un mouse

**Strada corta — hub USB 2.0 sulla porta nera.** Serve aggiungere al nostro
xHCI il supporto agli hub: riconoscere la classe 9, indirizzare l'hub, seguire
le variazioni di stato delle sue porte e indirizzare i dispositivi a valle.
Tutto il resto (HID, keymap, ripetizione) e' gia' scritto e validato.

**Strada lunga — PCIe verso il chip USB 3.0.** Da' tre porte in piu' e la
SuperSpeed, ma richiede la sequenza del capitolo 3. Va messa in roadmap come
filone a se', non come passo verso il mouse.

---

## 5. Da dove vengono questi dati

L'analisi non viene da un documento di terzi: viene dal device tree della
scheda, letto con `dtc`. E il file e' **proprio quello che U-Boot carica**,
come si verifica sulla partizione di boot:

```text
# uEnv.txt
fdt_addr_r=0x48000000
ramdisk_addr_r=0x48100000
fdtfile=starfive/jh7110-visionfive-v2.dtb

# extlinux/extlinux.conf
    linux /Exec64Core
    initrd /HomeOS.img
    fdtdir /dtbs
```

Cosi' si chiude anche il cerchio con il log del Core, che stampa
`platform-data=0x0000000048000000`: e' l'indirizzo in `fdt_addr_r`, cioe' il
device tree. (Il file sulla partizione si chiama `jh7110-visionfive-v2.dtb`,
che trae in inganno: al suo interno `model = "Milk-V Mars"`.)

A runtime, HomeOS rilegge lo stesso albero con il comando `fdt` — l'indirizzo
glielo da' il Core in `SysBase->ex_PlatformData` — e questo resta il modo
migliore per controllare che scheda e documento dicano la stessa cosa:

**Verificato sulla scheda il 2026-10-06**: il comando `fdt` legge l'albero che
U-Boot ha passato al Core.

```text
fdt          riepilogo: dimensione, versione, model, memoria
fdt usb      nodo del controller e sue proprieta'
fdt pcie     i due controller PCIe, PHY, reset
fdt mmc      i due controller di storage
```

---

## 6. Trovato per strada: i due controller di storage

Lo stesso device tree descrive lo storage, ed e' informazione che serve subito
per il volume persistente:

| Nodo | Base | Configurazione | Cos'e' |
|---|---|---|---|
| `sdio0@16010000` | `0x16010000` | 8 bit, non-removable, hs200, `cap-mmc-hw-reset` | eMMC |
| `sdio1@16020000` | `0x16020000` | 4 bit, `cap-sd-highspeed`, `no-mmc`, `broken-cd` | microSD |

Dettagli utili: clock `biu`/`ciu` (indici 0x5b/0x5d per sdio0, 0x5c/0x5e per
sdio1), un reset per controller (0x40 e 0x41), una interrupt a testa
(0x4a e 0x4b). `broken-cd` significa che il card-detect non e' collegato:
**la presenza della scheda si scopre interrogando la scheda stessa**, non un
pin.
