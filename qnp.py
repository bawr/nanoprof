#!/usr/bin/env python3

import json
import qnd

FILE = qnd.Qnd.from_file("./profile.qnd")

NODE_TO_PREV_IDX = {}
NODE_TO_CODE_IDX = {
    0: 0,
}
CODE_ADDR_TO_IDX = {
    0: 0,
}
CODE_IDX_TO_INFO = [
    {
        "name": "$",
    },
]
NODE_TO_CODE_LST = [
    [0],
]

JSON_PROFILES = {}
JSON = {
    "$schema": "https://www.speedscope.app/file-format-schema.json",
    "name": "$",
    "shared": {
        "frames": None,
    },
    "profiles": None,
}


if __name__ == "__main__":
    for raw in FILE.rec_list:
        rec = raw.rec
        if isinstance(rec, qnd.Qnd.Code):
            CODE_ADDR_TO_IDX[rec.ptr] = len(CODE_IDX_TO_INFO)
            CODE_IDX_TO_INFO.append({
                "name": rec.qualname.text,
                "file": rec.filename.text,
                "line": rec.line,
            })
        if isinstance(rec, qnd.Qnd.Node):
            node = rec.node_id
            NODE_TO_PREV_IDX[node] = rec.caller_id
            NODE_TO_CODE_IDX[node] = CODE_ADDR_TO_IDX[rec.ptr]
            code_lst = []
            while node:
                code_lst.append(NODE_TO_CODE_IDX[node])
                node = NODE_TO_PREV_IDX[node]
            NODE_TO_CODE_LST.append(code_lst[::-1])
        if isinstance(rec, qnd.Qnd.Emit):
            pass # print(rec.time * 1e-9)
        if isinstance(rec, qnd.Qnd.Time):
            profile = JSON_PROFILES.setdefault(rec.thread_id, {
                "type": "sampled",
                "name": ("THREAD[%d]" % rec.thread_id),
                "unit": "seconds",
                "samples": [],
                "weights": [],
                "startValue": 0,
                "endValue": 30,
            })
            profile["samples"].append(NODE_TO_CODE_LST[rec.node_id])
            profile["weights"].append((rec.time_active + rec.time_paused) * 1e-9)
    JSON["profiles"] = list(JSON_PROFILES.values())
    JSON["shared"]["frames"] = CODE_IDX_TO_INFO
    print(json.dumps(JSON))
