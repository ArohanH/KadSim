#include "output.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <numeric>

void write_global_block_tree_to_dot_file(std::string filename, std::map<BlockID, std::set<BlockID>> const& global_block_tree, std::map<BlockID, std::set<NodeID>> const& longest_chain_frontier_block_nodes, std::set<BlockID> const& frontier_blocks)
{
    std::ofstream f(filename);
    f << "strict digraph {\n";
    for (auto [block_id, _] : global_block_tree)
        f << "\t" << block_id << " [shape=box]\n";
    for (auto block_id : frontier_blocks)
        f << "\t" << block_id << " [shape=box]\n";
    for (auto [block_id, nodes] : longest_chain_frontier_block_nodes) {
        f << "\t" << block_id << "00" << " [shape=ellipse,label=\"";
        for (auto node_id : nodes)
            f << node_id << " ";
        f << "\"]\n";
    }
    for (auto [block_id, _] : longest_chain_frontier_block_nodes)
        f << "\t" << block_id << " -> " << block_id << "00 [style=dashed]\n";
    for (auto [block_id, successor_block_ids] : global_block_tree)
        for (auto successor_block_id : successor_block_ids)
            f << "\t" << block_id << " -> " << successor_block_id << '\n';
    f << '}';
}

void write_node_block_tree_to_dot_file(std::string filename, std::map<BlockID, std::set<BlockID>> const& node_block_tree, std::map<BlockID, double> const& arrival_times)
{
    std::ofstream f(filename);
    f << "strict digraph {\n";
    for (auto [block_id, arrival_time] : arrival_times) {
        auto label = std::to_string(block_id) + ": " + std::to_string(arrival_time) + "ms";
        f << "\t" << block_id << " [shape=box,label=\"" << label << "\"]\n";
    }
    for (auto [block_id, successor_block_ids] : node_block_tree)
        for (auto successor_block_id : successor_block_ids)
            f << "\t" << block_id << " -> " << successor_block_id << '\n';
    f << '}';
}

void write_network_to_dot_file(std::string filename, std::map<NodeID, std::set<NodeID>> const& adj_list)
{
    std::ofstream f(filename);
    f << "strict graph {\n";
    for (auto [node_id, neighbors] : adj_list) {
        for (auto neighbor_id : neighbors) {
            if (node_id < neighbor_id)
                f << "\t" << node_id << " -- " << neighbor_id << '\n';
        }
    }
    f << '}';
}

