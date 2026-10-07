#include "fbcon.h"
#include "display.h"
#include "font_topaz.h"
#include "gfx.h"
#include "services.h"
#include "theme.h"
#include "version.h"

#define CELL_W    8u
#define CELL_H    16u
#define HEADER_H  32u
#define MAX_COLS  240u   /* 1920 / 8  */
#define MAX_ROWS  67u    /* (1080-32) / 16 */

#define ATTR_NORMAL 0u
#define ATTR_ACCENT 1u

static char g_text[MAX_ROWS][MAX_COLS];
static uint8_t g_attr[MAX_ROWS][MAX_COLS];

static uint32_t g_cols = MAX_COLS;
static uint32_t g_rows = MAX_ROWS;
static uint32_t g_col, g_row;
static uint32_t g_x0, g_y0;      /* origine del contenuto   */
static uint32_t g_text_y;        /* prima riga dell'area di testo */
static uint32_t g_header_lines;  /* righe di banner riservate (0 = barra) */
static const struct theme_colors *g_theme;
static int g_ready;

/* Il cursore lampeggia: mezzo secondo acceso, mezzo spento. */
static int g_cursor_visible = 1;
static uint32_t g_cursor_col, g_cursor_row;   /* dove il cursore e' disegnato */
static int g_cursor_drawn;
static uint64_t g_blink_last;

static uint32_t text_y(void) { return g_text_y; }

static void draw_glyph(uint32_t x, uint32_t y, char c, uint32_t color);

static int u32_to_text(uint32_t value, char *out) {
    char tmp[11];
    int n = 0;
    if (value == 0) tmp[n++] = '0';
    while (value > 0) {
        tmp[n++] = (char)('0' + (value % 10u));
        value /= 10u;
    }
    for (int i = 0; i < n; i++) out[i] = tmp[n - 1 - i];
    out[n] = '\0';
    return n;
}

/*
 * Riga di stato della memoria, come sulle macchine a 8 bit: totale di sistema
 * e byte realmente liberi. Il valore libero e' letto adesso dall'allocatore
 * del Core, quindi non e' un numero scritto a mano.
 */
static void compose_ram_line(char *out) {
    const char *prefix = "512 MB RAM SYSTEM  ";
    int n = 0;
    for (int i = 0; prefix[i]; i++) out[n++] = prefix[i];
    n += u32_to_text(services_avail_mem(), out + n);
    const char *suffix = " BYTES FREE";
    for (int i = 0; suffix[i]; i++) out[n++] = suffix[i];
    out[n] = '\0';
}

static void draw_glyph(uint32_t x, uint32_t y, char c, uint32_t color) {
    uint8_t index = (uint8_t)c;
    if (index < 0x20u || index > 0x7Eu) index = (uint8_t)'?';
    const uint8_t *glyph = font_topaz[index - 0x20u];
    for (uint32_t row = 0; row < CELL_H; row++) {
        uint8_t bits = glyph[row];
        if (!bits) continue;
        for (uint32_t col = 0; col < CELL_W; col++) {
            if (bits & (0x80u >> col)) {
                gfx_fill_rect(x + col, y + row, 1, 1, color);
            }
        }
    }
}

static void render_cell(uint32_t col, uint32_t row) {
    if (col >= g_cols || row >= g_rows) return;
    uint32_t x = g_x0 + col * CELL_W;
    uint32_t y = text_y() + row * CELL_H;
    gfx_fill_rect(x, y, CELL_W, CELL_H, g_theme->bg);
    char c = g_text[row][col];
    if (c) {
        draw_glyph(x, y, c,
                   g_attr[row][col] == ATTR_ACCENT ? g_theme->accent : g_theme->text);
    }
}

/*
 * Cursore. Nel tema classico e' un blocco pieno, come sulla macchina
 * originale; nel moderno una barretta in fondo alla cella, che non copre il
 * testo ed e' la convenzione dei terminali di oggi.
 */
static void draw_cursor(void) {
    /*
     * Prima si cancella il cursore dalla posizione precedente. Senza questo,
     * tornando indietro con il backspace — o a inizio riga — restava un
     * quadratino del colore del cursore nella cella lasciata libera, perche'
     * nessuno la ridisegnava.
     */
    if (g_cursor_drawn) {
        render_cell(g_cursor_col, g_cursor_row);
        g_cursor_drawn = 0;
    }

    if (!g_ready || !g_cursor_visible) return;
    if (g_col >= g_cols || g_row >= g_rows) return;
    uint32_t x = g_x0 + g_col * CELL_W;
    uint32_t y = text_y() + g_row * CELL_H;
    if (g_theme->retro) {
        gfx_fill_rect(x, y, CELL_W, CELL_H, g_theme->accent);
    } else {
        gfx_fill_rect(x, y + CELL_H - 2u, CELL_W, 2u, g_theme->accent);
    }
    g_cursor_col = g_col;
    g_cursor_row = g_row;
    g_cursor_drawn = 1;
}

