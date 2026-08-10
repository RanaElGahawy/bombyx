#!/usr/bin/env python3
"""Descriptor invariants the HardCilk Scala generator relies on.

Violations surface downstream as opaque Chisel assertions rather than readable
errors, so they are checked here instead.
"""
import json
import sys


def check(path):
    d = json.load(open(path))
    tasks = {t["name"]: t for t in d["taskDescriptors"]}
    errs = []

    def sides(t):
        return {s["sideType"] for s in t.get("sidesConfigs", [])}

    # Every edge endpoint must itself be a descriptor.
    for key in ("spawnList", "spawnNextList", "sendArgumentList", "mallocList"):
        for src, dsts in d.get(key, {}).items():
            if src not in tasks:
                errs.append(f"{key}: source '{src}' is not a taskDescriptor")
            for dst in dsts:
                if dst not in tasks:
                    errs.append(f"{key}: '{src}' -> '{dst}' is not a taskDescriptor")

    # ArgumentNotifier.scala builds one notifier per sendArgument target and
    # reads its server count from that task's sides.
    for src, dsts in d.get("sendArgumentList", {}).items():
        for dst in dsts:
            if dst in tasks and "argumentNotifier" not in sides(tasks[dst]):
                errs.append(f"sendArgument target '{dst}' has no argumentNotifier side")

    # A spawn_next destination has its continuation closure allocated for it.
    for src, dsts in d.get("spawnNextList", {}).items():
        for dst in dsts:
            if dst in tasks and "allocator" not in sides(tasks[dst]):
                errs.append(f"spawnNext target '{dst}' has no allocator side")

    # The scheduler side must carry the task's own closure width.
    for name, t in tasks.items():
        for s in t.get("sidesConfigs", []):
            if s["sideType"] == "scheduler" and s["portWidth"] != t["widthTask"]:
                errs.append(
                    f"'{name}': scheduler portWidth {s['portWidth']} != widthTask {t['widthTask']}"
                )

    for e in errs:
        print(f"  DESC {e}")
    return not errs


if __name__ == "__main__":
    ok = all(check(p) for p in sys.argv[1:])
    sys.exit(0 if ok else 1)
