#!/usr/bin/env python3

import argparse
import time

import nanoprof._sampler

def gen1(n: int):
    i = 0
    while i < n:
        yield i
        i += 1

def gen2(n: int):
    i = 0
    while i < n:
        yield i
        i += 1
    mark_raise()
    raise EOFError()

def gen3(g):
    try:
        mark_try()
        yield from g
    except EOFError:
        mark_except()
    finally:
        mark_finally()

def mark():
    pass

def mark_raise():
    pass

def mark_try():
    pass

def mark_except():
    pass

def mark_finally():
    pass

def walk(g):
    return sum(1 for _ in g)

def nest(n: int):
    return n

def call(n1: int, n2: int) -> int:
    n1 = nest(n1)
    n2 = nest(n2)
    return n1 + n2

def main():
    call(1, 2)
    call(2, 3)
    walk(gen1(2))
    walk(gen3(gen2(3)))

if __name__ == "__main__":
    q = nanoprof._sampler.inject()
    q = nanoprof._sampler.enable(True)
    main()
    r = nanoprof._sampler.finish()
    for i, t in enumerate(r):
        print(f"{i} {t:-16_}")