/*
 * Da richiamare mentre si aspetta l'input: fa lampeggiare il cursore. Fuori
 * dall'attesa resta nello stato in cui si trova, cosi' mentre si digita e si
 * stampa il cursore non salta.
 */
void fbcon_cursor_blink(void) {
    if (!g_ready) return;
    uint64_t now = services_ticks();
    uint64_t half_second = HOMEOS_TIMER_FREQ_HZ / 2u;
    if (now - g_blink_last < half_second) return;
    g_blink_last = now;

    g_cursor_visible = !g_cursor_visible;
    draw_cursor();
}

static void render_header(void) {
    const uint32_t w = gfx_width();
    const uint32_t h = gfx_height();

    if (g_theme->border) {
        /* Stile classico: cornice piena invece della barra. L'interno si
         * riempie tutto d'un colpo: riempire a pezzi lasciava scoperto il
         * margine destro e la riga vuota sotto l'intestazione, che apparivano
         * come una fascia del colore del contorno. */
        gfx_fill_rect(0, 0, w, h, g_theme->border);
        gfx_fill_rect(g_x0, g_y0, w - 2u * g_x0, h - 2u * g_y0, g_theme->bg);

        /* Nel tema classico non c'e' intestazione fissa: banner e dati sono
         * righe di testo scritte all'avvio, che scorrono via con il resto
         * della console. Qui si disegna soltanto la cornice. */
        return;
    }

    gfx_fill_rect(0, 0, w, HEADER_H, g_theme->header_bg);
    gfx_fill_rect(0, HEADER_H - 2u, w, 2u, g_theme->accent);

    const char *title = "HomeOS " HOMEOS_VERSION;
    gfx_text(8u, 7u, title, g_theme->accent, 1u);

    const char *right = HOMEOS_ARCH;
    uint32_t rlen = 0;
    while (right[rlen]) rlen++;
    uint32_t rw = rlen * CELL_W;
    if (w > rw + 16u) gfx_text(w - rw - 8u, 7u, right, g_theme->muted, 1u);
}

static void scroll_up(void) {
    const uint32_t w = gfx_width();
    volatile uint32_t *fb = display_buffer();
    const uint32_t rows_to_move = (g_rows - 1u) * CELL_H;

    for (uint32_t y = 0; y < rows_to_move; y++) {
        volatile uint32_t *dst = fb + (uintptr_t)(text_y() + y) * w + g_x0;
        volatile uint32_t *src = fb + (uintptr_t)(text_y() + y + CELL_H) * w + g_x0;
        for (uint32_t x = 0; x < g_cols * CELL_W; x++) dst[x] = src[x];
    }
    for (uint32_t row = 0; row + 1u < g_rows; row++) {
        for (uint32_t col = 0; col < g_cols; col++) {
            g_text[row][col] = g_text[row + 1u][col];
            g_attr[row][col] = g_attr[row + 1u][col];
        }
    }
    for (uint32_t col = 0; col < g_cols; col++) {
        g_text[g_rows - 1u][col] = 0;
        g_attr[g_rows - 1u][col] = ATTR_NORMAL;
    }
    for (uint32_t col = 0; col < g_cols; col++) {
        render_cell(col, g_rows - 1u);
    }
}

static void newline(void) {
    g_col = 0;
    if (g_row + 1u >= g_rows) {
        scroll_up();
        g_row = g_rows - 1u;
    } else {
        g_row++;
    }
}

static void put_char(char c, uint8_t attr) {
    g_cursor_visible = 1;   /* mentre si digita il cursore resta acceso */
    if (g_col >= g_cols) newline();
    if (c == ' ') {
        g_text[g_row][g_col] = 0;
    } else {
        g_text[g_row][g_col] = c;
    }
    g_attr[g_row][g_col] = attr;
    render_cell(g_col, g_row);
    g_col++;
    if (g_col >= g_cols) newline();
}

void fbcon_clear(void) {
    for (uint32_t row = 0; row < g_rows; row++) {
        for (uint32_t col = 0; col < g_cols; col++) {
            g_text[row][col] = 0;
            g_attr[row][col] = ATTR_NORMAL;
        }
    }
    g_col = 0;
    g_row = 0;
    g_cursor_drawn = 0;
    gfx_fill_rect(g_x0, text_y(), g_cols * CELL_W, g_rows * CELL_H, g_theme->bg);
    draw_cursor();
}

void fbcon_redo(void) {
    if (!g_ready) return;
    render_header();
    gfx_fill_rect(g_x0, text_y(), g_cols * CELL_W, g_rows * CELL_H, g_theme->bg);
    for (uint32_t row = 0; row < g_rows; row++) {
        for (uint32_t col = 0; col < g_cols; col++) {
            if (g_text[row][col]) render_cell(col, row);
        }
    }
    draw_cursor();
}

