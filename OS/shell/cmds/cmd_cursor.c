/*
 * Comando 'cursor' — accende, sposta e prova il puntatore hardware.
 *
 * Serve prima del mouse: il piano cursore del DC8200 e' indipendente dal
 * mouse e dal resto del display, quindi si puo' verificare da tastiera. Il
 * comando 'cursor test' lo fa camminare sul bordo dello schermo: se il
 * puntatore si muove, il piano e' vivo e la posizione e' quella giusta.
 *
 * Il cursore si accende alla prima chiamata, non all'avvio: chi non lo usa
 * non paga niente e il comportamento di boot resta quello di prima.
 */

#include "../command.h"
#include "../shell.h"
#include "../../cursor.h"
#include "../../display.h"
#include "../../fbcon.h"
#include "../../gfx.h"
#include "../../services.h"

static int str_eq(const char *a, const char *b) {
    while (*a && *b) {
        if (*a != *b) return 0;
        a++;
        b++;
    }
    return *a == *b;
}

static int parse_int(const char *text, int32_t *out) {
    int32_t value = 0;
    int digits = 0;
    int negative = 0;

    if (*text == '-') {
        negative = 1;
        text++;
    }
    for (; *text >= '0' && *text <= '9'; text++) {
        value = value * 10 + (*text - '0');
        digits++;
    }
    if (!digits) return 0;
    *out = negative ? -value : value;
    return 1;
}

/*
 * Porta il cursore a fare il giro dello schermo: e' la prova che si muove.
 *
 * Il giro sta qualche decina di pixel dentro il bordo, non sul bordo: la
 * punta del puntatore e' l'angolo in alto a sinistra, e il corpo si estende
 * in basso a destra, quindi con la punta sul bordo destro o inferiore il
 * puntatore esce dallo schermo e se ne vede solo una scheggia. Camminando
 * dentro, il giro si vede tutto.
 */
static void walk_border(void) {
    const int32_t width = (int32_t)display_width();
    const int32_t height = (int32_t)display_height();
    const int32_t step = 16;
    const int32_t margin = 40;
    const int32_t left = margin;
    const int32_t top = margin;
    const int32_t right = width - 1 - margin;
    const int32_t bottom = height - 1 - margin;

    if (right <= left || bottom <= top) return;

    for (int32_t x = left; x < right; x += step) {
        cursor_move(x, top);
        services_delay_ms(8);
    }
    for (int32_t y = top; y < bottom; y += step) {
        cursor_move(right, y);
        services_delay_ms(8);
    }
    for (int32_t x = right; x > left; x -= step) {
        cursor_move(x, bottom);
        services_delay_ms(8);
    }
    for (int32_t y = bottom; y > top; y -= step) {
        cursor_move(left, y);
        services_delay_ms(8);
    }
}

/*
 * Un mirino sul framebuffer, nel punto in cui chiediamo al puntatore di
 * andare. Serve a togliere ogni ambiguita': a occhio, "e' a meta' schermo"
 * non e' un dato, "e' dentro la croce si' o no" lo e'.
 */
static void draw_cross(uint32_t x, uint32_t y) {
    gfx_init(display_buffer(), display_width(), display_height());
    gfx_fill_rect(x - 14u, y - 1u, 29u, 3u, 0xFF00FF00u);
    gfx_fill_rect(x - 1u, y - 14u, 3u, 29u, 0xFF00FF00u);
}

static void probe_points(void) {
    const uint32_t x[4] = { display_width() / 4u, display_width() * 3u / 4u,
                            display_width() / 4u, display_width() * 3u / 4u };
    const uint32_t y[4] = { display_height() / 4u, display_height() / 4u,
                            display_height() * 3u / 4u, display_height() * 3u / 4u };

    shell_puts("four crosses, two seconds each:\n");
    shell_puts("the pointer must land inside every one of them\n");

    for (int i = 0; i < 4; i++) {
        shell_puts("  cross ");
        shell_putu32((uint32_t)(i + 1));
        shell_puts(" at ");
        shell_putu32(x[i]);
        shell_puts(",");
        shell_putu32(y[i]);
        shell_puts("\n");
        draw_cross(x[i], y[i]);
        cursor_move((int32_t)x[i], (int32_t)y[i]);
        services_delay_ms(2000);
    }

    fbcon_redo();
    shell_puts("which crosses did the pointer reach?\n");
}

