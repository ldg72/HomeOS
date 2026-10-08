/*
 * Anteprima delle schermate di HomeOS sul computer di sviluppo.
 *
 * Disegna con lo stesso codice che gira sulla scheda — splash.c, fbcon.c,
 * theme.c, gfx.c — e salva un BMP. Serve a guardare e correggere colori,
 * impaginazione e temi senza scrivere la microSD, che e' il giro che finora
 * costava di piu'.
 *
 * Compilazione:
 *   cc -O2 -I OS -o /tmp/screen_preview tools/screen_preview.c \
 *      OS/splash.c OS/fbcon.c OS/gfx.c OS/font8x16.c OS/font_topaz.c OS/theme.c
 *
 * Uso:
 *   screen_preview FILE.bmp LARGHEZZA ALTEZZA [TEMA] [MODO]
 *       TEMA  1..N oppure modern|c64|c128|amiga   (default: tema di avvio)
 *       MODO  console | splash                    (default: console)
 *   screen_preview --all
 *       una immagine per tema a 1280x720, in console, piu' lo splash.
 *
 * La sessione stampata sotto e' un campione fisso, non la shell vera: serve a
 * giudicare i colori, non a verificare i comandi.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fbcon.h"
#include "splash.h"
#include "theme.h"

/* -------------------------------------------------------- agganci alla scheda */

/*
 * Qui non c'e' hardware: questi sono gli unici simboli che il codice di HomeOS
 * si aspetta dal resto del sistema. Se in futuro una schermata usera' altro,
 * il compilatore lo dira' qui — ed e' esattamente quello che vogliamo.
 */
static uint32_t *g_pixels;
static uint32_t g_width = 1280u;
static uint32_t g_height = 720u;

volatile uint32_t *display_buffer(void) { return g_pixels; }
uint32_t display_width(void) { return g_width; }
uint32_t display_height(void) { return g_height; }
uint64_t services_ticks(void) { return 0; }
uint32_t services_avail_mem(void) { return 266771136u; }

/* ------------------------------------------------------------------- uscita */

static void put_u16(uint8_t *out, uint16_t value) {
    out[0] = (uint8_t)(value & 0xFFu);
    out[1] = (uint8_t)(value >> 8);
}

static void put_u32(uint8_t *out, uint32_t value) {
    out[0] = (uint8_t)(value & 0xFFu);
    out[1] = (uint8_t)((value >> 8) & 0xFFu);
    out[2] = (uint8_t)((value >> 16) & 0xFFu);
    out[3] = (uint8_t)(value >> 24);
}

/* BMP a 24 bit, righe dal basso verso l'alto: il formato piu' semplice da
 * scrivere senza dipendenze, e lo aprono tutti (sips compreso). */
static int write_bmp(const char *path, const uint32_t *pixels,
                     uint32_t width, uint32_t height) {
    const uint32_t row_size = ((width * 3u) + 3u) & ~3u;
    const uint32_t image_size = row_size * height;
    uint8_t *row = malloc(row_size);
    uint8_t header[54];
    FILE *file;

    if (!row) return 0;
    file = fopen(path, "wb");
    if (!file) {
        free(row);
        return 0;
    }

    memset(header, 0, sizeof(header));
    header[0] = 'B';
    header[1] = 'M';
    put_u32(header + 2, 54u + image_size);
    put_u32(header + 10, 54u);
    put_u32(header + 14, 40u);
    put_u32(header + 18, width);
    put_u32(header + 22, height);
    put_u16(header + 26, 1u);
    put_u16(header + 28, 24u);
    put_u32(header + 34, image_size);
    fwrite(header, 1, sizeof(header), file);

    for (uint32_t y = height; y-- > 0; ) {
        const uint32_t *source = pixels + (uintptr_t)y * width;
        memset(row, 0, row_size);
        for (uint32_t x = 0; x < width; x++) {
            uint32_t pixel = source[x];
            row[x * 3u + 0u] = (uint8_t)(pixel & 0xFFu);
            row[x * 3u + 1u] = (uint8_t)((pixel >> 8) & 0xFFu);
            row[x * 3u + 2u] = (uint8_t)((pixel >> 16) & 0xFFu);
        }
        fwrite(row, 1, row_size, file);
    }

    fclose(file);
    free(row);
    return 1;
}

