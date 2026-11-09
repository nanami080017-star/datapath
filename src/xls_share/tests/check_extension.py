#!/usr/bin/env python3
"""Run the actual sharing plugin, inspect its netlist and prove its outputs."""
import argparse
import hashlib
import json
import re
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
TESTS = {
    "identity_data": [("lift_identity_unknown", "", "", "identity_unknown")],
    "area_eligibility": [
        ("narrow_share", "", "-rewrite-test", "narrow_share"),
        ("lift_control_operand", "", "", "lift"),
        ("lift_control_unknown", "", "", "lift_unknown"),
    ],
    "multi_batch": [("six_groups", "", "-estimate\nopt_share -rewrite-test", "multi_batch")],
    "unified": [("rewrite_competition", "", "", "unified")],
    "select_chain": [
        ("select_chain_pack", "", "", "packed_chain"),
        ("select_chain_polarity", "", "", "unpacked_chain"),
        ("select_chain_live_middle", "", "", "unpacked_chain"),
        ("select_chain_hidden_x", "", "", "defined_output"),
    ],
    "visibility_extended": [
        ("conditional_pmux", "", "", "conditional"),
        ("conditional_known", "", "", "conditional"),
        ("mask_control_share", "alumacc", "", "trimmed_control"),
    ],
    "selection": [("parallel_inputs", "alumacc", "", "estimate_selection")],
    "cost_accounting": [
        ("cost_repeated_input", "alumacc", "-estimate", "removed=19.710600000 added=21.587800000"),
        ("cost_truncated_product", "wreduce", "-estimate", "delta=-1.407900000"),
    ],
    "estimate": [(n, "alumacc", "-estimate\nopt_share", "estimate") for n in ("macc_sum", "cost_mux_constant")],
    "conditional": [("conditional_edge", "", "", "conditional"), ("conditional_no_imply", "", "", "unchanged")],
    "width": [("width_mul", "", "-rewrite-test", True), ("width_macc", "alumacc", "-rewrite-test", True), ("mixed_add_sub", "", "-rewrite-test", True)],
    "nested": [("lift_nested_identity", "", "", "lift")],
    "lifting": [(n, "", "", "lift") for n in ["lift_commute", "lift_identity_add", "lift_identity_sub"]] + [("lift_noncommute", "", "", "unlifted")],
    "macc": [
        ("macc_product", "alumacc", "-rewrite-test", True),
        ("macc_dependency", "alumacc", "-estimate", "dependency"),
    ],
}

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--plugin", required=True, type=Path)
    ap.add_argument("--out", required=True, type=Path)
    ap.add_argument("--stage", required=True, choices=TESTS)
    args = ap.parse_args()
    args.out.mkdir(parents=True, exist_ok=True)
    results = []
    for name, prepare, action, changed in TESTS[args.stage]:
        before, after = args.out / (name + "_before.json"), args.out / (name + "_after.json")
        observe = {
            "defined_output": "eval -set p 0 -set b 16'h5555 -set d 16'haaaa -show y",
            "lift_unknown": "eval -set a 8'h0 -set b 8'h2 -show y",
            "identity_unknown": "eval -set s 0 -set p 0 -set u 7'b1010101 -set b 4'd2 -show y",
        }.get(changed, "")
        simplify = "" if observe else "opt_expr"
        script = f"""read_verilog {ROOT}/src/xls_share/tests/extension.v
hierarchy -top {name}
proc
{simplify}
opt_clean
{prepare}
opt_clean
rename {name} gold
copy gold gate
select gate
write_json {before}
opt_share {action}
opt_clean
write_json {after}
{observe}
select *
techmap
opt_clean
rename -hide
equiv_make gold gate equiv
hierarchy -top equiv
equiv_simple
equiv_status -assert
"""
        ys, log = args.out / (name + ".ys"), args.out / (name + ".log")
        ys.write_text(script)
        with log.open("w") as out:
            run = subprocess.run([str(ROOT / "tools/local/bin/yosys"), "-Q", "-T",
                                  "-m", str(args.plugin.resolve()), "-s", str(ys)],
                                 cwd=ROOT, stdout=out, stderr=subprocess.STDOUT)
        result = {"test": name, "returncode": run.returncode, "log": str(log)}
        if run.returncode == 0:
            trace = log.read_text()
            a = json.loads(before.read_text())["modules"]["gate"]
            b = json.loads(after.read_text())["modules"]["gate"]
            count = lambda m: sum(c["type"] in ("$macc_v2", "$add", "$sub", "$mul", "$alu")
                                  for c in m["cells"].values())
            result.update(before_ops=count(a), after_ops=count(b))
            if changed == "defined_output":
                result["pass"] = "XLS apply collapse:" not in trace and "16'1010101010101010" in trace
            elif changed == "lift_unknown":
                result["pass"] = "XLS apply lift:" not in trace and "8'000000x0" in trace
            elif changed == "identity_unknown":
                result["pass"] = "XLS apply lift:" not in trace and "8'x1010101" in trace
            elif changed == "narrow_share":
                result["pass"] = "XLS resource candidates=1." in trace and count(a) == 2 and count(b) == 1
            elif args.stage == "cost_accounting":
                estimates = [line for line in trace.splitlines() if line.startswith("XLS estimate candidate:")]
                result["pass"] = len(estimates) == 1 and changed in estimates[0]
                result["estimate"] = estimates
            elif changed == "multi_batch":
                roles = {tuple(a["netnames"][name]["bits"]): name
                         for name in ("da", "db", "dc", "pa", "pb", "pc")}
                cells = {name.lstrip("\\"): roles[tuple(c["connections"]["Y"])]
                         for name, c in a["cells"].items() if c["type"] == "$mul"}
                groups = [sorted(cells[name] for name in line.split())
                          for line in re.findall(r"^XLS estimate group \d+:(.*)$", trace, re.M)]
                expected = [["da", "pa"], ["db", "pb"], ["dc", "pc"],
                            ["pa", "pb"], ["pa", "pc"], ["pb", "pc"], ["pa", "pb", "pc"]]
                result["pass"] = sorted(groups) == sorted(expected) and count(a) == 6 and count(b) == 4
                result["groups"] = groups
            elif changed == "unified":
                first = re.search(r"^XLS area selected=1 delta=(-?\d+(?:\.\d+)?) kind=(\w+)", trace, re.M)
                offers = re.findall(r"^XLS estimate candidate:.*delta=(-?\d+(?:\.\d+)?) kind=(\w+)",
                                    trace[:first.start()] if first else trace, re.M)
                def driver(name, cell_type):
                    bits = b["netnames"][name]["bits"]
                    return any(c["type"] == cell_type and c["connections"]["Y"] == bits
                               for c in b["cells"].values())
                result["pass"] = bool(first) and {kind for _, kind in offers} == {"collapse", "lift", "share"} and first[2] == "share" and float(first[1]) == min(float(value) for value, _ in offers) and ("0.000000000", "lift") in offers and driver("tap", "$add") and driver("live_pick", "$mux")
                result.update(first_selected=first.groups() if first else None, first_offers=offers)
            elif changed == "unlifted":
                result["pass"] = "XLS apply lift:" not in trace
            elif changed == "lift":
                result["pass"] = "XLS apply lift:" in trace and a != b
            elif changed == "estimate_selection":
                estimated = trace.count("XLS estimate candidate:")
                parallel = any(c["type"] == "$pmux" for c in b["cells"].values())
                result["pass"] = estimated > 0 and "Running ABC script:" not in trace and count(a) == 3 and count(b) == 1 and parallel
                result["estimated_candidates"] = estimated
            elif changed == "estimate":
                values = re.findall(r"XLS estimate candidate:.*delta=(-?\d+(?:\.\d+)?)", trace)
                result["pass"] = len(values) >= 2 and values[0] == values[1] and float(values[0]) < 0 and count(b) < count(a) and "Running ABC script:" not in trace
                result["estimated_deltas"] = [float(x) for x in values]
            elif changed == "packed_chain":
                selects = [c["type"] for c in b["cells"].values() if c["type"] in ("$mux", "$pmux")]
                result["pass"] = selects == ["$pmux"] and "XLS apply collapse:" in trace
            elif changed == "unpacked_chain":
                result["pass"] = "XLS apply collapse:" not in trace
            elif changed == "trimmed_control":
                selectors = [c["connections"]["S"] for c in b["cells"].values()
                             if c["type"] in ("$mux", "$pmux")]
                result["pass"] = len(selectors) == 2 and all(
                    bits == b["netnames"]["s"]["bits"] for bits in selectors) and count(b) < count(a)
            elif changed == "conditional":
                result["pass"] = "XLS condition specialize " in trace and a != b
            elif changed == "unchanged":
                result["pass"] = a == b
            elif changed == "dependency":
                result["pass"] = a == b and ": dependency\n" in trace
            else:
                result["pass"] = count(b) < count(a)
        else:
            result["pass"] = False
        results.append(result)
        print(json.dumps(result), flush=True)
    report = {"stage": args.stage, "plugin_sha256": hashlib.sha256(args.plugin.read_bytes()).hexdigest(),
              "tests": results}
    (args.out / "results.json").write_text(json.dumps(report, indent=2) + "\n")
    raise SystemExit(0 if all(r["pass"] for r in results) else 1)

if __name__ == "__main__":
    main()
