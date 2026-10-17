#!/usr/bin/env python3
"""Print a failure directory's shrunk case as a trace table with the
model's and the framework's fire steps, so a human can decide which side
is wrong from the LRM.  Usage: python3 triage.py failures/<dir> [...]"""
import json, os, sys, tempfile, subprocess, shutil
HERE = os.path.dirname(os.path.abspath(__file__)); sys.path.insert(0, HERE)
import pbt, ref_sva, pbt_gen

def table(tr):
    names = ["a", "b", "c", "d", "rst", "r", "cnt"]
    print("   step: " + " ".join("%2d" % k for k in range(tr.T)))
    for n in names:
        print("   %-5s " % n + " ".join("%2d" % tr.sigs[n][k] for k in range(tr.T)))

def main():
    for d in sys.argv[1:]:
        notes = json.load(open(os.path.join(d, "notes.json")))
        print("=" * 78); print(d); print("class:", notes["class"], "|", notes["detail"])
        sv = os.path.join(d, "shrunk.sv") if os.path.exists(os.path.join(d, "shrunk.sv")) else os.path.join(d, "case.sv")
        prop = [l for l in open(sv) if "assert property" in l][0].strip()
        print("prop:", prop)
        stims = sorted(f for f in os.listdir(d) if f.startswith("shrunk_stim")) or sorted(f for f in os.listdir(d) if f.startswith("stim"))
        wd = tempfile.mkdtemp()
        shutil.copy(sv, os.path.join(wd, "case.sv"))
        for f in stims:
            key = "shrunk_model_fires_" + f[len("shrunk_stim"):-3] if f.startswith("shrunk") else "model_fires_" + f[4:-3]
            yw = json.load(open(os.path.join(d, f)))
            T = len(yw["steps"])
            tr = pbt.trace_from_yw(os.path.join(d, f), T)
            shutil.copy(os.path.join(d, f), os.path.join(wd, f))
            open(os.path.join(wd, "sim.ys"), "w").write(pbt.SIM_SCRIPT.format(yw=f, summary="sum.json"))
            r = subprocess.run([pbt.YOSYS, "-q", "sim.ys"], cwd=wd, capture_output=True, text=True)
            fired = sorted(set(a["step"] for a in json.load(open(os.path.join(wd, "sum.json")))["assertions"])) if os.path.exists(os.path.join(wd, "sum.json")) else "ERR " + r.stderr[-100:]
            print(" trace %s: model fires %s, sim fires %s" % (f, notes.get(key), fired))
            table(tr)
        if os.path.exists(os.path.join(d, "cex.yw")):
            yw = json.load(open(os.path.join(d, "cex.yw")))
            print(" cex.yw signals:", [(s["path"][-1], s["width"], s.get("init_only")) for s in yw["signals"]], "steps:", len(yw["steps"]))
            print(" first step bits:", yw["steps"][0]["bits"], " second:", yw["steps"][1]["bits"] if len(yw["steps"]) > 1 else "")
            T = len(yw["steps"])
            ctr = pbt.trace_from_yw(os.path.join(d, "cex.yw"), T)
            shutil.copy(os.path.join(d, "case.sv"), os.path.join(wd, "case.sv"))
            shutil.copy(os.path.join(d, "cex.yw"), os.path.join(wd, "cex.yw"))
            open(os.path.join(wd, "sim.ys"), "w").write(pbt.SIM_SCRIPT.format(yw="cex.yw", summary="sum2.json"))
            r = subprocess.run([pbt.YOSYS, "-q", "sim.ys"], cwd=wd, capture_output=True, text=True)
            fired = sorted(set(a["step"] for a in json.load(open(os.path.join(wd, "sum2.json")))["assertions"])) if os.path.exists(os.path.join(wd, "sum2.json")) else "ERR"
            print(" cex replay: sim fires %s; model on decoded cex: see table" % fired)
            table(ctr)
        shutil.rmtree(wd)

main()