static int ensure_ready(void) {
    int rc;

    if (cursor_is_ready()) return 1;

    rc = cursor_init();
    if (rc == CURSOR_OK) return 1;

    shell_puts("cursor: non disponibile (");
    if (rc == CURSOR_ERR_NO_ROOM) {
        shell_puts("riserva framebuffer piena: a 1080p non c'e' posto per "
                   "l'immagine");
    } else if (rc == CURSOR_ERR_NO_DISPLAY) {
        shell_puts("display non inizializzato");
    } else {
        shell_puts("errore ");
        shell_putu32((uint32_t)(-rc));
    }
    shell_puts(")\n");
    return 0;
}

static void print_status(void) {
    uint16_t x = 0, y = 0;
    uint32_t config = 0, clk_gating = 0;

    shell_puts("hardware pointer: ");
    if (!cursor_is_ready()) {
        shell_puts("not started\n");
        return;
    }

    cursor_position(&x, &y);
    shell_puts(cursor_is_enabled() ? "on" : "off");
    shell_puts("\n  position    : ");
    shell_putu32(x);
    shell_puts(", ");
    shell_putu32(y);
    shell_puts("\n  size code   : ");
    shell_putu32(cursor_size_code());
    shell_puts("   (32x32 o 64x64, codifica da provare)\n");
    shell_puts("  image       : physical=");
    shell_puthex64(cursor_image_phys());
    shell_puts(" offset=");
    shell_puthex64(cursor_image_offset());
    shell_puts("\n");

    cursor_diag(&config, &clk_gating);
    shell_puts("  config reg  : ");
    shell_puthex64(config);
    shell_puts("\n  clk gating  : ");
    shell_puthex64(clk_gating);
    shell_puts("\n");
}

SHELL_COMMAND(cmd_cursor, "cursor",
              "hardware pointer: 'cursor', 'cursor X Y', 'cursor test'") {
    if (argc == 1) {
        print_status();
        return;
    }

    if (str_eq(argv[1], "test")) {
        if (!ensure_ready()) return;
        cursor_enable(1);
        shell_puts("walking the screen border...\n");
        walk_border();
        shell_puts("done\n");
        return;
    }

    if (str_eq(argv[1], "probe")) {
        if (!ensure_ready()) return;
        cursor_enable(1);
        probe_points();
        return;
    }

    if (str_eq(argv[1], "on") || str_eq(argv[1], "off")) {
        if (!ensure_ready()) return;
        cursor_enable(str_eq(argv[1], "on"));
        shell_puts("hardware pointer ");
        shell_puts(cursor_is_enabled() ? "on\n" : "off\n");
        return;
    }

    if (str_eq(argv[1], "size") && argc >= 3) {
        int32_t code = 0;
        if (!ensure_ready()) return;
        if (!parse_int(argv[2], &code) || code < 0 || code > 7) {
            shell_puts("usage: cursor size <0..7>\n");
            return;
        }
        cursor_set_size_code((uint32_t)code);
        shell_puts("size code = ");
        shell_putu32(cursor_size_code());
        shell_puts("\n");
        return;
    }

    if (str_eq(argv[1], "shape") && argc >= 3) {
        int32_t code = 0;
        if (!ensure_ready()) return;
        if (!parse_int(argv[2], &code) || code < 0 || code >= CURSOR_SHAPE_COUNT) {
            shell_puts("usage: cursor shape <0..");
            shell_putu32((uint32_t)(CURSOR_SHAPE_COUNT - 1));
            shell_puts(">\n");
            return;
        }
        cursor_set_shape((int)code);
        shell_puts("shape = ");
        shell_puts(cursor_shape_name(cursor_shape()));
        shell_puts("\n");
        return;
    }

    if (argc >= 3) {
        int32_t x = 0, y = 0;
        if (!ensure_ready()) return;
        if (!parse_int(argv[1], &x) || !parse_int(argv[2], &y)) {
            shell_puts("usage: cursor <x> <y>\n");
            return;
        }
        cursor_move(x, y);
        return;
    }

    shell_puts("usage: cursor [X Y | on | off | size <n> | test | probe]\n");
}
