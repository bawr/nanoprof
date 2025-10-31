#!/usr/bin/env python3

import asyncio
import nanoprof._sampler

def MARK(item=None):
    return item

def TEST():
    pass

async def afunc1():
    MARK()
    await asyncio.sleep(1.5)
    MARK()

async def aerror():
    MARK()
    raise RuntimeError()

async def afunc2():
    MARK()
    await afunc1()
    MARK()
    try:
        await aerror()
    except RuntimeError:
        pass
    MARK()

async def agen():
    yield 1
    await asyncio.sleep(1.25)
    yield 2
    await asyncio.sleep(1.25)
    yield 3

async def sleep():
    await asyncio.sleep(1.50)
    await asyncio.sleep(0.75)
    await asyncio.sleep(0.50)
    await asyncio.sleep(0.25)

async def main_asyncio():
    MARK()
    acoro1 = afunc1()
    MARK()
    await acoro1
    MARK()
    await afunc1()
    MARK()
    await afunc2()
    MARK()
    async for item in agen():
        MARK(item)
    MARK()
    await sleep()
    MARK()

def main():
    asyncio.run(main_asyncio())

if __name__ == "__main__":
    q = nanoprof._sampler.inject(asyncio.tasks._all_tasks.data)
    q = nanoprof._sampler.enable(False)
    main()
    r = nanoprof._sampler.finish()
    for i, t in enumerate(r):
        print(f"{i} {t:-16_}")
