#!/usr/bin/env python3

import asyncio
import nanoprof._sampler

def MARK(item=None):
    return item

async def wait(t):
    await asyncio.sleep(t)

async def agen():
    yield 1
    await wait(1.25)
    yield 2
    await wait(1.25)
    yield 3

async def wrap():
    await four()

async def four():
    await wait(1.50)
    await wait(0.75)
    await wait(0.50)
    await wait(0.25)


async def loop():
    MARK()
    async for item in agen():
        MARK(item)
    MARK()
    await wrap()
    MARK()
    await asyncio.gather(wrap(), four())
    MARK()


def main():
    main = MARK(loop())
    f = asyncio.Future
    asyncio.run(main)

if __name__ == "__main__":
    import weakref
    weakref.WeakSet
    q = nanoprof._sampler.inject(asyncio.tasks._all_tasks)
    q = nanoprof._sampler.enable(True)
    main()
    r = nanoprof._sampler.finish()
    for i, t in enumerate(r):
        print(f"{i} {t:-16_}")
