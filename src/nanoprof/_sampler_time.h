#ifndef SAMPLER_TIME_H
#define SAMPLER_TIME_H

#include <stdint.h>
#include <time.h>


static uint64_t tick_to_ns_mul = 1;
static uint64_t tick_to_ns_div = 1;
static uint64_t tick_hz = 1e9;

static uint64_t
gcd(uint64_t a, uint64_t b)
{
    uint64_t c;
    while (b)
    {
        c = a % b;
        a = b;
        b = c;
    }
    return a;
}


#ifdef __linux__

static uint64_t
clock_gettime_nsec_np(clockid_t clock_id)
{
    struct timespec ts = {};
    clock_gettime(clock_id, &ts);
    return (uint64_t) 1e9 * ts.tv_sec + ts.tv_nsec;
}

#define CLOCK_HARDWARE CLOCK_MONOTONIC_RAW
#else
#define CLOCK_HARDWARE CLOCK_UPTIME_RAW
#endif

#ifdef __x86_64__

#include <x86intrin.h>

static uint64_t
tick_read(void)
{
    asm volatile("nop");
    return __rdtsc();
}

static uint64_t
tick_freq(void)
{
    return tick_hz;
}

static void
tick_init(void)
{
    uint64_t c0 = clock_gettime_nsec_np(CLOCK_HARDWARE);
    uint64_t t0 = tick_read();
    struct timespec ts = {
        .tv_nsec = 1e8,
    };
    while (nanosleep(&ts, &ts));
    uint64_t c1 = clock_gettime_nsec_np(CLOCK_HARDWARE);
    uint64_t t1 = tick_read();

    double t_hz = 1e9 * (t1 - t0) / (c1 - c0);
    double r_hz = 1e7;

    tick_to_ns_mul = 1e9;
    tick_to_ns_div = tick_hz = (uint64_t) (0.5 + t_hz / r_hz) * r_hz;

    uint64_t t_gcd = gcd(tick_to_ns_mul, tick_to_ns_div);

    tick_to_ns_mul /= t_gcd;
    tick_to_ns_div /= t_gcd;
}

#elif __ARM_ARCH_ISA_A64

#include <arm_acle.h>

static uint64_t
tick_read(void)
{
    asm volatile("nop");
    return __arm_rsr64("CNTVCT_EL0");
}

static uint64_t
tick_freq(void)
{
    return __arm_rsr64("CNTFRQ_EL0");
}

static void
tick_init(void)
{
    tick_to_ns_mul = 1e9;
    tick_to_ns_div = tick_hz = tick_freq();

    uint64_t t_gcd = gcd(tick_to_ns_mul, tick_to_ns_div);

    tick_to_ns_mul /= t_gcd;
    tick_to_ns_div /= t_gcd;
}

#elif 0

static uint64_t
tick_read(void)
{
    return clock_gettime_nsec_np(CLOCK_HARDWARE);
}

static uint64_t
tick_freq(void)
{
    return 1e9;
}

static void
tick_init(void)
{
    tick_to_ns_mul = 1;
    tick_to_ns_div = 1;
    tick_hz = 1e9;
}

#else
#error "Platform not supported."
#endif

static inline uint64_t
tick_time(void)
{
    return tick_read() * tick_to_ns_mul / tick_to_ns_div;
}

#endif
