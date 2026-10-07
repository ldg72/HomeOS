#ifndef HOMEOS_MMIO_H
#define HOMEOS_MMIO_H

/*
 * Accesso MMIO. Volatile + barriera di ordinamento dopo la scrittura: serve a
 * garantire che i registri siano scritti nell'ordine previsto prima di leggere
 * lo stato del blocco. Non e' una flush di cache e non rende coerente memoria
 * condivisa con un DMA.
 */

#include <stdint.h>

static inline uint32_t mmio_read32(uintptr_t addr) {
    return *(volatile uint32_t *)addr;
}

static inline void mmio_write32(uintptr_t addr, uint32_t value) {
    *(volatile uint32_t *)addr = value;
    asm volatile("fence rw, rw" ::: "memory");
}

static inline void mmio_fence(void) {
    asm volatile("fence rw, rw" ::: "memory");
}

#endif
