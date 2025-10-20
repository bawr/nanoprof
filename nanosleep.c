#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

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

static inline volatile uint64_t tick_time(void) {
    asm volatile ("");
    return __builtin_arm_rsr64("CNTVCT_EL0");
}

static inline volatile uint64_t tick_freq(void) {
    return __builtin_arm_rsr64("CNTFRQ_EL0");
}

static inline void tick_init(void) {
    tick_to_ns_mul = 1e9;
    tick_to_ns_div = tick_freq();

    uint64_t g = gcd(tick_to_ns_mul, tick_to_ns_div);

    tick_to_ns_mul /= g;
    tick_to_ns_div /= g;
}

static inline uint64_t tick_to_ns(uint64_t tick) {
    return (tick * tick_to_ns_mul) / tick_to_ns_div;
}

typedef struct {
    const struct timespec delay;
    int loops;
} test;

int main() {
    tick_init();
    uint64_t tt, tp, tq;
    uint64_t td, t0, t1;

    test test_arr[] = {
        { { 0, 1e8 }, 1e1 },
        { { 0, 1e7 }, 1e2 },
        { { 0, 1e6 }, 1e3 },
        { { 0, 1e5 }, 1e4 },
        { { 0, 1e4 }, 1e5 },
        { { 0, 1e3 }, 1e5 },
        { { 0, 1e2 }, 1e5 },
        { { 0, 1e1 }, 1e5 },
        { { 0, 1e0 }, 1e5 },
    };
    int n = sizeof(test_arr) / sizeof(test);

    for (int i = 0; i < n; i++) {
        test test = test_arr[i];
        tt = 0;
        t0 = tick_time();
        for (int j = 0; j < test.loops; j++) {
            struct timespec early = {};
            tp = tick_time();
            nanosleep(&test.delay, &early);
            tq  = tick_time();
            tt += (tq - tp);

        }
        t1 = tick_time();

        tt = tick_to_ns(tt);
        td = tick_to_ns(t1 - t0);

        double sv = 1e-9 * test.delay.tv_nsec;    
        double st = 1e-9 * tt / test.loops;
        double sd = 1e-9 * td / test.loops;

        printf(
            "%.1e :: %.1e ~ %.1e :: %.1e\n",
            sv,
            sd,
            sd - sv,
            sd - st
        );
    }
}
