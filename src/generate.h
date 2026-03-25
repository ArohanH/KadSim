#ifndef GENERATE_H
#define GENERATE_H

#include "classic_node.h"
#include "node.h"

#include <map>
#include <memory>
#include <random>
#include <set>

class Simulator;

std::set<std::shared_ptr<ClassicNode>> generate_nodes(Simulator& simulator, size_t nr_nodes, double z_0, double z_1, double cpu_ratio, double txn_interarrival_time_mean, double global_block_interarrival_time_mean, bool use_kadcast, size_t beta, std::mt19937& overlay_rng);
std::map<NodeID, std::set<NodeID>> generate_random_undirected_graph(std::mt19937& rng, std::set<NodeID> const& nodes, size_t min_deg, size_t max_deg);
std::map<NodeID, std::map<NodeID, Simulator::LinkProperties>> generate_link_delays(std::set<std::shared_ptr<ClassicNode>> const& nodes, std::map<NodeID, std::set<NodeID>> const& adj_list, std::function<Simulator::LinkProperties(ClassicNode const&, ClassicNode const&)> generate_link);

#endif // GENERATE_H
