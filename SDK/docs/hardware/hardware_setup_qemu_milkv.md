# Exec64 RISC-V Hardware Setup Guide

**Last Updated**: March 9, 2026
**Targets**: QEMU (VirtIO), Milk-V Mars (StarFive JH7110)

## 🖥️ Target 1: QEMU (VirtIO)

L'ambiente di sviluppo primario è QEMU in modalità `virt`.

### Requisiti
- `qemu-system-riscv64` (v8.0+)
- Immagine disco exFAT (`Release/RISCV64/disk.img`)

### Comandi di Avvio
- **Test Testuale**: `./run_riscv.sh`
- **Test GUI (Intuition)**: `./run_riscv_gui.sh`
    - Include l'automazione per il caricamento dei driver grafici (`loadgfx`).

---

## 🪐 Target 2: Milk-V Mars (Real Hardware)

La Milk-V Mars è il target bare-metal principale per testare la protezione hardware su silicio reale (StarFive JH7110).

### 💾 Preparazione MicroSD
La scheda deve essere formattata con una partizione **FAT32** per il boot e una partizione **exFAT** per i dati del sistema.

1.  **Kernel**: Copia `platforms/milkv-mars/Exec64` come file di boot.
2.  **Immagine Disco**: La partizione exFAT deve contenere le cartelle `C:`, `LIBS:`, `DEVS:` e i binari ELF compilati per RISC-V.

### 🔌 Debugging Seriale (GPIO)
Per interagire con la Shell è necessario un adattatore **USB-to-TTL (3.3V)**.

**Schema Pinout (Milk-V Mars)**:
| Segnale | Pin Unità Mars | Nota |
| :--- | :--- | :--- |
| **GND** | Pin 6 | Terra |
| **RX** | Pin 8 (TXD) | Ricezione dati dal Mac |
| **TX** | Pin 10 (RXD) | Trasmissione dati al Mac |

**Connessione da macOS**:
```bash
# Identifica il device
ls /dev/tty.usbserial*

# Connettiti (Baudrate 115200)
screen /dev/tty.usbserial-XXXX 115200
```

---

## 🔎 Boot Troubleshooting

1.  **Hang all'avvio**: Verifica la frequenza del timer. Su RISC-V e spesso `10MHz` o `12.5MHz`, configurabile in `core/arch/riscv64/boards/milkv/platform.h`.
2.  **Instruction Page Fault**: Solitamente causato dall'esecuzione di codice non mappato con il bit `PTE_U` se il task è in U-Mode.
3.  **No Disk Found**: Assicurati che il driver VirtIO o SD-MMC sia inizializzato correttamente per la piattaforma specifica.
