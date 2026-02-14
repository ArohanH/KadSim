# KadSim Simulator
## Compile
```
make -j
```
## Run
```
kadsim <nr_nodes> <z_0> <z_1> <cpu_ratio> <txn_interarrival_time_mean> <global_block_interarrival_time_mean> <duration> [graph_seed simulator_seed]
```
where
 - `nr_nodes` is the number of nodes
 - `z_0` is the fraction of nodes that are slow
 - `z_1` is the fraction of nodes that are low-CPU
 - `cpu_ratio` is the ratio of the hashing power of a high-CPU and a low-CPU node
 - `txn_interarrival_time_mean` is the mean transaction interarrival time (in `ms`) *at each node* when sampling from an exponential distribution
 - `global_block_interarrival_time_mean` is the mean block interarrival time (in `ms`) *globally* when sampling from an exponential distribution at *each node*
 - `duration` is how long (in `ms` in terms of in-simulation time) the simulation should be run for
 - `graph_seed` is a seed for random generation of the nodes and network delay parameters *before* simulation
 - `simulator_seed` is a seed for random events *during* simulation
