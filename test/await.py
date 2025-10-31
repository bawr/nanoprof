#!/usr/bin/env python3

import asyncio
import sys
import time

import nanoprof._sampler


async def afirst(t=0.75):
    await asyncio.sleep(t)

async def asleep(t=0.75):
    await asyncio.sleep(t)

async def afunc2a():
    await afirst()
    await afunc1ap()
    await afunc1aq()

async def afunc1ap():
    await asleep()

async def afunc1aq():
    await asleep()
    await asyncio.to_thread(time.sleep, 3),

async def afunc2b():
    await afirst()
    await asyncio.gather(
        afunc1bp(),
        afunc1bq(),
    )

async def afunc1bp():
    await asleep()

async def afunc1bq():
    await asleep()

async def afunc2c():
    await afirst()
#   return
    for task in asyncio.all_tasks():
        print(task)
        print(task._coro)
        print(task._fut_waiter)
        print(hex(id(task._fut_waiter)) if task._fut_waiter is not None else None)
        task.print_stack(file=sys.stdout)
        print()

async def main_asyncio(ctrace):
    acoro2a = afunc2a()
    acoro2b = afunc2b()
    acoro2c = afunc2c() if not ctrace else asleep()
    atask2a = asyncio.create_task(acoro2a)
    atask2b = asyncio.create_task(acoro2b)
    atask2c = asyncio.create_task(acoro2c)
    await asyncio.wait((
        atask2a,
        atask2b,
        atask2c,
    ))

def main(ctrace):
    asyncio.run(main_asyncio(ctrace))

if __name__ == "__main__":
    t = False
    q = nanoprof._sampler.inject(asyncio.tasks._all_tasks.data)
    q = nanoprof._sampler.enable(t)
    main(t)
    r = nanoprof._sampler.finish()
    for i, t in enumerate(r):
        print(f"{i} {t:-16_}")
