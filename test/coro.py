#!/usr/bin/env python3

import nanoprof._sampler

async def coro():
    pass

def main():
    coro()

if __name__ == "__main__":
    q = nanoprof._sampler.inject(set())
    q = nanoprof._sampler.enable(False)
    main()
    r = nanoprof._sampler.finish()
    for i, t in enumerate(r):
        print(f"{i} {t:-16_}")
