# HomeOS — Display HDMI e GPIO

Questa e la parte piu importante per la domotica: cosa il Core offre gia e cosa
HomeOS deve implementare.

## 1. Framebuffer HDMI

### Cosa fornisce il Core

Il Core mappa e **riserva** una regione framebuffer per la Mars e ne espone gli
indirizzi (`exec64/marsfb_memory.h`):

| Costante | Valore | Uso |
|---|---|---|
| `MARSFB_PHYS` | `0x70000000` | indirizzo fisico della regione |
| `MARSFB_UNCACHED` | `0x470000000` | alias non cachato (scanout) |
| `MARSFB_MAX_WIDTH` | 1920 | larghezza massima |
| `MARSFB_MAX_HEIGHT` | 1080 | altezza massima |
| `MARSFB_BYTES` | 1920*1080*4 | dimensione massima |

Importante: la mappatura **non va mai esposta a task utente** e **non si scrive
mai attraverso l'alias cachato**. Per il scanout si usa l'alias non cachato.

### Cosa fornisce l'SDK come ABI

- `exec64/marsfb.h`: ABI 2 di diagnostica/bring-up (`MarsFbReport`, comandi
  `MARSFB_STATUS/POWER/CLOCKS/PHY/FILL_*/LIVE_*/DEMO_*`).
- `devices/marsdisplay.h`: device `jh7110display.device` e comando
  `MARS_DISPLAY_CMD_DIAGNOSTIC`.
- `devices/display.h`: ABI generica display v1 (`DISPLAY_CMD_SETUP_SCANOUT`,
  `PRESENT`, `QUERY_CAPS`, ...) con formato `XRGB8888`.

### Cosa deve fare HomeOS

Il driver vero (controller display JH7110: reset/clock, VOUT, PHY TMDS, timing,
scanout) nella configurazione attuale e lato OS: per HomeOS va **scritto o
portato**. La sequenza e documentata e validata in:

- `docs/hardware/marsfb_v2.md`
- `docs/hardware/jh7110_display_framebuffer_bringup.md`
- `docs/hardware/STATUS_DISPLAY_MARS.md`, `ROADMAP_DISPLAY_MARS.md`

Approccio consigliato in HomeOS:

1. porteresti su le sequenze di power/clock/PHY dal bring-up documentato;
2. usi `MARSFB_UNCACHED` come base del framebuffer di scanout;
3. disegni il testo della shell direttamente in `XRGB8888`;
4. aggiungi un `PRESENT`/commit quando cambi i pixel.

Nota onesta: il bring-up e descritto come "scanout richiesto"; la conferma
visiva stabile su monitor e il pezzo da consolidare nel tuo driver.

## 2. GPIO (domotica)

### Cosa fornisce l'SDK

`interfaces/gpio.h` definisce una superficie **wiringPi-style**:

```c
struct GPIOInterface {
    struct Interface i;
    int  (*wiringPiSetup)(void);
    void (*pinMode)(int pin, int mode);
    void (*pullUpDnControl)(int pin, int pud);
    void (*digitalWrite)(int pin, int value);
    int  (*digitalRead)(int pin);
    void (*pwmWrite)(int pin, int value);
    ...
};
```

con costanti `INPUT/OUTPUT/PWM_OUTPUT`, `LOW/HIGH`, `PUD_OFF/PUD_DOWN/PUD_UP`.

### Stato reale

**Non esiste ancora un'implementazione hardware**: nel progetto di origine questa
libreria e uno stub che non tocca i registri. Per la domotica devi scrivere un
driver SYS_GPIO JH7110 in HomeOS.

Punti di partenza:

- mappa registri **SYS_GPIO** dal datasheet StarFive JH7110 (GPIO do function,
  input/output enable, output value, interrupt);
- pinout e basi: `docs/hardware/hardware_setup_qemu_milkv.md`;
- il timer di sistema e gia gestito dal Core: usalo per debounce/delay invece di
  busy-wait.

### Consiglio di design

Tieni il driver GPIO dentro HomeOS, non nel Core: il Core non deve conoscere la
domotica. Esponi un'interfaccia stabile (anche uguale a `GPIOInterface`) verso la
tua shell, cosi i futuri "moduli" li scrivi sopra il driver senza toccare il
boot.

## 3. Input (tastiera)

Per una shell sull'HDMI serve input. L'ABI device e in `devices/input.h`
(`INPUT_CMD_READ_EVENT`/`READ_EVENTS`, eventi `SYN/KEY/REL/ABS`). Lo stato della
tastiera USB Mars e in `docs/hardware/STATUS_USB_KEYBOARD_MARS.md`.

Se non vuoi dipendere dalla tastiera nella prima iterazione, la console seriale
(115200 8N1) e sempre disponibile e sufficiente per pilotare la shell:
renderizzi su HDMI, comandi via seriale.
