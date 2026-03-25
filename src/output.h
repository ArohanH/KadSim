#ifndef OUTPUT_H
#define OUTPUT_H

#include "id.h"

#include <map>
#include <set>
#include <string>
#include <vector>

void write_global_block_tree_to_dot_file(std::string filename, std::map<BlockID, std::set<BlockID>> const& global_block_tree, std::map<BlockID, std::set<NodeID>> const& longest_chain_frontier_block_nodes, std::set<BlockID> const& frontier_blocks);
void write_node_block_tree_to_dot_file(std::string filename, std::map<BlockID, std::set<BlockID>> const& node_block_tree, std::map<BlockID, double> const& arrival_times);
void write_network_to_dot_file(std::string filename, std::map<NodeID, std::set<NodeID>> const& adj_list);

struct OutputStats {
    std::string overlay_protocol;
    std::string graph_density;
    size_t nr_nodes;
    double duration;

    size_t total_blocks;
    size_t blocks_fast_highcpu = 0, blocks_fast_lowcpu = 0, blocks_slow_highcpu = 0, blocks_slow_lowcpu = 0;
    size_t longest_chain_length;
    size_t blocks_in_longest_chain_fast_highcpu = 0, blocks_in_longest_chain_fast_lowcpu = 0, blocks_in_longest_chain_slow_highcpu = 0, blocks_in_longest_chain_slow_lowcpu = 0;
    std::map<size_t, size_t> branch_length_distribution;
    std::map<size_t, size_t> chain_length_distribution;

    /*
     * new performance metrics
     */
    size_t total_messages_sent;
    double total_traffic_kb;

    /*
     * per-block propagation delay to each node (block_id -> sorted list of delays in ms)
     */
    std::map<BlockID, std::vector<double>> block_propagation_delays;

    /*
     * stale block rate
     */
    size_t stale_blocks;
};
void write_stats_to_py_file(std::string filename, OutputStats output_stats);

#endif // OUTPUT_H