/* Calcola la geometria della console dal tema e dalla risoluzione corrente.
 * Non tocca il contenuto: la usano sia l'inizializzazione sia la schermata di
 * avvio classica, cosi' le due disegnano esattamente lo stesso impianto. */
static void fbcon_setup(void) {
    gfx_init(display_buffer(), display_width(), display_height());
    g_theme = theme_current();

    const uint32_t w = gfx_width();
    const uint32_t h = gfx_height();
    /* Nel tema classico il bordo e' spesso un ventiquattresimo dell'altezza,
     * come sulle macchine a 8 bit dove occupava una porzione fissa del quadro. */
    const uint32_t border = g_theme->border ? (h / 24u) : 0u;

    g_x0 = border;
    g_y0 = border;
    g_header_lines = 0u;
    g_text_y = g_theme->border ? border : HEADER_H;

    g_cols = (w - 2u * border) / CELL_W;
    g_rows = (h - g_text_y - border) / CELL_H;
    if (g_cols > MAX_COLS) g_cols = MAX_COLS;
    if (g_rows > MAX_ROWS) g_rows = MAX_ROWS;
}

void fbcon_init(void) {
    fbcon_setup();

    g_col = 0;
    g_row = 0;
    g_cursor_drawn = 0;
    g_ready = 1;

    render_header();
    gfx_fill_rect(g_x0, text_y(), g_cols * CELL_W, g_rows * CELL_H, g_theme->bg);
    for (uint32_t row = 0; row < g_rows; row++) {
        for (uint32_t col = 0; col < g_cols; col++) {
            g_text[row][col] = 0;
            g_attr[row][col] = ATTR_NORMAL;
        }
    }
    draw_cursor();
}

/*
 * Schermata di avvio del tema classico: e' letteralmente l'intestazione della
 * console senza testo sotto. Cosi' quando la console prende il controllo non
 * cambia nulla sullo schermo: si aggiunge soltanto il testo.
 */
void fbcon_classic_boot_screen(void) {
    fbcon_setup();
    g_ready = 0;
    render_header();
}

/* Riga centrata nella console, scritta come testo normale. */
static void console_centered(const char *text, int accent) {
    uint32_t length = 0;
    while (text[length]) length++;
    uint32_t pad = (length < g_cols) ? (g_cols - length) / 2u : 0u;
    for (uint32_t i = 0; i < pad; i++) fbcon_putc(' ');
    if (accent) fbcon_puts_accent(text);
    else fbcon_puts(text);
    fbcon_putc('\n');
}

/*
 * Banner di avvio del tema classico, scritto come testo: sta in cima allo
 * schermo finche' c'e' spazio, poi scorre via insieme alle righe precedenti.
 * E' il comportamento della macchina originale, dove la schermata di avvio
 * non era altro che la prima schermata della console.
 */
void fbcon_classic_banner(void) {
    if (!g_ready) return;
    /* Tre righe come all'origine: banner lungo, dati della macchina e del
     * kernel su una riga sola, poi la memoria. */
    console_centered("**** HOMEOS " HOMEOS_VERSION "  -  RISC-V 64-BIT ****", 1);
    console_centered(HOMEOS_BOARD "  -  KERNEL " HOMEOS_CORE_NAME
                     " v" HOMEOS_CORE_VERSION, 0);
    char ram[64];
    compose_ram_line(ram);
    console_centered(ram, 0);
    fbcon_putc('\n');
}

void fbcon_putc(char c) {
    if (!g_ready) return;
    switch (c) {
        case '\n':
            render_cell(g_col < g_cols ? g_col : g_cols - 1u, g_row);
            newline();
            draw_cursor();
            break;
        case '\r':
            g_col = 0;
            draw_cursor();
            break;
        case '\b':
            if (g_col > 0) {
                g_col--;
            } else if (g_row > 0) {
                g_row--;
                g_col = g_cols - 1u;
            }
            g_text[g_row][g_col] = 0;
            render_cell(g_col, g_row);
            draw_cursor();
            break;
        default:
            put_char(c, ATTR_NORMAL);
            draw_cursor();
            break;
    }
}

void fbcon_puts(const char *text) {
    if (!g_ready || !text) return;
    for (uint32_t i = 0; text[i]; i++) fbcon_putc(text[i]);
}

void fbcon_puts_accent(const char *text) {
    if (!g_ready || !text) return;
    for (uint32_t i = 0; text[i]; i++) {
        if (text[i] == '\n') {
            fbcon_putc('\n');
        } else {
            put_char(text[i], ATTR_ACCENT);
            draw_cursor();
        }
    }
}
