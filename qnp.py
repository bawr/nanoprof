#!/usr/bin/env python3

import qnd

FILE = qnd.Qnd.from_file("./test/profile.qnd")
CODE = {}
TREE = {}

def print_time(trec: qnd.Qnd.Time):
    trep = []
    nrec = TREE.get(trec.node_id)
    while nrec:
        trep.append("%s:%s:%d" % CODE[nrec[2]])
        nrec = TREE.get(nrec[1])
    stem = ";".join(trep[::-1])
    if trec.time_active:
        print("%s;THREAD:%d;ACTIVE %d" % (stem, trec.thread_id, trec.time_active * 1e-6))
    if trec.time_paused:
        print("%s;THREAD:%d;PAUSED %d" % (stem, trec.thread_id, trec.time_paused * 1e-6))

if __name__ == "__main__":
    for raw in FILE.rec_list:
        rec = raw.rec
        if isinstance(rec, qnd.Qnd.Code):
            CODE[rec.ptr] = (rec.filename.text, rec.qualname.text, rec.line)
        if isinstance(rec, qnd.Qnd.Node):
            TREE[rec.node_id] = (rec.node_id, rec.caller_id, rec.ptr)
        if isinstance(rec, qnd.Qnd.Emit):
            pass # print(rec.time * 1e-9)
        if isinstance(rec, qnd.Qnd.Time):
            print_time(rec)
