#include <stdint.h>
#include <time.h>

static uint64_t tick_to_ns_mul = 1;
static uint64_t tick_to_ns_div = 1;

static uint64_t gcd(uint64_t a, uint64_t b) {
    uint64_t c;
    while (b) {
        c = a % b;
        a = b;
        b = c;
    }
    return a;
}

#if __x86_64

#include <x86intrin.h>

static inline uint64_t tick_time(void) {
    return __rdtsc();
}

static inline uint64_t tick_freq(void) {
    return 1e9;
}

static inline void tick_init(void) {
}

#elif 0

static inline uint64_t tick_time(void) {
    return clock_gettime_nsec_np(CLOCK_MONOTONIC);
}

static inline uint64_t tick_freq(void) {
    return 1e9;
}

static inline void tick_init(void) {
    tick_to_ns_mul = 1;
    tick_to_ns_div = 1;
}

#elif __ARM_ARCH_ISA_A64

#include <arm_acle.h>

static inline uint64_t tick_time(void) {
    asm volatile ("nop");
    return __arm_rsr64("CNTVCT_EL0") * tick_to_ns_mul / tick_to_ns_div;
}

static inline uint64_t tick_freq(void) {
    return __arm_rsr64("CNTFRQ_EL0");
}

static inline void tick_init(void) {
    tick_to_ns_mul = 1e9;
    tick_to_ns_div = tick_freq();

    uint64_t g = gcd(tick_to_ns_mul, tick_to_ns_div);

    tick_to_ns_mul /= g;
    tick_to_ns_div /= g;
}

#else
#error "Platform not supported."
#endif
