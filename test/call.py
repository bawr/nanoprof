#!/usr/bin/env python3

import time
import nanoprof._sampler

def main():
    time.sleep(5.0)

if __name__ == "__main__":
    q = nanoprof._sampler.inject(set())
    q = nanoprof._sampler.enable(True)
    main()
    r = nanoprof._sampler.finish()
    for i, t in enumerate(r):
        print(f"{i} {t:-16_}")
