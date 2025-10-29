#!/usr/bin/env python3

import threading
import time

import nanoprof._sampler

def wait(t):
    time.sleep(t * 10)

def mark(item = None):
    return item

def thread(arg):
    mark(arg)
    wait(0.1 * 10)
    mark(arg)

def main():
    m0 = mark(object())
    m1 = mark(object())
    m2 = mark(object())
    t1 = threading.Thread(
        target = thread,
        args = (m1,)
    )
    t2 = threading.Thread(
        target = thread,
        args = (m2,)
    )
    mark(m0)
    t1.start()
    wait(0.25)
    t1.join()
    wait(0.25)
    mark(m0)
    t2.start()
    wait(0.25)
    t2.join()
    wait(0.25)
    mark(m0)

if __name__ == "__main__":
    q = nanoprof._sampler.inject(None)
    q = nanoprof._sampler.enable(True)
    main()
    r = nanoprof._sampler.finish()
    for i, t in enumerate(r):
        print(f"{i} {t:-16_}")
