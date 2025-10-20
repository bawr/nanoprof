#!/usr/bin/env python3

import asyncio
import contextlib

import nanoprof._sampler

def MARK(item=None):
    return item

@contextlib.asynccontextmanager
async def WAITER():
    MARK()
    await WAIT(1.0)
    MARK()
    try:
        yield None
    finally:
        MARK()
        await WAIT(1.0)
        MARK()

async def WAIT(t):
    await asyncio.sleep(t)

async def task():
    async with WAITER():
        await WAIT(1.0)

async def loop():
    await task()

def main():
    main = MARK(loop())
    asyncio.run(main)

if __name__ == "__main__":
    q = nanoprof._sampler.inject(asyncio.tasks._all_tasks.data)
    q = nanoprof._sampler.enable(True)
    main()
    r = nanoprof._sampler.finish()
