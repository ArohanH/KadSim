# KadSim — KADcast vs Classic Flooding Blockchain Simulator

A discrete-event simulator for comparing **KADcast** (structured overlay broadcast) against **Classic flooding** (unstructured gossip) in a peer-to-peer cryptocurrency network. Built in C++17.

---

## Build & Run

### Compile
```
make -j
```

### Run single simulation
```
bin/kadsim <nr_nodes> <z_0> <z_1> <cpu_ratio> <txn_iat> <block_iat> <duration> <protocol> [density] [beta] [graph_seed sim_seed]
```

### Run paired experiment (Classic + KADcast, with analysis)
```
python3 run_experiment.py <nr_nodes> <z_0> <z_1> <cpu_ratio> <txn_iat> <block_iat> <duration> [density] [beta] [graph_seed sim_seed]
```

### Parameters

| Parameter | Description |
|---|---|
| `nr_nodes` | Number of nodes in the network |
| `z_0` | Fraction of nodes that are **slow** (low bandwidth) |
| `z_1` | Fraction of nodes that are **low-CPU** |
| `cpu_ratio` | Ratio of hashing power: high-CPU / low-CPU |
| `txn_iat` | Mean transaction interarrival time (ms) per node, Exp distributed |
| `block_iat` | Mean block interarrival time (ms) globally, Exp distributed |
| `duration` | Simulation duration (ms, in-simulation time) |
| `protocol` | `Classic` or `Kadcast` |
| `density` | Graph density: `sparse`, `moderate`, or `dense` (default: `dense`) |
| `beta` | KADcast redundancy parameter β ∈ [1, 10] (default: 3) |
| `graph_seed` | RNG seed for network topology generation |
| `sim_seed` | RNG seed for simulation events |

---

## Network Model

### Underlay Graph

Nodes are connected by a random undirected graph with configurable degree ranges:

| Density | Degree range | Description |
|---|---|---|
| `sparse` | [3, 6] | Resource-constrained / poorly-connected |
| `moderate` | [10, 25] | Typical P2P network |
| `dense` | [50, 80] | Well-connected / datacenter-like |

### Node Types

Each node is classified along two axes:

| | Fast (bandwidth) | Slow (bandwidth) |
|---|---|---|
| **High-CPU** | 12.5 KB/ms (100 Mbit/s) | 0.625 KB/ms (5 Mbit/s) |
| **Low-CPU** | 12.5 KB/ms (100 Mbit/s) | 0.625 KB/ms (5 Mbit/s) |

- Fraction slow = $z_0$, fraction low-CPU = $z_1$ (independent).
- Block validation throughput: low-CPU = 10 KB/ms, high-CPU = $10 \times \texttt{cpu\_ratio}$ KB/ms.

### Per-Link Properties

Each undirected edge $(u, v)$ is assigned:

$$d_{uv}^{\text{prop}} \sim \mathcal{U}(5, 300) \text{ ms}$$

$$c_{uv} = \begin{cases} 12.5 \text{ KB/ms} & \text{if both } u, v \text{ are fast} \\ 0.625 \text{ KB/ms} & \text{otherwise} \end{cases}$$

### Routing

Shortest paths are computed via **Dijkstra** on propagation delays. For each source $s$, the simulator pre-computes:
- $\text{next\_hop}(s, t)$ — first hop on the latency-shortest path from $s$ to $t$
- $d(s, t)$ — total propagation delay along the shortest path
- $h(s, t)$ — hop count along the shortest path

### Upload Channel Model

Each node has **$P = 8$ parallel upload channels**, sharing the node's total upload bandwidth equally. Each channel operates as a FIFO queue:

$$c_i^{\text{channel}} = \frac{c_{\text{node}}}{P}$$

When a message of size $m$ KB is sent, the simulator assigns it to the **least-busy channel** (earliest free time). The upload start time is:

$$t_{\text{upload}} = \max(t_{\text{request}}, t_{\text{slot\_free}})$$

The channel is then busy until:

$$t_{\text{slot\_free}} \leftarrow t_{\text{upload}} + \frac{m}{c_i^{\text{channel}}}$$

This models contention: if a node sends multiple messages concurrently, later sends queue behind earlier ones on each channel.

---

## Protocol-Specific Delay Models

### Classic Flooding

Classic uses **store-and-forward hop-by-hop relay**. A node receiving a block validates it, then floods it to all neighbors (except the sender). Each hop incurs:

$$t_{\text{receive}} = t_{\text{upload}} + d_{uv}^{\text{prop}} + \frac{m}{\min(c_i^{\text{channel}},\; c_{uv})}$$

where:
- $t_{\text{upload}}$ = upload channel start time at the relay (after queuing)
- $d_{uv}^{\text{prop}}$ = propagation delay on the physical link
- $\min(c_i^{\text{channel}}, c_{uv})$ = effective speed (bottleneck of upload channel or link)

**Per-hop queuing delay** is added before the upload slot request to model the inv/getdata handshake overhead and application-level processing at each relay:

$$q_{\text{classic}} \sim \text{Exp}(\mu_{\text{classic}})$$

The total per-hop delay for Classic is therefore:

$$\Delta_{\text{classic}}^{\text{hop}} = q_{\text{classic}} + \text{upload\_wait} + d_{uv}^{\text{prop}} + \frac{m}{\text{effective\_speed}}$$

For a message traversing $H$ hops, the end-to-end delay is the **sum** of per-hop delays (each hop is a separate event):

