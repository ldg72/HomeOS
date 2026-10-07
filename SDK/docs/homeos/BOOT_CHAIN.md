# HomeOS — Catena di boot (ufficiale)

Questo documento fissa in modo esplicito come la Milk-V Mars avvia il Core e
come il Core arriva a eseguire HomeOS. E il riferimento per capire **chi carica
cosa** e **da dove viene il nome del file**.

## 1. Schema

```text
U-Boot / extlinux  (partizione MARS_BOOT, FAT32)
   ├─ linux  /Exec64          carica il Core in RAM
   └─ initrd /HomeOS.img      carica il bundle OS come initrd
            │
            ▼
Exec64Core  (gia' in esecuzione, S-Mode)
   - legge dal Device Tree il range initrd: linux,initrd-start / linux,initrd-end
   - valida il contenuto: magic EX64BNDL, header BootBundle, ABI Init
   - estrae il payload Init (ELF64) e lo esegue
            │
            ▼
Init  (primo ingresso di HomeOS, in a0/ha il puntatore a Exec64InitContext)
   - e' l'OS: inizializza driver, servizi, shell
   - carica il resto del proprio sistema
```

## 2. Chi fa cosa

| Passo | Chi | Cosa decide |
|---|---|---|
| 1 | U-Boot/extlinux | **quali file** caricare e a quale indirizzo (nomi inclusi) |
| 2 | Bootstrap di piattaforma | pubblica il range initrd al Core via `BootInfo` |
| 3 | Exec64Core | valida `BootInfo` + bundle, carica **Init** |
| 4 | Init (HomeOS) | tutto il resto: servizi, driver, shell, policy di riavvio |

## 3. Il nome del file e libero (punto chiave)

Il nome `*.img` **non e nel Core**: il binario non contiene alcuna stringa di
nome file OS. Il Core:

- riceve un **intervallo di memoria** dal Device Tree (`linux,initrd-start` /
  `linux,initrd-end`), non un nome;
- valida il **contenuto** (magic `EX64BNDL`, header, ABI), non il nome.

Conseguenze pratiche:

- puoi chiamare il bundle `HomeOS.img`, `os.img`, `pippo.img`: e' solo la riga
  `initrd` della configurazione di boot a deciderlo;
- cambiare nome **non** richiede di ricompilare il Core: e' una modifica di una
  riga in `extlinux.conf` (piu' il nome del file sulla microSD);
- vale anche per il Core: la label `linux /Exec64` puo' puntare a qualunque nome
  tu abbia dato al binario del Core.

Esempio di cambio nome:

```diff
 label homeos
-    initrd /Exec64OS.img
+    initrd /HomeOS.img
```

## 4. Init e' separato dall'OS? Si, gia' adesso

Il file che il bootloader passa (`*img`) e' un **bundle**, non l'OS intero. Il
Core ne estrae **Init**, che e' l'ingresso dell'OS e ne possiede tutto il resto:

```text
bundle (Init + eventuale filesystem)
   -> Core carica Init
        -> Init avvia l'OS (servizi, driver, shell)
```

Quindi il modello **Kernel -> Init -> OS** e' quello attuale; non serve
introdurre un "Init separato" perche' esiste gia'. Cio' che il Core non fa e'
conoscere i nomi dei file o il filesystem dell'OS.

## 5. Convenzione adottata in HomeOS

| Artefatto | Nome |
|---|---|
| Core | `Exec64` (o `Exec64Core`, come preferisci) |
| Bundle OS | `HomeOS.img` |

Nel progetto di origine il bundle era pubblicato anche come `Exec64OS.img`
(alias di compatibilita'): e' solo un nome, non un requisito del Core. Per
HomeOS si usa `HomeOS.img`; gli esempi in `CORE/boot-config/` mostrano la riga
`initrd /HomeOS.img`.

## 6. Cosa il Core pretende davvero (per non sbagliare)

1. Il bundle inizia con i byte `45 58 36 34 42 4E 44 4C` (`EX64BNDL`).
2. `bb_HeaderSize == 80`, `bb_InitOffset` allineato a 4096.
3. Il payload Init e' un ELF64 RISC-V con ABI Init compatibile.
4. Il range initrd e' dentro la RAM e non sovrapposto al DTB/Core.

Dettaglio dei campi in `BOOT_IMAGE_FORMAT.md`. Se qualcosa non torna, il Core
lo dice sulla seriale (115200 8N1) e non avvia alcun OS.