void write_stats_to_py_file(std::string filename, OutputStats stats)
{
    std::ofstream statfile(filename);
    statfile << std::fixed << std::setprecision(4);

    statfile << "# Auto-generated simulation statistics\n";
    statfile << "overlay_protocol=\"" << stats.overlay_protocol << "\"\n";
    statfile << "graph_density=\"" << stats.graph_density << "\"\n";
    statfile << "nr_nodes=" << stats.nr_nodes << '\n';
    statfile << "duration=" << stats.duration << '\n';

    statfile << "total_blocks=" << stats.total_blocks << '\n';
    statfile << "blocks_fast_highcpu=" << stats.blocks_fast_highcpu << ';';
    statfile << "blocks_slow_highcpu=" << stats.blocks_slow_highcpu << ';';
    statfile << "blocks_fast_lowcpu=" << stats.blocks_fast_lowcpu << ';';
    statfile << "blocks_slow_lowcpu=" << stats.blocks_slow_lowcpu << '\n';
    statfile << "longest_chain_length=" << stats.longest_chain_length << '\n';
    statfile << "blocks_in_longest_chain_fast_highcpu=" << stats.blocks_in_longest_chain_fast_highcpu << ';';
    statfile << "blocks_in_longest_chain_slow_highcpu=" << stats.blocks_in_longest_chain_slow_highcpu << ';';
    statfile << "blocks_in_longest_chain_fast_lowcpu=" << stats.blocks_in_longest_chain_fast_lowcpu << ';';
    statfile << "blocks_in_longest_chain_slow_lowcpu=" << stats.blocks_in_longest_chain_slow_lowcpu << '\n';
    statfile << "branch_length_distribution={";
    for (auto [l, n] : stats.branch_length_distribution)
        statfile << l << ':' << n << ',';
    statfile << "}\n";
    statfile << "chain_length_distribution={";
    for (auto [l, n] : stats.chain_length_distribution)
        statfile << l << ':' << n << ',';
    statfile << "}\n";

    /*
     * new performance metrics
     */
    statfile << "\n# --- Performance Metrics ---\n";
    statfile << "total_messages_sent=" << stats.total_messages_sent << '\n';
    statfile << "total_traffic_kb=" << stats.total_traffic_kb << '\n';
    statfile << "stale_blocks=" << stats.stale_blocks << '\n';
    double stale_rate = (stats.total_blocks > 0) ? (double)stats.stale_blocks / stats.total_blocks : 0.0;
    statfile << "stale_rate=" << stale_rate << '\n';

    /*
     * per-block propagation delay percentiles
     * for each block, delays are sorted arrival times relative to creation
     */
    std::vector<double> all_p50, all_p90, all_p99, all_mean;
    for (auto const& [block_id, delays] : stats.block_propagation_delays) {
        if (delays.empty())
            continue;
        std::vector<double> sorted = delays;
        std::sort(sorted.begin(), sorted.end());
        size_t n = sorted.size();
        double p50 = sorted[std::min((size_t)(n * 0.50), n - 1)];
        double p90 = sorted[std::min((size_t)(n * 0.90), n - 1)];
        double p99 = sorted[std::min((size_t)(n * 0.99), n - 1)];
        double mean = std::accumulate(sorted.begin(), sorted.end(), 0.0) / n;
        all_p50.push_back(p50);
        all_p90.push_back(p90);
        all_p99.push_back(p99);
        all_mean.push_back(mean);
    }

    auto median = [](std::vector<double>& v) -> double {
        if (v.empty())
            return 0.0;
        std::sort(v.begin(), v.end());
        size_t n = v.size();
        return (n % 2 == 0) ? (v[n / 2 - 1] + v[n / 2]) / 2.0 : v[n / 2];
    };

    statfile << "\n# --- Block Propagation Delay Summary (ms) ---\n";
    statfile << "# Across all blocks, median of per-block percentiles\n";
    statfile << "median_p50_propagation_delay=" << median(all_p50) << '\n';
    statfile << "median_p90_propagation_delay=" << median(all_p90) << '\n';
    statfile << "median_p99_propagation_delay=" << median(all_p99) << '\n';
    statfile << "median_mean_propagation_delay=" << median(all_mean) << '\n';

    /*
     * raw per-block propagation delay arrays for detailed analysis
     */
    statfile << "\n# Per-block propagation delays (block_id: [delay_ms, ...])\n";
    statfile << "block_propagation_delays={\n";
    for (auto const& [block_id, delays] : stats.block_propagation_delays) {
        statfile << "  " << block_id << ": [";
        for (size_t i = 0; i < delays.size(); ++i) {
            if (i > 0)
                statfile << ',';
            statfile << delays[i];
        }
        statfile << "],\n";
    }
    statfile << "}\n";

    /*
     * overhead ratio (paper metric: how much extra traffic beyond minimum)
     * minimum = total_blocks * block_avg_size * nr_nodes
     * overhead_ratio = (total_traffic - minimum) / minimum
     */
    if (!stats.block_propagation_delays.empty() && stats.nr_nodes > 0) {
        statfile << "\n# Traffic summary\n";
        statfile << "traffic_per_node_kb=" << stats.total_traffic_kb / stats.nr_nodes << '\n';
        statfile << "messages_per_node=" << (double)stats.total_messages_sent / stats.nr_nodes << '\n';
    }

    /*
     * upload contention stats
     */
    statfile << "\n# --- Upload Contention Stats ---\n";
    statfile << "contention_total_sends=" << stats.contention_total_sends << '\n';
    statfile << "contention_queued_sends=" << stats.contention_queued_sends << '\n';
    statfile << "contention_fraction=" << (stats.contention_total_sends > 0 ? (double)stats.contention_queued_sends / stats.contention_total_sends : 0.0) << '\n';
    statfile << "contention_mean_wait_ms=" << stats.contention_mean_wait_ms << '\n';
    statfile << "contention_p50_wait_ms=" << stats.contention_p50_wait_ms << '\n';
    statfile << "contention_p90_wait_ms=" << stats.contention_p90_wait_ms << '\n';
    statfile << "contention_p99_wait_ms=" << stats.contention_p99_wait_ms << '\n';
    statfile << "contention_max_wait_ms=" << stats.contention_max_wait_ms << '\n';
}
