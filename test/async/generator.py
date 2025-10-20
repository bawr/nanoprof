#!/usr/bin/env python3

import asyncio
import nanoprof._sampler

def MARK(item=None):
    return item

def SPIN():
    MARK()
    sum(range(20_000_000))
    MARK()

async def WAIT(t):
    await asyncio.sleep(t)

async def agen():
    await WAIT(1.00)
    yield MARK(0)
    SPIN()
    yield MARK(1)
    await WAIT(1.00)
    yield MARK(2)
    await WAIT(1.00)
    yield MARK(3)

async def loop():
    MARK()
    async for item in agen():
        MARK(item)
    MARK()

def main():
    MARK()
    main = MARK(loop())
    MARK()
    asyncio.run(main)
    MARK()

if __name__ == "__main__":
    q = nanoprof._sampler.inject(asyncio.tasks._all_tasks.data)
    q = nanoprof._sampler.enable(False)
    main()
    r = nanoprof._sampler.finish()
