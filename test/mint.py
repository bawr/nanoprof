#!/usr/bin/env python3

import threading
import time

import nanoprof._sampler

def wait(t):
    time.sleep(t)

def spin(n):
    s = 0
    for i in range(n):
        s += mark(i)

def mark(item = None):
    return item

def thread(t, n):
    wait(t)
    spin(n)

def main():
    for i in range(2):
        threads = [
            threading.Thread(target = thread, args = (0.25, 75_000_000)),
            threading.Thread(target = thread, args = (0.50, 50_000_000)),
            threading.Thread(target = thread, args = (0.75, 25_000_000)),
            threading.Thread(target = thread, args = (1.00,          0)),
        ]
        for t in threads:
            t.start()
        wait(0.25)
        for t in threads:
            t.join()

if __name__ == "__main__":
    q = nanoprof._sampler.inject(None)
    q = nanoprof._sampler.enable(
        False,
        1e-3,
        1e-4,
    )
    main()
    r = nanoprof._sampler.finish()
