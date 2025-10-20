#!/usr/bin/env python3

from typing import Tuple
from typing import TypeAlias
from typing import TypeVar
import multiprocessing
import os

PID: TypeAlias = int
T = TypeVar("T")


def proc(q: multiprocessing.Queue, x: T) -> Tuple[PID, T]:
    v = (os.getpid(), x)
    q.put(v)
    return v


if __name__ == "__main__":
    cx = multiprocessing.get_context("forkserver")
    cx.set_forkserver_preload([])
    pq = cx.Queue(10)
    p1 = cx.Process(None, proc, "p1", (pq, 1))
    p2 = cx.Process(None, proc, "p2", (pq, 2))
    p1.start()
    p2.start()
    p1.join()
    p2.join()
    print(pq.get())
    print(pq.get())
