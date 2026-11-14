# Suggested Settings

The **pathwyse.set** file in the root directory uses PWDefault, which solves both cyclic and acyclic networks. The settings below give better performance depending on the network type. See [Parameters](PARAMETERS_TABLE.md) for the full description of each parameter.

---

## Cyclic networks

In our preliminary experiments, the best performance was obtained by PWBucket with NG, restricted DSSR and the bucket join:

```
algo_names  = PWBucket
algo_active = 1

algo/default/autoconfig = 1
algo/default/search = bidirectional
algo/default/parallel = 1
algo/default/compare_unreachables = 1
algo/default/ng = standard
algo/default/ng/set_size = 8
algo/default/dssr = restricted
algo/default/join/type = bucket_node
algo/bucket/size_mode = min_res_consumption
```

With both NG and DSSR enabled, the search first finds an ng-path solution. If it is elementary, the search stops. Otherwise, DSSR iterations start from the NG neighbourhoods: with restricted DSSR, each node repeated in the current solution is added only to the neighbourhoods of the nodes in its cycle, instead of to all neighbourhoods as in standard DSSR. The search is repeated until the solution is elementary.

The NG neighbourhoods are built from `set_size`. To use your own, for example to run a fair comparison with another solver using the same neighbourhoods, load them from a file with `algo/default/ng/set_path` (see [Parameters](PARAMETERS_TABLE.md)).

If an ng-path solution is enough, set `algo/default/dssr = off`: the search stops after the NG relaxation, and the returned path is not guaranteed to be elementary.

When `algo/requested_solutions > 1`, autoconfig switches the join to `kbucket_node`, which collects multiple solutions.

PWBucket requires at least one resource suitable for building the buckets. If none is available, use PWDefault with the settings provided in **pathwyse.set**.

---

## Acyclic networks

Use PWAcyclic when the network has a single resource and no negative costs.

Otherwise, use PWDefault with NG and DSSR disabled:

```
algo_names  = PWDefault
algo_active = 1

algo/default/autoconfig = 1
algo/default/ng = off
algo/default/dssr = off
```

With these settings, autoconfig turns off visited-node tracking, since paths in acyclic networks are always elementary.