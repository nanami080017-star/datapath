#!/usr/bin/env python3
"""Run the upstream Yosys/ABC flow and report unconstrained CombArea."""
import argparse
import csv
from concurrent.futures import ThreadPoolExecutor, as_completed
from datetime import datetime, timedelta, timezone
import hashlib
import json
import os
from pathlib import Path
from queue import Queue
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parent
DATASET = ROOT / "benchmarks/rtl/MutexShare-Case-Set"
LIBERTY = DATASET / "lib/gscl45nm.lib"
YOSYS = ROOT / "tools/local/bin/yosys"
ABC = ROOT / "tools/abc/abc"
THREADS = 4


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--case", type=int, nargs="+", required=True,
                        help="MutexShare case numbers (1-5000)")
    parser.add_argument("--name", help="Output directory name under run/")
    parser.add_argument("--jobs", type=int, choices=range(1, 11), default=10,
                        help="Maximum concurrent cases (1-10, default: 10)")
    parser.add_argument("--sharing-plugin", type=Path,
                        help="Load the XLS sharing plugin; default uses original opt_share")
    args = parser.parse_args()
    if args.sharing_plugin:
        args.sharing_plugin = args.sharing_plugin.resolve()
        if not args.sharing_plugin.is_file():
            parser.error("--sharing-plugin must name an existing plugin")
    if any(n < 1 or n > 5000 for n in args.case):
        parser.error("--case must be between 1 and 5000")
    now = datetime.now(timezone(timedelta(hours=8)))
    name = args.name if args.name is not None else now.strftime("%Y%m%d_%H%M%S_%f")
    if not name or Path(name).name != name or name in (".", ".."):
        parser.error("--name must be one directory name")
    cases = list(dict.fromkeys(args.case))
    workers = min(args.jobs, len(cases))
    groups = physical_cpu_groups(workers)
    slots = Queue()
    for cpus in groups:
        slots.put(cpus)
    output = ROOT / "run" / name
    output.mkdir(parents=True)

    raw_env = subprocess.check_output(
        ["bash", "-c", 'source "$1" >/dev/null && env -0',
         "bash", str(ROOT / "tools/env.sh")])
    env = dict(item.split("=", 1) for item in raw_env.decode().rstrip("\0").split("\0"))
    env["YOSYS_MAX_THREADS"] = str(THREADS)
    inputs = [LIBERTY] + [
        DATASET / "case" / f"case_{n:04d}" / "tc.v" for n in cases]
    repos = ["tools/yosys", "tools/abc", "benchmarks/rtl/MutexShare-Case-Set"]
    manifest = {
        "flow": "upstream Yosys synth -noabc and default ABC Liberty mapping",
        "created": now.isoformat(),
        "arguments": sys.argv[1:],
        "resources": {"max_concurrent_cases": args.jobs,
                      "YOSYS_MAX_THREADS": THREADS, "physical_cpu_groups": groups},
        "runner_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        "versions": {
            "yosys": subprocess.check_output(
                [str(YOSYS), "-V"], env=env, cwd=output, text=True).strip(),
            "abc": subprocess.check_output(
                [str(ABC), "-c", "version"], env=env, cwd=output, text=True).strip(),
        },
        "commits": {repo: subprocess.check_output(
            ["git", "rev-parse", "HEAD"], cwd=ROOT / repo, text=True).strip()
            for repo in repos},
        "binary_sha256": {str(p.relative_to(ROOT)):
            hashlib.sha256(p.read_bytes()).hexdigest() for p in (YOSYS, ABC)},
        "inputs_sha256": {str(p.relative_to(ROOT)):
            hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs},
    }
    if args.sharing_plugin:
        source = ROOT / "src/xls_share"
        manifest["sharing_plugin"] = {
            "implementation": "XLS RTLIL adaptation, local characterized-area deltas; one final ABC mapping",
            "path": str(args.sharing_plugin),
            "sha256": hashlib.sha256(args.sharing_plugin.read_bytes()).hexdigest(),
            "project_commit": subprocess.check_output(
                ["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip(),
            "source_sha256": {
                str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest()
                for p in sorted(source.iterdir())
                if p.suffix in (".cc", ".h", ".py") or p.name == "Makefile"},
        }
    (output / "run.json").write_text(json.dumps(manifest, indent=2) + "\n")
    with (output / "results.csv").open("w", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=["case", "CombArea", "elapsed_s"])
        writer.writeheader()
        with ThreadPoolExecutor(max_workers=workers) as executor:
            futures = [executor.submit(run_case, number, output, env, slots, args.sharing_plugin)
                       for number in cases]
            for future in as_completed(futures):
                result = future.result()
                writer.writerow(result)
                stream.flush()
                print(f"{result['case']}: CombArea={result['CombArea']:.6f}, "
                      f"elapsed={result['elapsed_s']:.3f}s", flush=True)
    print(f"Results: {output}", flush=True)


def physical_cpu_groups(workers):
    allowed = os.sched_getaffinity(0)
    topology = subprocess.check_output(
        ["lscpu", "-p=CPU,CORE,SOCKET,ONLINE"], text=True)
    physical = {}
    for cpu, core, socket, online in csv.reader(
            line for line in topology.splitlines() if not line.startswith("#")):
        if online == "Y" and int(cpu) in allowed:
            physical.setdefault((socket, core), int(cpu))
    cpus = list(physical.values())
    if len(cpus) < workers * THREADS:
        raise ValueError(f"{workers} concurrent cases require "
                         f"{workers * THREADS} physical cores; {len(cpus)} available")
    return [cpus[i:i + THREADS] for i in range(0, workers * THREADS, THREADS)]


def run_case(number, output, env, slots, plugin):
    case = f"case_{number:04d}"
    folder = output / case
    folder.mkdir()
    rtl = DATASET / "case" / case / "tc.v"
    script = folder / "flow.ys"
    script.write_text(
        f'read_verilog "{rtl}"\n'
        "synth -top tc -noabc\n"
        f'abc -exe "{ABC}" -liberty "{LIBERTY}"\n'
        "clean\n"
        "write_verilog netlist.v\n"
        f'tee -o area.json stat -json -liberty "{LIBERTY}"\n')
    cpus = slots.get()
    command = ["taskset", "--cpu-list", ",".join(map(str, cpus)),
               str(YOSYS)]
    if plugin:
        command.extend(["-m", str(plugin)])
    command.extend(["-s", str(script)])
    start = time.monotonic()
    try:
        with (folder / "yosys.log").open("w") as log:
            subprocess.run(command, cwd=folder, env=env, stdout=log,
                           stderr=subprocess.STDOUT, check=True)
        end = time.monotonic()
        (folder / "execution.json").write_text(json.dumps({
            "command": command, "physical_cpus": cpus,
            "YOSYS_MAX_THREADS": THREADS,
            "started_monotonic_s": start, "finished_monotonic_s": end,
        }, indent=2) + "\n")
        area = json.loads((folder / "area.json").read_text())["design"]
        return {"case": case,
                "CombArea": area["area"] - area["sequential_area"],
                "elapsed_s": end - start}
    finally:
        slots.put(cpus)


if __name__ == "__main__":
    main()
