"""
Random PDPTW pricing instances: PathWyse (pairs, forward search) against brute force.

Run from any directory (pathwyse.set next to this file is copied to a temporary working directory):
    python test_random.py <instances> <capacity> <seed> [--algo exact|dom|queue] [--nsol K]
        [--min-req 2] [--max-req 4] [--slack 8] [--pi-max 45]

exact: the best objective must equal the brute-force optimum.
dom, queue: heuristics; every returned route must be valid.
In all modes every returned route must be feasible and its recomputed objective must match
the objective PathWyse reports.
"""
import argparse
import itertools
import os
import random
import shutil
import sys
import tempfile

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
BIN = os.path.join(ROOT, "bin")
if sys.platform.startswith("win"):
    os.add_dll_directory(BIN)
    gpp = shutil.which("g++")
    if gpp:
        os.add_dll_directory(os.path.dirname(gpp))
sys.path.insert(0, BIN)
from wrapper import PWSolver   # noqa: E402

TOL = 1e-6
BIG = 10 ** 6


def make_instance(rng, n, Q, slack=8, pi_max=45):
    """Nodes: 0 origin, 1..n pickups, n+1..2n deliveries, 2n+1 destination."""
    N = 2 * n + 2
    dest = N - 1
    pts = [(rng.randint(0, 20), rng.randint(0, 20)) for _ in range(N)]
    pts[dest] = pts[0]
    tt = [[abs(pts[i][0] - pts[j][0]) + abs(pts[i][1] - pts[j][1]) for j in range(N)] for i in range(N)]
    service = [0] + [rng.randint(0, 3) for _ in range(2 * n)] + [0]
    demand = [0] * N
    for r in range(1, n + 1):
        q = rng.randint(1, 3)
        demand[r], demand[n + r] = q, -q
    pi = [0] * N
    for r in range(1, n + 1):
        pi[r] = rng.randint(0, pi_max)
    init = rng.randint(0, 20)

    # windows around a hidden route that respects precedence
    events = list(range(1, n + 1))
    rng.shuffle(events)
    seq, opened = [], []
    pend = events[:]
    while pend or opened:
        if pend and (not opened or rng.random() < 0.5):
            r = pend.pop()
            seq.append(r)
            opened.append(r)
        else:
            r = opened.pop(rng.randrange(len(opened)))
            seq.append(n + r)
    lb, ub = [0] * N, [BIG] * N
    t, cur = 0, 0
    for v in seq:
        t = t + service[cur] + tt[cur][v]
        lb[v] = max(0, t - rng.randint(0, slack))
        ub[v] = t + rng.randint(0, slack)
        cur = v
    return {"n": n, "N": N, "dest": dest, "tt": tt, "service": service, "demand": demand,
            "pi": pi, "init": init, "lb": lb, "ub": ub, "Q": Q}


def evaluate(inst, nodes):
    """(feasible, objective incl. init cost) of origin -> nodes -> destination."""
    n, dest = inst["n"], inst["dest"]
    tt, s = inst["tt"], inst["service"]
    path = [0] + list(nodes) + [dest]
    if len(set(nodes)) != len(nodes):
        return False, None
    t, load, cost, opened = 0, 0, inst["init"], set()
    for i, j in zip(path, path[1:]):
        t = max(t + s[i] + tt[i][j], inst["lb"][j])
        if t > inst["ub"][j]:
            return False, None
        cost += tt[i][j] + (-inst["pi"][j] if 1 <= j <= n else 0)
        load += inst["demand"][j]
        if load > inst["Q"] or load < 0:
            return False, None
        if 1 <= j <= n:
            opened.add(j)
        elif n < j <= 2 * n:
            if (j - n) not in opened:
                return False, None
            opened.discard(j - n)
    if opened:
        return False, None
    return True, cost


def brute_force(inst):
    """Best objective over all feasible routes (the empty route included)."""
    n = inst["n"]
    best = [None]

    def rec(route, remaining_p, opened):
        ok, cost = evaluate(inst, route) if not opened else (False, None)
        if ok and (best[0] is None or cost < best[0]):
            best[0] = cost
        # extend by an unvisited pickup or a delivery of an open request, with pruning
        for p in remaining_p:
            new = route + [p]
            if partial_feasible(inst, new):
                rec(new, remaining_p - {p}, opened | {p})
        for p in opened:
            new = route + [n + p]
            if partial_feasible(inst, new):
                rec(new, remaining_p, opened - {p})

    rec([], set(range(1, n + 1)), set())
    return best[0]


