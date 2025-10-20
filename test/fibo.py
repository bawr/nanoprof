#!/usr/bin/env python3

import argparse
import concurrent.futures
import time

import nanoprof._sampler


def fibo(n: int) -> int:
    t = 0
    for i in range(n):
        t += i
    if n < 0:
        raise ValueError()
    elif n <= 1:
        return n
    else:
        fib_1 = fibo(n - 1)
        fib_2 = fibo(n - 2)
        fib_n = fib_1 + fib_2
        return fib_n


def main(n: int, t: int, r: int, s: str):
    t0 = time.clock_gettime_ns(time.CLOCK_MONOTONIC)
    if (s == "fast"):
        nanoprof._sampler.enable(False)
    if (s == "slow"):
        nanoprof._sampler.enable(True)
    for i in range(r):
        if (t <= 1):
            print(i, 0, n, fibo(n))
        else:
            with concurrent.futures.ThreadPoolExecutor(t) as pool:
                results = pool.map(fibo, [n] * t)
                for j, f in enumerate(results):
                    print(i, j, n, f)
    if (s != "none"):
        print()
        p = nanoprof._sampler.finish()
        for i, t in enumerate(p):
            print(f"{i} {t:-16_}")
    print()
    t1 = time.clock_gettime_ns(time.CLOCK_MONOTONIC)
    print(f"T {t1 - t0:-16_}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("-n", type=int, default=33, help="number")
    parser.add_argument("-t", type=int, default=1,  help="thread_count")
    parser.add_argument("-r", type=int, default=5,  help="repeat_count")
    parser.add_argument("-s", choices=("fast", "slow", "noop", "none"), default="none", help="sample_type")
    args = parser.parse_args()
    if (args.s != "none"):
        nanoprof._sampler.inject()
    main(args.n, args.t, args.r, args.s)
