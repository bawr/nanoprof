#!/usr/bin/env python3

import asyncio
import sys

import nanoprof._sampler


async def adumper():
    await asyncio.sleep(0.25)
    for task in asyncio.all_tasks():
        print(task)
        print(task._coro)
        print(task._fut_waiter)
        print(hex(id(task._fut_waiter)))
        task.print_stack(file=sys.stdout)
        print()

async def awaiter(f: asyncio.Future):
    await f

async def main_asyncio():
    f = asyncio.get_running_loop().create_future()
    await asyncio.wait((
        asyncio.create_task(adumper()),
        asyncio.create_task(awaiter(f)),
    ))

def main():
    asyncio.run(main_asyncio())

if __name__ == "__main__":
    main()
