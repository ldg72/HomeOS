#include "gfx.h"
#include "font8x16.h"

#define GLYPH_W 8u
#define GLYPH_H 16u

static volatile uint32_t *g_fb;
static uint32_t g_w, g_h;

void gfx_init(volatile uint32_t *framebuffer, uint32_t width, uint32_t height) {
    g_fb = framebuffer;
    g_w = width;
    g_h = height;
}

uint32_t gfx_width(void)  { return g_w; }
uint32_t gfx_height(void) { return g_h; }

void gfx_pixel(uint32_t x, uint32_t y, uint32_t color) {
    if (!g_fb || x >= g_w || y >= g_h) return;
    g_fb[(uintptr_t)y * g_w + x] = color;
}

void gfx_fill_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color) {
    if (!g_fb) return;
    for (uint32_t j = 0; j < h; j++) {
        uint32_t py = y + j;
        if (py >= g_h) break;
        volatile uint32_t *row = g_fb + (uintptr_t)py * g_w;
        for (uint32_t i = 0; i < w; i++) {
            uint32_t px = x + i;
            if (px >= g_w) break;
            row[px] = color;
        }
    }
}

static uint32_t lerp(uint32_t a, uint32_t b, uint32_t t) {
    uint32_t ar = (a >> 16) & 0xffu, ag = (a >> 8) & 0xffu, ab = a & 0xffu;
    uint32_t br = (b >> 16) & 0xffu, bg = (b >> 8) & 0xffu, bb = b & 0xffu;
    uint32_t r = ar + (((br - ar) * t) / 255u);
    uint32_t g = ag + (((bg - ag) * t) / 255u);
    uint32_t bl = ab + (((bb - ab) * t) / 255u);
    return 0xFF000000u | (r << 16) | (g << 8) | bl;
}

void gfx_vgradient(uint32_t x, uint32_t y, uint32_t w, uint32_t h,
                   uint32_t top, uint32_t bottom) {
    if (!g_fb || h == 0) return;
    for (uint32_t j = 0; j < h; j++) {
        uint32_t t = (h > 1u) ? ((j * 255u) / (h - 1u)) : 0u;
        gfx_fill_rect(x, y + j, w, 1, lerp(top, bottom, t));
    }
}

/* Rientro orizzontale di un angolo arrotondato, con radice intera. */
static uint32_t corner_inset(uint32_t dy, uint32_t radius) {
    if (dy >= radius) return 0;
    int32_t d = (int32_t)radius - 1 - (int32_t)dy;
    int32_t sq = (int32_t)radius * (int32_t)radius - d * d;
    if (sq <= 0) return 0;
    int32_t s = 0;
    while ((s + 1) * (s + 1) <= sq) s++;
    return (uint32_t)((int32_t)radius - s);
}

static uint32_t row_inset(uint32_t j, uint32_t h, uint32_t radius) {
    if (j < radius) return corner_inset(j, radius);
    if (j >= h - radius) return corner_inset(h - 1u - j, radius);
    return 0;
}

void gfx_rounded_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h,
                      uint32_t radius, uint32_t color) {
    if (!g_fb || w == 0 || h == 0) return;
    if (radius * 2u > w) radius = w / 2u;
    if (radius * 2u > h) radius = h / 2u;
    for (uint32_t j = 0; j < h; j++) {
        uint32_t inset = row_inset(j, h, radius);
        if (w > 2u * inset) gfx_fill_rect(x + inset, y + j, w - 2u * inset, 1, color);
    }
}

void gfx_rounded_gradient(uint32_t x, uint32_t y, uint32_t w, uint32_t h,
                          uint32_t radius, uint32_t top, uint32_t bottom) {
    if (!g_fb || w == 0 || h == 0) return;
    if (radius * 2u > w) radius = w / 2u;
    if (radius * 2u > h) radius = h / 2u;
    for (uint32_t j = 0; j < h; j++) {
        uint32_t inset = row_inset(j, h, radius);
        uint32_t t = (h > 1u) ? ((j * 255u) / (h - 1u)) : 0u;
        if (w > 2u * inset) {
            gfx_fill_rect(x + inset, y + j, w - 2u * inset, 1, lerp(top, bottom, t));
        }
    }
}

void gfx_char_font(uint32_t x, uint32_t y, char c, uint32_t color, uint32_t scale,
                   const uint8_t (*font)[16]) {
    if (!g_fb || scale == 0) return;
    uint8_t index = (uint8_t)c;
    if (index < 0x20u || index > 0x7Eu) index = (uint8_t)'?';
    const uint8_t *glyph = font[index - 0x20u];
    for (uint32_t row = 0; row < GLYPH_H; row++) {
        uint8_t bits = glyph[row];
        if (!bits) continue;
        for (uint32_t col = 0; col < GLYPH_W; col++) {
            if (bits & (0x80u >> col)) {
                gfx_fill_rect(x + col * scale, y + row * scale, scale, scale, color);
            }
        }
    }
}

void gfx_char(uint32_t x, uint32_t y, char c, uint32_t color, uint32_t scale) {
    gfx_char_font(x, y, c, color, scale, font8x16);
}

uint32_t gfx_text_width(const char *text, uint32_t scale) {
    uint32_t n = 0;
    while (text && text[n]) n++;
    return n * GLYPH_W * scale;
}

void gfx_text(uint32_t x, uint32_t y, const char *text, uint32_t color, uint32_t scale) {
    if (!text) return;
    uint32_t cx = x;
    for (uint32_t i = 0; text[i]; i++) {
        gfx_char(cx, y, text[i], color, scale);
        cx += GLYPH_W * scale;
    }
}

void gfx_text_centered(uint32_t y, const char *text, uint32_t color, uint32_t scale) {
    uint32_t w = gfx_text_width(text, scale);
    uint32_t x = (w < g_w) ? (g_w - w) / 2u : 0u;
    gfx_text(x, y, text, color, scale);
}

/*
 * Testo riempito con una sfumatura verticale: ogni riga del glifo prende il
 * colore interpolato. Rende il wordmark meno "terminale" senza aggiungere
 * risorse grafiche.
 *
 * La rampa si chiude a GRADIENT_SPAN righe e non a 15: in questo font i glifi
 * occupano circa le prime 12 righe della cella 8x16, quindi interpolando su
 * tutta la cella la sfumatura resta impercettibile.
 */
#define GRADIENT_SPAN 12u

void gfx_text_centered_gradient(uint32_t y, const char *text,
                                uint32_t top, uint32_t bottom, uint32_t scale) {
    if (!g_fb || !text || scale == 0) return;
    uint32_t w = gfx_text_width(text, scale);
    uint32_t x0 = (w < g_w) ? (g_w - w) / 2u : 0u;
    uint32_t cx = x0;

    for (uint32_t i = 0; text[i]; i++) {
        uint8_t index = (uint8_t)text[i];
        if (index < 0x20u || index > 0x7Eu) index = (uint8_t)'?';
        const uint8_t *glyph = font8x16[index - 0x20u];
        for (uint32_t row = 0; row < GLYPH_H; row++) {
            uint8_t bits = glyph[row];
            if (!bits) continue;
            uint32_t span = (row < GRADIENT_SPAN) ? row : GRADIENT_SPAN;
            uint32_t color = lerp(top, bottom, (span * 255u) / GRADIENT_SPAN);
            for (uint32_t col = 0; col < GLYPH_W; col++) {
                if (bits & (0x80u >> col)) {
                    gfx_fill_rect(cx + col * scale, y + row * scale, scale, scale, color);
                }
            }
        }
        cx += GLYPH_W * scale;
    }
}