def partial_feasible(inst, nodes):
    tt, s = inst["tt"], inst["service"]
    t, load, prev = 0, 0, 0
    for j in nodes:
        t = max(t + s[prev] + tt[prev][j], inst["lb"][j])
        load += inst["demand"][j]
        if t > inst["ub"][j] or load > inst["Q"]:
            return False
        prev = j
    return True


def build_solver(inst, algo, nsol):
    N, n, dest = inst["N"], inst["n"], inst["dest"]
    pw = PWSolver()
    pw.createProblem()
    pw.setProblemName("rand")
    pw.setNumNodes(N)
    pw.setDirected(True)
    pw.setCyclic(True)
    pw.setOrigin(0)
    pw.setDestination(dest)
    pw.initProblem()
    tw = pw.addResource("TW")      # resource 0 is the critical resource
    cap = pw.addResource("CAP")
    pw.setResBounds(cap, 0, inst["Q"])
    for i in range(N):
        pw.setResNodeBound(tw, i, inst["lb"][i], inst["ub"][i])
        pw.setResNodeConsumption(tw, i, inst["service"][i])
        pw.setResNodeConsumption(cap, i, inst["demand"][i])
    for i in range(N):
        if i == dest:
            continue
        for j in range(N):
            if j == i or j == 0:
                continue
            pw.addArc(i, j)
            pw.setArcCost(i, j, inst["tt"][i][j])
            pw.setResArcConsumption(tw, i, j, inst["tt"][i][j])
    for i in range(1, n + 1):
        pw.setNodeCost(i, -inst["pi"][i])
        assert pw.setPair(i, n + i) == 1
    pw.setInitCost(inst["init"])
    pw.buildProblem()
    pw.setupAlgorithms()
    pw.disableAllAlgorithms()
    pw.enableAlgorithm({"exact": 0, "dom": 1, "queue": 2}[algo])
    return pw


def run_one(inst, algo, nsol):
    pw = build_solver(inst, algo, nsol)
    pw.solve()
    sols = []
    for k in range(pw.getNumberOfSolutions()):
        tour = pw.getSolutionTour(k)
        nodes = [v for v in tour if v not in (0, inst["dest"])]
        sols.append((nodes, pw.getSolutionObjective(k)))
    return sols


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("instances", type=int)
    ap.add_argument("capacity", type=int)
    ap.add_argument("seed", type=int)
    ap.add_argument("--algo", default="exact", choices=["exact", "dom", "queue"])
    ap.add_argument("--nsol", type=int, default=1)
    ap.add_argument("--min-req", type=int, default=2)
    ap.add_argument("--max-req", type=int, default=4)
    ap.add_argument("--slack", type=int, default=8, help="time window slack around the hidden route")
    ap.add_argument("--pi-max", type=int, default=45, help="largest pickup reward (large values make cycles attractive)")
    a = ap.parse_args()

    # PathWyse reads pathwyse.set from the working directory
    here = os.path.dirname(os.path.abspath(__file__))
    settings = open(os.path.join(here, "pathwyse.set")).read()
    assert "algo/requested_solutions = 1" in settings
    settings = settings.replace("algo/requested_solutions = 1", f"algo/requested_solutions = {a.nsol}")
    tmp = tempfile.TemporaryDirectory()
    with open(os.path.join(tmp.name, "pathwyse.set"), "w") as f:
        f.write(settings)
    start_dir = os.getcwd()
    os.chdir(tmp.name)

    rng = random.Random(a.seed)
    agree = invalid = obj_mismatch = missing = 0
    for it in range(a.instances):
        n = rng.randint(a.min_req, a.max_req)
        inst = make_instance(rng, n, a.capacity, a.slack, a.pi_max)
        opt = brute_force(inst)
        sols = run_one(inst, a.algo, a.nsol)
        bad = False
        for nodes, pw_obj in sols:
            ok, cost = evaluate(inst, nodes)
            if not ok:
                invalid += 1
                bad = True
                print(f"instance {it}: INVALID route {nodes}")
            elif abs(cost - pw_obj) > TOL:
                obj_mismatch += 1
                bad = True
                print(f"instance {it}: objective {pw_obj} reported, {cost} recomputed for {nodes}")
        if a.algo == "exact":
            best = min((evaluate(inst, nodes)[1] for nodes, _ in sols if evaluate(inst, nodes)[0]), default=None)
            if best is None or abs(best - opt) > TOL:
                missing += 1
                bad = True
                print(f"instance {it}: n={n} PathWyse best {best}, brute force {opt}")
        if not bad:
            agree += 1
    os.chdir(start_dir)     # Windows cannot delete the working directory
    tmp.cleanup()
    print(f"{agree} of {a.instances} instances ok  (invalid routes {invalid}, "
          f"objective mismatches {obj_mismatch}, not optimal {missing})")


if __name__ == "__main__":
    main()
