#!/usr/bin/env python3

import copy
import json
import qnd

STRING_PTR_TO_IDX = {
    0: 0,
}
CODE_PTR_TO_IDX = {
    0: 0,
}
NODE_ID_TO_CODE_IDX = {
    0: 0,
}
THREAD_ID_TO_THREAD = {}

LAST_TIME = 0
LAST_TIME_PER_THREAD = {}
LAST_LOAD_PER_THREAD = {}

STRING_ARRAY = [
    "$",
]
SOURCES_TABLE = {
    "uuid": [None],
    "filename": [0],
    "length": 1
}
FRAME_TABLE = {
    "address": [0],
    "inlineDepth": [0],
    "category": [0],
    "subcategory": [0],
    "func": [0],
    "nativeSymbol": [None],
    "innerWindowID": [None],
    "line": [None],
    "column": [None],
    "length": 1
}
FUNC_TABLE = {
    "isJS": [False],
    "relevantForJS": [False],
    "name": [0],
    "resource": [-1],
    "source": [0],
    "lineNumber": [None],
    "columnNumber": [None],
    "length": 1
}
STACK_TABLE = {
    "frame": [0],
    "prefix": [None],
    "length": 1
}
SAMPLE_CORE = {
    "stack": [0],
    "time": [0],
#   "threadCPUDelta": [0],
    "weight": [0],
    "weightType": "tracing-ms",
    "length": 1
}
THREAD_CORE = {
    "name": "Thread",
    "isMainThread": ...,
    "processType": "default",
    "processName": "Python",
    "processStartupTime": 0,
    "processShutdownTime": None,
    "registerTime": 0,
    "unregisterTime": None,
    "pid": ...,
    "tid": ...,
    "pausedRanges": [],
    "frameTable": FRAME_TABLE,
    "funcTable": FUNC_TABLE,
    "nativeSymbols": {
        "libIndex": [],
        "address": [],
        "name": [],
        "functionSize": [],
        "length": 0
    },
    "resourceTable": {
        "lib": [],
        "name": [],
        "type": [],
        "length": 0
    },
    "stackTable": STACK_TABLE,
    "markers": {
        "data": [],
        "name": [],
        "startTime": [],
        "endTime": [],
        "phase": [],
        "category": [],
        "length": 0
    },
    "samples": SAMPLE_CORE
}
PROFILE_CORE = {
    "meta": {
        "interval": 0.000001,
        "startTime": ...,
        "sampleUnits": {
            "time": "ms",
            "eventDelay": "ms",
            "threadCPUDelta": "ns"
        },
        "processType": 0,
        "product": "Python",
        "stackwalk": 1,
        "debug": False,
        "version": 32,
        "categories": [
            {
                "name": "Idle",
                "color": "grey",
                "subcategories": [
                    "Other"
                ]
            },
            {
                "name": "Python",
                "color": "green",
                "subcategories": [
                    "Other"
                ]
            }
        ],
        "preprocessedProfileVersion": -58,
        "symbolicated": True,
        "markerSchema": []
    },
    "libs": [],
    "counters": [],
    "shared": {
        "stringArray": STRING_ARRAY,
        "sources": SOURCES_TABLE,
        "frameTable": FRAME_TABLE,
        "funcTable": FUNC_TABLE,
        "stackTable": STACK_TABLE,
    },
    "threads": [],
}


def table_insert(table, values):
    for k, v in values.items():
        table[k].append(v)
    idx = table["length"]
    table["length"] += 1
    return idx

if __name__ == "__main__":
    PROFILE_CORE["meta"]["startTime"] = 0
    FILE = qnd.Qnd.from_file("/tmp/profile.qnd")
    N = 0
    for raw in FILE.rec_list:
        rec = raw.rec
        N += 1
        if isinstance(rec, qnd.Qnd.Utf8):
            STRING_PTR_TO_IDX[rec.ptr] = len(STRING_PTR_TO_IDX)
            STRING_ARRAY.append(rec.text)
        if isinstance(rec, qnd.Qnd.Code):
            CODE_PTR_TO_IDX[rec.ptr] = len(CODE_PTR_TO_IDX)
            source_idx = table_insert(SOURCES_TABLE, {
                "uuid": None,
                "filename": STRING_PTR_TO_IDX[rec.ptr_filename],
            })
            func_idx = table_insert(FUNC_TABLE, {
                "isJS": False,
                "relevantForJS": False,
                "name": STRING_PTR_TO_IDX[rec.ptr_qualname],
                "resource": -1,
                "source": source_idx,
                "lineNumber": rec.line,
                "columnNumber": None,
            })
            frame_idx = table_insert(FRAME_TABLE, {
                "address": rec.ptr,
                "inlineDepth": 0,
                "category": 1,
                "subcategory": 0,
                "func": func_idx,
                "nativeSymbol": None,
                "innerWindowID": None,
                "line": rec.line,
                "column": None,
            })
        if isinstance(rec, qnd.Qnd.Node):
            NODE_ID_TO_CODE_IDX[rec.node_id] = CODE_PTR_TO_IDX[rec.ptr]
            stack_id = table_insert(STACK_TABLE, {
                "frame": CODE_PTR_TO_IDX[rec.ptr],
                "prefix": rec.caller_id,
            })
            assert rec.node_id == stack_id
        if isinstance(rec, qnd.Qnd.Pthr):
            if (rec.thread_id not in THREAD_ID_TO_THREAD):
                thread = THREAD_ID_TO_THREAD[rec.thread_id] = THREAD_CORE.copy()
                PROFILE_CORE["threads"].append(thread)
                thread["isMainThread"] = len(THREAD_ID_TO_THREAD) == 1
                thread["pid"] = 666
                thread["tid"] = rec.thread_id
                thread["samples"] = copy.deepcopy(SAMPLE_CORE)
            else:
                ...
            LAST_TIME_PER_THREAD[rec.thread_id] = LAST_TIME
        if isinstance(rec, qnd.Qnd.Mark):
            LAST_TIME = rec.time / 1e9
        if isinstance(rec, qnd.Qnd.Time):
            thread = THREAD_ID_TO_THREAD[rec.thread_id]
            stack_id = rec.node_id
            timer = (rec.time_active + rec.time_paused + rec.time_waited) / 1e9
            table_insert(thread["samples"], {
                "stack": stack_id,
                "time": LAST_TIME_PER_THREAD[rec.thread_id] * 1000,
#               "threadCPUDelta": ...,
                "weight": timer * 1000,
            })
            LAST_TIME_PER_THREAD[rec.thread_id] += timer
    for thread in PROFILE_CORE["threads"]:
        thread["funcTable"] = None
        thread["frameTable"] = None
        thread["stackTable"] = None
    PROFILE_CORE["shared"]["funcTable"] = FUNC_TABLE
    PROFILE_CORE["shared"]["frameTable"] = FRAME_TABLE
    PROFILE_CORE["shared"]["stackTable"] = STACK_TABLE
    print(json.dumps(PROFILE_CORE))
