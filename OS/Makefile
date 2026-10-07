# HomeOS — build dello scheletro Init (RISC-V64, Milk-V Mars)
#
# Produce:
#   Build/HomeOS.elf   il payload Init
#   Build/HomeOS.img   il bundle di boot da copiare sulla microSD
#
# Uso:
#   make            compila e impacchetta
#   make clean      rimuove Build/
#
# Il bundle include l'immagine SYS exFAT ($(SYS_IMG)): il Core la monta al boot
# e legge Prefs/default.prefs per scegliere il profilo di Init. Con
# system-profile=performance Init viene caricato in S-Mode (supervisor), che e'
# il profilo richiesto da HomeOS per pilotare framebuffer e MMIO del display.
# Rigenerare l'immagine: sh tools/mksys.sh OS/sys/HomeOS-SYS.exfat 2 performance

CROSS      ?= riscv64-elf-
CC         := $(CROSS)gcc

SDK_DIR    := ../SDK
TOOLS_DIR  := ../tools
OUT_DIR    := Build
ELF        := $(OUT_DIR)/HomeOS.elf
IMG        := $(OUT_DIR)/HomeOS.img
SYS_IMG    := sys/HomeOS-SYS.exfat
SRCS       := init.c console.c services.c boot.c display.c gfx.c font8x16.c \
              font_topaz.c fbcon.c splash.c fdt.c \
              usb/usb.c usb/usb_hw.c usb/xhci.c usb/hid.c \
              gpio/gpio.c theme.c \
              shell/shell.c $(wildcard shell/cmds/*.c)

# Gli stessi vincoli del Core: freestanding, nessuna libc, no linker relaxation
# (il Core applica le relocation a runtime), niente small-data (nessun GP).
# -g1 e non -g: con -Wl,-q ogni voce di debug genera una rilocazione, e i dati
# di debug completi gonfiavano l'ELF da 170 KB a 482 KB senza utilita' pratica.
CFLAGS := -std=gnu11 -ffreestanding -nostdlib -O2 -g1 -Wall -Wextra \
          -march=rv64imafd_zifencei -mabi=lp64d -mcmodel=medany \
          -mno-relax -msmall-data-limit=0 \
          -DARCH_RISCV64 -DCOMPILING_APP \
          -I$(SDK_DIR)/include -I$(SDK_DIR)/arch/riscv64/include

# Risoluzione di avvio. Default 640x480: e' la risoluzione della modalita'
# classica, che e' anche quella di avvio.
# Altre scelte: make BOOT_W=1280 BOOT_H=720   oppure   BOOT_W=1920 BOOT_H=1080
BOOT_W     ?= 640
BOOT_H     ?= 480
CFLAGS     += -DHOMEOS_BOOT_WIDTH=$(BOOT_W) -DHOMEOS_BOOT_HEIGHT=$(BOOT_H)

# Tema di avvio: 1 = modern, 2 = retro. Si parte in modalita' classica.
THEME      ?= 2
CFLAGS     += -DHOMEOS_DEFAULT_THEME=$(THEME)

LDFLAGS := -Wl,-q -Wl,--gc-sections -Wl,--no-dynamic-linker -Wl,-e,_start

.PHONY: all clean elf
all: $(IMG)

elf: $(ELF)

$(OUT_DIR):
	mkdir -p $@

$(ELF): $(SRCS) | $(OUT_DIR)
	$(CC) $(CFLAGS) $(LDFLAGS) $(SRCS) -o $@

$(IMG): $(ELF) $(SYS_IMG) $(TOOLS_DIR)/mkbundle.py
	python3 $(TOOLS_DIR)/mkbundle.py $(IMG) $(ELF) $(SYS_IMG)

clean:
	rm -rf $(OUT_DIR)
