#include "assert.h"
#include "classic_node.h"
#include "kadcast_node.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <numeric>
#include <map>
#include <memory>
#include <random>
#include <set>
#include <stdexcept>

std::set<std::shared_ptr<ClassicNode>> generate_nodes(Simulator& simulator, size_t nr_nodes, double z_0, double z_1, double cpu_ratio, double txn_interarrival_time_mean, double global_block_interarrival_time_mean, bool use_kadcast, size_t beta, std::mt19937& overlay_rng)
{
    std::set<std::shared_ptr<ClassicNode>> nodes;

    size_t nr_slow_low_cpu = std::floor(nr_nodes * z_0 * z_1);
    size_t nr_slow_high_cpu = std::floor(nr_nodes * z_0 * (1.0 - z_1));
    size_t nr_fast_low_cpu = std::floor(nr_nodes * (1.0 - z_0) * z_1);
    size_t nr_fast_high_cpu = nr_nodes - nr_slow_low_cpu - nr_slow_high_cpu - nr_fast_low_cpu;

    /*
     * all hashing powers should sum to 1.0
     */
    size_t nr_low_cpu = nr_slow_low_cpu + nr_fast_low_cpu;
    size_t nr_high_cpu = nr_slow_high_cpu + nr_fast_high_cpu;
    double hashing_power_low_cpu = 1.0 / (nr_low_cpu + cpu_ratio * nr_high_cpu);
    double hashing_power_high_cpu = cpu_ratio * hashing_power_low_cpu;
    double constexpr lowcpu_validation_throughput_kb_per_ms = 10.0;

    std::tuple<size_t, bool, bool, double> node_types[] = {
        { nr_slow_low_cpu, true, true, hashing_power_low_cpu },
        { nr_slow_high_cpu, true, false, hashing_power_high_cpu },
        { nr_fast_low_cpu, false, true, hashing_power_low_cpu },
        { nr_fast_high_cpu, false, false, hashing_power_high_cpu },
    };

    size_t lbit_width = 1;
    while ((size_t { 1 } << lbit_width) < nr_nodes)
        ++lbit_width;

    std::vector<LbitID> overlay_ids(size_t { 1 } << lbit_width);
    std::iota(overlay_ids.begin(), overlay_ids.end(), 0);
    std::shuffle(overlay_ids.begin(), overlay_ids.end(), overlay_rng);
    size_t overlay_id_index = 0;

    for (auto [nr, is_slow_node, is_lowcpu_node, hashing_power] : node_types) {
        double block_interarrival_time_mean = global_block_interarrival_time_mean / hashing_power;
        double block_validation_throughput_kb_per_ms = lowcpu_validation_throughput_kb_per_ms * (is_lowcpu_node ? 1.0 : cpu_ratio);
        for (size_t i = 0; i < nr; ++i) {
            if (use_kadcast) {
                nodes.insert(std::make_shared<KadcastNode>(
                    simulator,
                    is_slow_node,
                    is_lowcpu_node,
                    txn_interarrival_time_mean,
                    block_interarrival_time_mean,
                    block_validation_throughput_kb_per_ms,
                    overlay_ids.at(overlay_id_index++),
                    lbit_width,
                    beta));
            } else {
                nodes.insert(std::make_shared<ClassicNode>(
                    simulator,
                    is_slow_node,
                    is_lowcpu_node,
                    txn_interarrival_time_mean,
                    block_interarrival_time_mean,
                    block_validation_throughput_kb_per_ms));
            }
        }
    }

    if (use_kadcast) {
        std::map<NodeID, LbitID> node_lbit_ids;
        for (auto const& node : nodes)
            node_lbit_ids[node->node_id] = std::dynamic_pointer_cast<KadcastNode>(node)->lbit_id;
        for (auto const& node : nodes)
            std::dynamic_pointer_cast<KadcastNode>(node)->initialize_routing(node_lbit_ids);
    }

    return nodes;
}

void dfs(NodeID n, std::map<NodeID, std::set<NodeID>> const& adj_list, std::set<NodeID>& visited)
{
    visited.insert(n);
    for (auto v : adj_list.at(n))
        if (visited.count(v) == 0)
            dfs(v, adj_list, visited);
}
bool graph_is_connected(std::map<NodeID, std::set<NodeID>> const& adj_list)
{
    std::set<NodeID> visited;
    dfs(adj_list.begin()->first, adj_list, visited);
    return visited.size() == adj_list.size();
}

