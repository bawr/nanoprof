#!/usr/bin/env python

import argparse
import asyncio

import nanoprof._sampler


def main(path):
    text = open(path).read()
    code = compile(text, path, "exec")
    data = {
        "__name__": "__main__",
    }
    nanoprof._sampler.framer()
    exec(code, data)
    nanoprof._sampler.finish()


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("path", nargs=1)
    args = parser.parse_args()
    main(args.path[0])
