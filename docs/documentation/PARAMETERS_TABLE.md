# Parameters

We provide a configuration file for PathWyse in the root directory: **pathwyse.set**. See [Suggested Settings](SUGGESTED_SETTINGS.md) for the recommended configuration on cyclic and acyclic networks.

If the file is present in the folder where PathWyse is run, it will override the library default configuration. Note that this is the current working directory of the process, not the folder containing the executable: running a binary built under a `build/` folder from within that folder will fall back to the defaults unless `pathwyse.set` is also placed there (or the binary is run from the folder that contains it), showing a warning.

Settings are specified with the following format (case sensitive):

```
parameter = value
```

Values can be either numbers or text. Single-line comments are supported and must begin with `#`.

---

## Solver

| Parameter | Description | Values | Default |
|---|---|---|---|
| `verbosity` | Controls console output | `0` off · `1` standard · `2` adds solver operations · `3` data collection · `4` debug | `2` |

---

## Problem

| Parameter | Description | Values | Default |
|---|---|---|---|
| `problem/memory_threshold` | Enables memory compression and sparse data structures above this node count | positive integer | `10000` |
| `problem/decimal_digits` | Decimal places to preserve when scaling floating-point costs to integers (PathWyse supports max 12 digits, including the integer part) | non-negative integer | `2` |
| `problem/coordinates/distance/scaling` | Multiplier applied to computed distances | double | `0.95` |
| `problem/coordinates/microdegrees` | Scales coordinates from microdegrees to degrees | `0` no · `1` yes | `0` |

---

## Preprocessing

| Parameter | Description | Values | Default |
|---|---|---|---|
| `algo/preprocessing/intensity` | Number of Dijkstra-based preprocessing rounds | `off` · `standard` (4 rounds) · `full` (standard + one round per extra resource) | `standard` |

---

## Algorithms

| Parameter | Description | Values | Default |
|---|---|---|---|
| `algo_names` | Comma-separated list of algorithms to use | see below | `PWDefault` |
| `algo_active` | Comma-separated enable flags, one per algorithm in `algo_names` | `0` disabled · `1` enabled | `1` |
| `algo_parallel` | Runs all enabled algorithms in parallel | `0` sequential · `1` parallel | `0` |

**Exact algorithms**

| Name | Description |
|---|---|
| `PWDefault` | Multi-resource DP algorithm for cyclic and acyclic networks |
| `PWBucket` | Multi-resource bucket-based DP algorithm for cyclic networks |
| `PWAcyclic` | Single-resource A\*-based DP algorithm for acyclic networks |

**Heuristic algorithms**

| Name | Description |
|---|---|
| `PWDefaultRelaxDom` | PWDefault with relaxed dominance |
| `PWDefaultRelaxQueue` | PWDefault with fixed-size label priority queues |
| `PWBucketRelaxDom` | PWBucket with relaxed dominance |

**Example:** declare three algorithms, with only the two heuristics active at startup:

```
algo_names   = PWDefault,PWDefaultRelaxDom,PWDefaultRelaxQueue
algo_active  = 0,1,1
algo_parallel = 1
```

Algorithm IDs are assigned in order: `PWDefault` gets id `0`, `PWDefaultRelaxDom` gets id `1`, `PWDefaultRelaxQueue` gets id `2`. These IDs are used at runtime to enable, disable, or reset individual algorithms via the Python interface.

---

## PWDefault

