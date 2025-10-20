#!/usr/bin/env python3

import asyncio
import sys
import nanoprof._sampler

def MARK(item=None):
    return item

def SPIN():
    sum(range(10_000_000))

async def WAIT(t):
    SPIN()
    await asyncio.sleep(t)

async def task():
    await WAIT(1.0)
    await WAIT(1.0)

async def task0():
    await task()

async def task1():
    await task()

async def task2():
    await task()

async def loop():
    await asyncio.wait_for(task0(), 1024)
    await asyncio.gather(task1(), task2())

def main():
    asyncio.run(loop())

if __name__ == "__main__":
    q = nanoprof._sampler.inject(asyncio.tasks._all_tasks.data)
    q = nanoprof._sampler.enable(False)
    main()
    r = nanoprof._sampler.finish()