/* ------------------------------------------------------------------ schermate */

static void console_sample(void) {
    const struct theme_colors *theme = theme_current();

    if (theme->classic) {
        fbcon_classic_banner();
    } else {
        fbcon_puts("HomeOS shell - type 'help' for the command list\n");
    }
    fbcon_puts("\n");

    /* Una sessione realistica: prompt, un comando, il suo output, prompt. */
    fbcon_puts_accent(theme->prompt);
    fbcon_puts("info\n");
    fbcon_puts("HomeOS 0.1 \"Mizar\"\n");
    fbcon_puts("  architecture : riscv64  (RISC-V 64-bit)\n");
    fbcon_puts("  board        : Milk-V Mars / StarFive JH7110\n");
    fbcon_puts("  profile      : supervisor (S-Mode)\n");
    fbcon_puts("  display      : 1280x720 XRGB8888\n");
    fbcon_puts_accent(theme->prompt);
}

static int render(const char *path, uint32_t width, uint32_t height,
                  int theme_index, int splash_mode) {
    uint32_t *pixels = calloc((size_t)width * height, sizeof(uint32_t));
    if (!pixels) {
        fprintf(stderr, "memoria esaurita per %ux%u\n", width, height);
        return 0;
    }

    g_pixels = pixels;
    g_width = width;
    g_height = height;

    if (theme_index >= 0) theme_set(theme_index);

    if (splash_mode) {
        splash_compose(pixels, width, height);
    } else {
        fbcon_init();
        console_sample();
    }

    if (!write_bmp(path, pixels, width, height)) {
        fprintf(stderr, "non riesco a scrivere %s\n", path);
        free(pixels);
        return 0;
    }

    printf("%-28s %4ux%-5u tema=%s\n", path, width, height,
           theme_current()->name);
    free(pixels);
    return 1;
}

static int find_theme(const char *name) {
    for (int i = 0; i < theme_count(); i++) {
        if (strcmp(theme_at(i)->name, name) == 0) return i;
    }
    return -1;
}

int main(int argc, char **argv) {
    if (argc == 2 && strcmp(argv[1], "--all") == 0) {
        int ok = 1;
        for (int i = 0; i < theme_count(); i++) {
            char path[64];
            snprintf(path, sizeof(path), "/tmp/screen-%s.bmp", theme_at(i)->name);
            ok &= render(path, 1280, 720, i, 0);
        }
        ok &= render("/tmp/screen-splash.bmp", 1280, 720, 0, 1);
        return ok ? 0 : 1;
    }

    if (argc < 5) {
        fprintf(stderr,
                "uso: %s FILE.bmp LARGHEZZA ALTEZZA [TEMA] [console|splash]\n"
                "     %s --all\n", argv[0], argv[0]);
        return 2;
    }

    int theme_index = -1;
    int splash_mode = 0;

    if (argc >= 5) {
        theme_index = find_theme(argv[4]);
        if (theme_index < 0) {
            int number = atoi(argv[4]);
            if (number >= 1 && number <= theme_count()) theme_index = number - 1;
        }
        if (theme_index < 0) {
            fprintf(stderr, "tema sconosciuto: %s\n", argv[4]);
            return 2;
        }
    }
    if (argc >= 6) {
        if (strcmp(argv[5], "splash") == 0) splash_mode = 1;
        else if (strcmp(argv[5], "console") != 0) {
            fprintf(stderr, "modo sconosciuto: %s\n", argv[5]);
            return 2;
        }
    }

    return render(argv[1], (uint32_t)atoi(argv[2]), (uint32_t)atoi(argv[3]),
                  theme_index, splash_mode) ? 0 : 1;
}
