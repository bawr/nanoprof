#!/usr/bin/env python3

import asyncio
import contextvars
import time

import nanoprof._sampler
C = contextvars.ContextVar("C", default=0)


def threaded(n: int):
    time.sleep(0.1)
    print(n, C.get(), nanoprof._sampler.cstack())

async def anested(p: int, q: int):
    C.set(1)
    await asyncio.gather(
        acontext(p),
        acontext(q),
    )

async def acontext(n: int):
    await asyncio.sleep(0.1)
    print(n, C.get(), nanoprof._sampler.cstack())

async def main_asyncio():
    print(1, C.get(), nanoprof._sampler.cstack())
    await asyncio.gather(
        acontext(2),
        acontext(3),
        anested(4, 5),
        asyncio.to_thread(threaded, 9),
    )

def main():
    print(0, C.get(), nanoprof._sampler.cstack())
    asyncio.run(main_asyncio())

if __name__ == "__main__":
    main()
