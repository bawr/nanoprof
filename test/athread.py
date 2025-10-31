#!/usr/bin/env python3

import asyncio
import sys
import time

import nanoprof._sampler


async def afirst(t=0.75):
    await asyncio.sleep(t)

async def athread():
    await afirst()
    await asyncio.to_thread(time.sleep, 3),

async def adump():
    await afirst()
    for task in asyncio.all_tasks():
        print(task)
        print(task._coro)
        print(task._fut_waiter)
        print(hex(id(task._fut_waiter)) if task._fut_waiter is not None else None)
        task.print_stack(file=sys.stdout)
        print()

async def main_asyncio():
    await asyncio.gather(
        athread(),
        adump(),
    )

def main():
    asyncio.run(main_asyncio())

if __name__ == "__main__":
    q = nanoprof._sampler.inject(asyncio.tasks._all_tasks.data)
    q = nanoprof._sampler.enable(False)
    main()
    r = nanoprof._sampler.finish()
