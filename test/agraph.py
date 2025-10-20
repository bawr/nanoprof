import asyncio


async def concurrent_graph():
    await asyncio.sleep(0.5)
    for task in asyncio.all_tasks():
        asyncio.print_call_graph(task)


async def nested_coroutine():
    await asyncio.sleep(1.0)
    print("Nested coroutine finished.")

async def main_coroutine():
    print("Main coroutine started.")
    task = asyncio.create_task(nested_coroutine())
    done = asyncio.gather(task, concurrent_graph())
    await done
    print("Main coroutine finished.")

async def entry_point():
    await main_coroutine()
    print("Printing call graph for current task:")
    asyncio.print_call_graph()

asyncio.run(entry_point())
