#ifndef HOMEOS_GFX_H
#define HOMEOS_GFX_H

/*
 * Primitive grafiche minime su framebuffer XRGB8888 lineare.
 * Monolitico: nessuna libreria, nessuna astrazione. Il colore ha il byte alto
 * come alpha e deve valere 0xFF, altrimenti il pixel risulta trasparente.
 */

#include <stdint.h>

void gfx_init(volatile uint32_t *framebuffer, uint32_t width, uint32_t height);

void gfx_pixel(uint32_t x, uint32_t y, uint32_t color);
void gfx_fill_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color);
void gfx_vgradient(uint32_t x, uint32_t y, uint32_t w, uint32_t h,
                   uint32_t top, uint32_t bottom);
void gfx_rounded_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h,
                      uint32_t radius, uint32_t color);
void gfx_rounded_gradient(uint32_t x, uint32_t y, uint32_t w, uint32_t h,
                          uint32_t radius, uint32_t top, uint32_t bottom);

void gfx_char(uint32_t x, uint32_t y, char c, uint32_t color, uint32_t scale);
/* Come gfx_char, ma con un font esplicito (16 righe da 8 pixel per glifo). */
void gfx_char_font(uint32_t x, uint32_t y, char c, uint32_t color, uint32_t scale,
                   const uint8_t (*font)[16]);
void gfx_text(uint32_t x, uint32_t y, const char *text, uint32_t color, uint32_t scale);
uint32_t gfx_text_width(const char *text, uint32_t scale);
void gfx_text_centered(uint32_t y, const char *text, uint32_t color, uint32_t scale);
void gfx_text_centered_gradient(uint32_t y, const char *text,
                                uint32_t top, uint32_t bottom, uint32_t scale);

uint32_t gfx_width(void);
uint32_t gfx_height(void);

#endif
