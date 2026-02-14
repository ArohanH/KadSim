#include "output.h"

#include <fstream>

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
}
