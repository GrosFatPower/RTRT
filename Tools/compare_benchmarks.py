#!/usr/bin/env python3

import json
import sys


def load(path):
    with open(path, "r", encoding="utf-8") as stream:
        return json.load(stream)


def metric(result, section, name):
    value = result.get(section, {}).get(name, {})
    if not value:
        value = result.get(section + "_ms", {}).get(name)
    if isinstance(value, dict):
        return value.get("median_ms", value.get("mean_ms"))
    return value


def print_delta(name, before, after):
    if before is None or after is None:
        return
    delta = after - before
    percent = delta * 100.0 / before if before else 0.0
    print(f"{name:32} {before:10.3f} -> {after:10.3f} ms  {delta:+9.3f} ms  {percent:+7.2f}%")


def main():
    if len(sys.argv) != 3:
        print("usage: compare_benchmarks.py BEFORE.json AFTER.json")
        return 2

    before = load(sys.argv[1])
    after = load(sys.argv[2])
    print_delta(
        "CPU frame",
        before.get("samples", {}).get("cpu_frame", {}).get("median_ms", before.get("samples", {}).get("cpu_frame_average_ms")),
        after.get("samples", {}).get("cpu_frame", {}).get("median_ms", after.get("samples", {}).get("cpu_frame_average_ms")),
    )

    for section in ("cpu_timings", "renderer_timings"):
        print(f"\n{section.replace('_', ' ').title()}")
        names = sorted(
            set(before.get(section, {}))
            | set(after.get(section, {}))
            | set(before.get(section + "_ms", {}))
            | set(after.get(section + "_ms", {}))
        )
        for name in names:
            print_delta(name, metric(before, section, name), metric(after, section, name))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