std::map<NodeID, std::set<NodeID>> generate_random_undirected_graph(std::mt19937& rng, std::set<NodeID> const& node_ids, size_t min_deg, size_t max_deg)
{
    if (!(min_deg + 1 <= node_ids.size()))
        throw std::invalid_argument("min_deg + 1 <= nr_nodes");
    if (!(min_deg <= max_deg))
        throw std::invalid_argument("min_deg <= max_deg");

    std::map<NodeID, std::set<NodeID>> adj_list;

    size_t nr_nodes = node_ids.size();

    do {
        /*
         * step 1: generate degree sequence
         */
        std::vector<int> degrees(nr_nodes);
        size_t total_degree;
        do {
            total_degree = 0;
            std::uniform_int_distribution<int> dist(min_deg, max_deg);
            for (int& d : degrees) {
                d = dist(rng);
                total_degree += d;
            }
        } while (total_degree % 2 != 0); // Ensure the sum of degrees is even

        std::map<NodeID, int> degree_sequence;
        size_t i = 0;
        for (auto const& node_id : node_ids) {
            degree_sequence[node_id] = degrees[i++];
            adj_list[node_id] = {};
        }

        /*
         * step 2: create configuration model
         */
        std::vector<NodeID> stubs;
        for (auto const& [node_id, degree] : degree_sequence)
            stubs.insert(stubs.end(), degree, node_id);
        std::shuffle(stubs.begin(), stubs.end(), rng);
        /*
         * stubs now contains all the node ids (each appearing as many times as
         * its degree) in shuffled order
         */

        bool should_reshuffle = true;
        while (!stubs.empty()) {
            NodeID id1 = stubs.back();
            stubs.pop_back();
            NodeID id2 = stubs.back();
            stubs.pop_back();
            if (id1 != id2 && adj_list[id1].count(id2) == 0) {
                /*
                 * if id1 != id2 and they aren't already connected, connect them
                 */
                adj_list[id1].insert(id2);
                adj_list[id2].insert(id1);
                should_reshuffle = true;
            } else if (should_reshuffle) {
                stubs.push_back(id1);
                stubs.push_back(id2);
                std::shuffle(stubs.begin(), stubs.end(), rng);
                should_reshuffle = false;
            } else
                should_reshuffle = true;
        }

        /*
         * step 3: adjust degrees to meet constraints
         */
        bool should_rerun_adjustment;
        do {
            should_rerun_adjustment = false;
            std::uniform_int_distribution<int> dist(0, nr_nodes - 1);
            for (auto const& v : node_ids) {
                while (adj_list[v].size() < min_deg) {
                    NodeID u;
                    /*
                     * select random non-neighbour with deg < 6 to connect to
                     */
                    do {
                        auto it = node_ids.begin();
                        std::advance(it, dist(rng));
                        u = *it;
                    } while (!(u != v && adj_list[u].size() < max_deg && adj_list[v].count(u) == 0));
                    adj_list[u].insert(v);
                    adj_list[v].insert(u);
                }

                while (adj_list[v].size() > max_deg) {
                    NodeID u = *adj_list[v].begin();
                    adj_list[v].erase(u);
                    adj_list[u].erase(v);
                    if (adj_list[u].size() < min_deg || adj_list[u].size() > max_deg)
                        should_rerun_adjustment = true;
                }
            }
        } while (should_rerun_adjustment);
    } while (!graph_is_connected(adj_list));

    /*
     * verify constraints
     */
    size_t actual_max_deg = std::numeric_limits<size_t>::min(), actual_min_deg = std::numeric_limits<size_t>::max();
    for (auto const& [_, adj] : adj_list) {
        size_t deg = adj.size();
        if (max_deg < deg)
            max_deg = deg;
        if (min_deg > deg)
            min_deg = deg;
    }
    assert(actual_max_deg <= max_deg);
    assert(actual_min_deg >= min_deg);
    assert(graph_is_connected(adj_list));

    return adj_list;
}

std::map<NodeID, std::map<NodeID, Simulator::LinkProperties>> generate_link_delays(std::set<std::shared_ptr<ClassicNode>> const& nodes, std::map<NodeID, std::set<NodeID>> const& adj_list, std::function<Simulator::LinkProperties(ClassicNode const&, ClassicNode const&)> generate_link)
{
    std::map<NodeID, std::map<NodeID, Simulator::LinkProperties>> link_params;

    std::map<NodeID, std::shared_ptr<ClassicNode const>> node_id_to_node;
    for (auto const& n : nodes)
        node_id_to_node[n->node_id] = n;

    for (auto const& node : nodes)
        link_params[node->node_id] = {};

    for (auto const& [node_id, neighbors] : adj_list) {
        for (auto neighbor_id : neighbors) {
            if (neighbor_id < node_id)
                continue;

            auto properties = generate_link(*node_id_to_node[node_id], *node_id_to_node[neighbor_id]);
            link_params[node_id][neighbor_id] = properties;
            link_params[neighbor_id][node_id] = properties;
        }
    }

    return link_params;
}