| Parameter | Description | Values | Default |
|---|---|---|---|
| `algo/default/autoconfig` | Autoconfigures best settings for the problem type | `0` off · `1` on | `1` |
| `algo/default/timelimit` | Time limit in seconds (`0.0` = no limit) | double ≥ 0 | `0.0` |
| `algo/default/search` | Search direction | `bidirectional` · `forward` · `backward` | `bidirectional` |
| `algo/default/parallel` | Runs forward and backward steps in parallel (requires `bidirectional` search) | `0` off · `1` on | `0` |
| `algo/default/bidirectional/split` | Forward budget fraction of critical resource UB (requires `bidirectional` search) | `[0.0, 1.0]` | `0.5` |
| `algo/default/reserve` | Maximum labels stored per direction; memory is reserved upfront | positive integer | `10000000` |
| `algo/default/use_visited` | Labels track visited nodes (overridden by autoconfig) | `0` off · `1` on | `1` |
| `algo/default/compare_unreachables` | Include unreachable nodes in dominance checks (requires `use_visited = 1`, overridden by autoconfig) | `0` no · `1` yes | `1` |
| `algo/two_cycle_elimination` | Enables two-cycle elimination (requires `compare_unreachables = 1`, otherwise disabled) | `0` off · `1` on | `0` |
| `algo/default/ng` | NG-path relaxation type (does not guarantee an elementary solution) | `off` · `restricted` · `standard` | `off` |
| `algo/default/ng/set_size` | NG neighbourhood size per node (requires NG enabled) | positive integer | `8` |
| `algo/default/ng/set_path` | Path to a CSV file defining the NG neighbourhoods: an n × n matrix of `0`/`1`, one row per node, where each node must include itself. Overrides `set_size` (requires NG enabled) | file path | empty |
| `algo/default/dssr` | DSSR type (guarantees optimality if completed before timelimit; applied after NG if both are enabled) | `off` · `restricted` · `standard` | `standard` |
| `algo/default/candidate/type` | Label candidate selection strategy (overridden by autoconfig) | `node` · `roundrobin` | `node` |
| `algo/default/join/type` | Join strategy after bidirectional search (requires `bidirectional` search) | see below | `ordered` |
| `algo/default/relaxations/queue_limit` | Node pool size limit for `PWDefaultRelaxQueue` | positive integer | `20` |
| `algo/requested_solutions` | Maximum solutions returned (PWDefault, PWBucket and relaxations) | positive integer | `1` |

**Example:** setting `algo/requested_solutions = 5` requests up to 5 solutions. The behaviour depends on the search configuration:

- **Bidirectional search:** the k-ordered join is used, collecting the k best complete paths found during the join phase.
- **Unidirectional search (forward or backward):** the k best labels reaching the destination are returned.

When relaxations are active, the returned solutions are the best k feasible paths found within the relaxation:

- **NG relaxation:** the k best NG-routes are returned. Since NG-routes are a relaxation of the elementary constraint, some returned paths may contain repeated nodes and are not guaranteed to be elementary.
- **DSSR:** the k best elementary solutions found up to that DSSR iteration are returned.

In both cases, some candidate solutions may be discarded before being returned: during the collection phase, paths that violate the relaxation constraints (e.g., contain forbidden cycles when using NG, or are non-elementary when using DSSR) are filtered out. As a result, fewer than k solutions may be returned.

**Join methods**

| Value | Description |
|---|---|
| `naive` | Join all label pairs on arcs |
| `naive_node` | Naive join performed on nodes |
| `classic` | Use incumbent to prune suboptimal arc comparisons |
| `classic_node` | Classic join performed on nodes |
| `ordered` | Sort pools and terminate early when no improvement is possible (arcs) |
| `ordered_node` | Ordered join performed on nodes |
| `kordered` | Ordered join extended to collect multiple solutions (arcs); automatically selected when `requested_solutions > 1` |
| `kordered_node` | k-ordered join performed on nodes |
| `pareto` | Binary-search based join on arcs |
| `pareto_node` | Binary-search based join on nodes |
| `bucket_node` | Join on nodes exploiting the bucket structure (PWBucket only) |
| `kbucket_node` | Bucket join extended to collect multiple solutions (PWBucket only); automatically selected when `requested_solutions > 1` and a node join is set |

---

## PWBucket

PWBucket also reads the following parameters from the PWDefault section: `algo/default/timelimit`, `algo/default/search`, `algo/default/parallel`, `algo/default/ng`, `algo/default/ng/set_size`, `algo/default/ng/set_path`, `algo/default/dssr`, `algo/default/join/type`, `algo/two_cycle_elimination`, `algo/requested_solutions`.

| Parameter | Description | Values | Default |
|---|---|---|---|
| `algo/bucket/resource` | Resource ID used to index the buckets | non-negative integer | `0` |
| `algo/bucket/size_mode` | Bucket size strategy | `unit_size` (size 1) · `min_res_consumption` (minimum res consumption) | `unit_size` |
| `algo/bucket/prev_dom_check_percentage` | Number of preceding buckets checked for dominance, as a fraction of the total bucket count | `[0.0, 1.0]` | `1.0` |
| `algo/bucket/next_dom_check_percentage` | Number of following buckets checked for dominance, as a fraction of the total bucket count | `[0.0, 1.0]` | `1.0` |

---

## Data Collection and Output

| Parameter | Description | Values | Default |
|---|---|---|---|
| `data_collection/level` | Extent of data collected | `-1` off · `0` performance data, no overhead · `1` adds insertion times, small overhead · `4` adds label collection (large overhead, requires `output/write = 1`) | `-1` |
| `output/write` | Write collected data to disk (in the output folder) | `0` off · `1` on | `0` |
