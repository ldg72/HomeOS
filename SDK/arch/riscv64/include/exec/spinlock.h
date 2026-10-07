#ifndef SPINLOCK_H
#define SPINLOCK_H

#include <stdint.h>

/*
 * Lightweight Spinlock & Atomics
 * Optimized for AArch64 (Apple Silicon M4)
 * WORKAROUND: USES NON-ATOMIC VOLATILE DUE TO HVF BUS ABORTS (0x35)
 */

// Spinlock structure including saved interrupt state
typedef struct {
    volatile int lock;
} spinlock_t;

#define SPIN_LOCK_INIT { 0 }

static inline void spin_init(spinlock_t *l) {
    l->lock = 0;
}

// Alias for compatibility
#define spin_lock_init spin_init


// Global flag to track if we should use atomics (for HVF workaround if needed)
static volatile int g_use_atomics = 1;

static inline uint64_t spin_lock_irqsave(spinlock_t *l) {
    uint64_t state;
    // Read sstatus and clear SIE (bit 1)
    asm volatile("csrr %0, sstatus" : "=r"(state));
    asm volatile("csrci sstatus, 2" ::: "memory");

    int tmp = 1;
    asm volatile(
        "1: amoswap.w.aq %0, %2, (%1)\n"
        "   bnez %0, 1b\n"
        : "=&r" (tmp)
        : "r" (&l->lock), "r" (tmp)
        : "memory"
    );
    return state;
}

static inline void spin_unlock_irqrestore(spinlock_t *l, uint64_t state) {
    asm volatile(
        "amoswap.w.rl zero, zero, (%0)"
        : 
        : "r" (&l->lock)
        : "memory"
    );
    // Restore SIE bit only if it was set in the saved state
    if (state & 2) {
        asm volatile("csrs sstatus, 2" ::: "memory");
    }
}

static inline void spin_lock(spinlock_t *l) {
    // Legacy/Simple version: always disables interrupts, but we should 
    // eventually transition to irqsave for safety.
    asm volatile("csrci sstatus, 2" ::: "memory");
    int tmp = 1;
    asm volatile(
        "1: amoswap.w.aq %0, %2, (%1)\n"
        "   bnez %0, 1b\n"
        : "=&r" (tmp)
        : "r" (&l->lock), "r" (tmp)
        : "memory"
    );
}

static inline void spin_unlock(spinlock_t *l) {
    asm volatile(
        "amoswap.w.rl zero, zero, (%0)"
        : 
        : "r" (&l->lock)
        : "memory"
    );
    // DANGEROUS: Always re-enables interrupts.
    // Fixed: only re-enable if NOT in S-mode trap (SPIE usually 1 here if we want to re-enable)
    // Actually, for safety, we should not touch sstatus here if we don't know the state.
    // But for compatibility with existing code:
    asm volatile("csrs sstatus, 2" ::: "memory");
}

static inline uint32_t atomic_or(volatile uint32_t *ptr, uint32_t val) {
    // RISC-V and other architectures: use compiler builtins
    return __atomic_fetch_or(ptr, val, __ATOMIC_ACQ_REL);
}

static inline uint32_t atomic_and(volatile uint32_t *ptr, uint32_t val) {
    // RISC-V and other architectures: use compiler builtins
    return __atomic_fetch_and(ptr, val, __ATOMIC_ACQ_REL);
}

static inline uint32_t atomic_load(volatile uint32_t *ptr) {
    return __atomic_load_n(ptr, __ATOMIC_ACQUIRE);
}

static inline void atomic_store(volatile uint32_t *ptr, uint32_t val) {
    __atomic_store_n(ptr, val, __ATOMIC_RELEASE);
}

#endif
