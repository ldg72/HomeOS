# Exec64Core per Milk-V Mars

`Exec64Core` è il kernel/Core di Exec64 compilato per la Milk-V Mars (StarFive
JH7110, RISC-V64). È il file che viene avviato dal bootloader; **non contiene**
l'OS, la shell, i driver grafici o il filesystem.

## Provenienza del binario

| Voce | Valore |
|---|---|
| File | `Exec64Core` |
| Formato | immagine bare-metal RISC-V64 (flat, non ELF) |
| Architettura | RISC-V64, hart singolo |
| Sorgente | `Build/riscv64/milkv/Core/Exec64Core` di `EXEC64_multiarch` |
| Costruito | 2026-10-04 |
| SHA-256 | vedi `SHA256SUMS` |

## Ruolo nel boot (contratto split)

Il Core è **indipendente dall'OS**. All'avvio:

1. il bootloader carica il Core e, come *initrd*, un bundle OS (`HomeOS.img`);
2. il Core costruisce e valida un `BootInfo` e un `BootBundle` (vedi
   `../SDK/include/exec64/bootinfo.h` e `bootbundle.h`);
3. il Core carica il payload `Init` contenuto nel bundle e gli passa il
   controllo con `Exec64InitContext` in `a0` (vedi `exec64/init.h`);
4. da quel momento la sessione appartiene all'OS: il Core non conosce nomi di
   file, percorsi DOS o policy della shell.

Questo è esattamente il punto di aggancio di **HomeOS**: sostituire l'immagine OS
con un bundle che contiene il proprio `Init`.

Il **nome** del file (`Exec64OS.img`, `HomeOS.img`, `os.img`, ...) è una scelta
della configurazione di boot, non del Core: il Core riceve dal Device Tree solo
un intervallo di memoria (`linux,initrd-start/end`) e valida il contenuto
(`EX64BNDL`), mai il nome. Dettagli e schema completo in
`../SDK/docs/homeos/BOOT_CHAIN.md`.

## File richiesti sulla partizione di boot

Sulla partizione `MARS_BOOT` (FAT32) servono indicativamente:

```text
Exec64            il Core (questo binario, rinominato come da configurazione)
HomeOS.img        il bundle OS (Init + eventuale filesystem)
uEnv.txt
extlinux/extlinux.conf
```

`boot-config/` contiene esempi funzionanti di queste configurazioni. La direttiva
`initrd` di extlinux è ciò che pubblica l'immagine OS al Core; il nome indicato
lì è arbitrario. Vedi `boot-config/extlinux/extlinux.conf.homeos` per l'esempio
HomeOS con `initrd /HomeOS.img`.

## Nota sulla compatibilità

Il bundle deve dichiarare una ABI Init compatibile con il Core (major uguale,
minor non superiore a quella supportata). Un bundle incoerente viene rifiutato
dal Core in modo deterministico sulla seriale, senza avviare alcun OS.