$$T_{\text{classic}} = \sum_{i=1}^{H} \Delta_{\text{classic}}^{\text{hop},\, i}$$

**Current parameter:** $\mu_{\text{classic}} = 100$ ms (based on Decker & Wattenhofer, 2013).

### KADcast Overlay Broadcast

KADcast uses a **Kademlia-based structured overlay**. Blocks are split into chunks (max 500 KB each) and sent directly from the overlay source to each delegate via the shortest underlay path. No intermediate overlay-level store-and-forward.

For a single chunk delivery from overlay source $s$ to delegate $t$:

$$T_{\text{kadcast}} = t_{\text{upload}}(s) + d(s, t) + \frac{m}{c_s^{\text{channel}}} + \sum_{j=1}^{h(s,t)-1} q_j^{\text{kadcast}}$$

where:
- $t_{\text{upload}}(s)$ = upload channel queuing at the **source only**
- $d(s, t)$ = total propagation delay along the Dijkstra shortest path
- $m / c_s^{\text{channel}}$ = transmission delay at the source's upload channel
- $q_j^{\text{kadcast}} \sim \text{Exp}(\mu_{\text{kadcast}})$ = per-intermediate-hop queuing (IP/TCP forwarding noise)
- $h(s, t) - 1$ = number of intermediate IP hops (excludes the source)

**Current parameter:** $\mu_{\text{kadcast}} = 20$ ms (OS/TCP stack overhead at intermediate IP nodes).

**Key difference:** Upload channel contention is modeled at **every relay** for Classic, but only at the **source** for KADcast. Intermediate underlay hops for KADcast are transparent network-layer forwarding.

### KADcast Overlay Structure

- Each node gets a random $L$-bit overlay ID ($L = \lceil \log_2 N \rceil$).
- XOR-distance buckets: bucket $h$ contains peers whose IDs differ in the $h$-th most significant bit.
- On broadcast, a node at height $h$ selects $\beta$ random delegates from each bucket $\leq h$ and sends them chunks with decremented height.
- **Cut-through forwarding:** delegates begin re-broadcasting upon receiving the first chunk, without waiting for the full block.

### Block Validation

Upon receiving a complete block, each node schedules a validation timer:

$$t_{\text{validate}} = \frac{\text{block\_size}}{v_{\text{node}}}$$

where $v_{\text{node}}$ is the node's validation throughput (KB/ms). The block is only forwarded after validation completes.

---

## Message Sizes

| Object | Size |
|---|---|
| Transaction | 1 KB |
| Block | $1 + n_{\text{txns}}$ KB (max 1000 KB) |
| Chunk (KADcast) | max 500 KB (blocks split into ⌈size/500⌉ chunks) |

---

## Delay Budget Summary

For a **fast node**, 1000 KB block, mean propagation 152.5 ms, $P = 8$:

### Classic (per hop)

| Component | Expression | Typical value |
|---|---|---|
| Protocol queuing | $\text{Exp}(100\text{ms})$ | ~100 ms |
| Upload wait | depends on congestion | 0–5000+ ms |
| Propagation | $\mathcal{U}(5, 300)$ | ~152 ms |
| Transmission | $1000 / \min(1.5625, c_{uv})$ | ~640 ms |
| **Per-hop total** | sum of above | **~892 ms** + congestion |

### KADcast (source → delegate, 500 KB chunk, ~3 IP hops)

| Component | Expression | Typical value |
|---|---|---|
| Upload wait | depends on congestion | 0–5000+ ms |
| Propagation | $d(s,t)$ (Dijkstra sum) | ~457 ms |
| Transmission | $500 / 1.5625$ | ~320 ms |
| IP hop queuing | $2 \times \text{Exp}(20\text{ms})$ | ~40 ms |
| **Total** | sum of above | **~817 ms** + congestion |

---

## Metrics

### Consensus Metrics
- **Total blocks mined:** number of blocks created during the simulation
- **Longest chain length:** length of the longest valid chain at simulation end
- **Stale blocks:** blocks mined but not in the longest chain ($= \text{total} - \text{longest}$)
- **Stale rate:** $\text{stale blocks} / \text{total blocks}$

### Network Load Metrics
- **Total messages sent:** count of overlay-level sends (one per source→destination pair; intermediate relay hops are not counted separately)
- **Total traffic (KB):** sum of `size_kb()` of all sent messages
- **Messages per node / Traffic per node:** averages across all nodes

### Block Propagation Delay Metrics

For each block $b$, the propagation delay to node $n$ is:

$$\delta_b(n) = t_{\text{arrival}}(b, n) - t_{\text{creation}}(b)$$

where $t_{\text{creation}}(b)$ is the arrival time at the miner. Per-block percentiles (p50, p90, p99, max) are computed. The reported metrics are **medians across all blocks**:

- **Median of per-block p50:** the median block's 50th-percentile propagation delay
- **Median of per-block p90:** the median block's 90th-percentile propagation delay
- **Mean block coverage:** average fraction of nodes that received each block

### Mining Distribution
- Blocks mined by each node category (Fast/Slow × HighCPU/LowCPU)
- Blocks in the longest chain by each category

---

## Output

Each simulation run produces a directory containing:
- `log` — event log
- `stats.py` — machine-readable statistics (loaded by `analyze.py`)
- `network.dot` — underlay graph (Graphviz)
- `global_block_tree.dot` — block tree across all nodes
- `block_tree_of_node_*.dot` — per-node block trees

The `run_experiment.py` script runs both protocols with the same parameters and produces a `results_*.txt` comparison file.
