#!/usr/bin/env python3

import threading
import time

import nanoprof._sampler


def mark(item = None):
    return item

def thread(arg):
    mark(arg)
    total = 0
    while True:
        total += 1
        if (total % 1e6 == 0):
            mark(arg)
        if (total % 1e8 == 0):
            break
    return arg

def main():
    m0 = mark(object())
    m1 = mark(object())
    m2 = mark(object())
    t1 = threading.Thread(
        target = thread,
        args = (m1,),
    )
    t2 = threading.Thread(
        target = thread,
        args = (m2,),
    )
    mark(m0)
    t1.start()
    t2.start()
    mark(m0)
    t1.join()
    t2.join()
    mark(m0)
    time.sleep(1.1)
    mark(m0)
    time.sleep(1.1)
    mark(m0)

if __name__ == "__main__":
    q = nanoprof._sampler.inject()
    q = nanoprof._sampler.enable(True)
    main()
    r = nanoprof._sampler.finish()
    for i, t in enumerate(r):
        print(f"{i} {t:-16_}")
