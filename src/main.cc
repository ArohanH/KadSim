#include "generate.h"
#include "output.h"
#include "simulator.h"
#include "stats.h"
#include "txn_block.h"

#include <cmath>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <random>
#include <sstream>
#include <string>

static std::ofstream logfile;

int main(int argc, char const* argv[])
{
    /*
     * process cmdline args
     */
    size_t nr_nodes;
    double z_0, z_1, cpu_ratio, txn_interarrival_time_mean, global_block_interarrival_time_mean, duration;
    unsigned int graph_seed, simulator_seed;
    if (argc != 8 && argc != 10) {
        std::cerr << "Usage: " << argv[0] << " <nr_nodes> <z_0> <z_1> <cpu_ratio> <txn_interarrival_time_mean> <global_block_interarrival_time_mean> <duration> [graph_seed simulator_seed]\n";
        return 0;
    } else {
        nr_nodes = std::stoull(argv[1]);
        z_0 = std::stod(argv[2]);
        z_1 = std::stod(argv[3]);
        cpu_ratio = std::stod(argv[4]);
        txn_interarrival_time_mean = std::stod(argv[5]);
        global_block_interarrival_time_mean = std::stod(argv[6]);
        duration = std::stod(argv[7]);

        if (argc == 10) {
            graph_seed = std::stoull(argv[8]);
            simulator_seed = std::stoull(argv[9]);
        } else {
            graph_seed = std::random_device {}();
            simulator_seed = std::random_device {}();
        }
    }

    /*
     * prep output dir
     */
    std::string output_dir;
    {
        auto t = std::time(nullptr);
        auto tm = *std::localtime(&t);
        std::stringstream ss;
        ss << std::put_time(&tm, "blockchain-simulator-output_%Y-%m-%d-%H-%M-%S");
        output_dir = ss.str();
    }
    std::filesystem::create_directory(output_dir);

    /*
     * prep logfile
     */
    logfile = std::ofstream(output_dir + "/log");

    logfile << "*** Simulator for a P2P cryptocurrency network ***\n";
    logfile << "*** " << output_dir << " ***\n";
    logfile << "*** [graph_rng seed=" << graph_seed << ", simulator_rng seed=" << simulator_seed << "] ***\n";
    logfile << "*** Invocation: ";
    for (int i = 0; i < 8; ++i)
        logfile << argv[i] << " ";
    logfile << graph_seed << ' ' << simulator_seed << " ***\n";

    /*
     * simulator random number generator
     */
    std::mt19937 simulator_rng(simulator_seed);

    Simulator simulator(simulator_rng, logfile);

    /*
     * generate nodes
     */
    std::set<std::shared_ptr<ClassicNode>> nodes = generate_nodes(simulator, nr_nodes, z_0, z_1, cpu_ratio, txn_interarrival_time_mean, global_block_interarrival_time_mean);
    std::set<NodeID> node_ids;
    std::map<NodeID, std::shared_ptr<ClassicNode>> node_id_to_node;
    for (auto node : nodes) {
        node_ids.insert(node->node_id);
        node_id_to_node[node->node_id] = node;
    }

    /*
     * graph random number generator
     */
    std::mt19937 graph_rng(graph_seed);

    /*
     * generate network
     */
    auto adj_list = generate_random_undirected_graph(graph_rng, node_ids, 3, 6);

    write_network_to_dot_file(output_dir + "/network.dot", adj_list);

    /*
     * // both constant for a link
     * progapation_delay    ~ U([10.0ms, 500.0ms])
     * link_speed           = 100kb/ms if both fast, else 5kb/ms
     *
     * transmission_delay   = msg_len / link_speed
     * queueing_delay       ~ Exp(link_speed / 96.0kb)  // since mean of exp dist is 1/lambda
     */
    auto generate_link_delay = [&graph_rng](ClassicNode const& n1, ClassicNode const& n2) -> Simulator::LinkDelay {
        std::uniform_real_distribution<double> propagation_delay_dist(10.0, 500.0);
        double propagation_delay = propagation_delay_dist(graph_rng);

        double link_speed = (!n1.is_slow_node && !n2.is_slow_node ? 100.0 : 5.0);

        return [propagation_delay, link_speed](std::mt19937& sim_rng, size_t msg_len) -> double {
            double transmission_delay = msg_len / link_speed;

            std::exponential_distribution<double> queueing_delay_dist(link_speed / 96.0);
            double queueing_delay = queueing_delay_dist(sim_rng);

            return propagation_delay + transmission_delay + queueing_delay;
        };
    };

    /*
     * generate link delays
     */
    {
        std::set<std::shared_ptr<Node>> tmp;
        tmp.insert(nodes.begin(), nodes.end());
        simulator.add_nodes(tmp);
    }
    auto links = generate_link_delays(nodes, adj_list, generate_link_delay);
    simulator.add_links(links);

    /*
     * create initial events
     */
    NodeID genesis_block_miner = (*nodes.begin())->node_id;
    std::shared_ptr<Block> genesis = std::make_shared<Block>(NO_BLOCK_ID, genesis_block_miner, std::vector<std::shared_ptr<Txn>>());
    std::shared_ptr<CreateTxn> create_txn = std::make_shared<CreateTxn>(std::exponential_distribution<double>(1.0 / txn_interarrival_time_mean));
    std::vector<std::shared_ptr<Event>> init_events;
    for (auto& n : nodes) {
        std::shared_ptr<Event> genesis_event = create_synthetic_delivery_event(genesis, n->node_id, 0.0);
        std::shared_ptr<Event> txn_init_event = create_synthetic_timed_event(create_txn, n->node_id, 0.0001);
        init_events.push_back(genesis_event);
        init_events.push_back(txn_init_event);
    }

    /*
     * run simulation
     */
    simulator.init(0.0);
    simulator.inject_events(init_events);
    simulator.run(duration);

    logfile << "*** Simulation complete ***\n";
    logfile.close();

    /*
     * collect output
     */
    std::map<BlockID, std::set<BlockID>> global_block_tree;
    std::map<BlockID, NodeID> global_block_miners;
    for (auto const& node : nodes) {
        auto block_tree = node->collect_block_tree();
        auto block_arrival_times = node->collect_block_arrival_times();
        auto block_miners = node->collect_block_miners();

        write_node_block_tree_to_dot_file(output_dir + "/block_tree_of_node_" + std::to_string(node->node_id) + ".dot", block_tree, block_arrival_times);

        for (auto const& [block_id, successors] : block_tree) {
            auto it = global_block_tree.find(block_id);
            if (it == global_block_tree.end())
                global_block_tree[block_id] = successors;
            else
                it->second.insert(successors.begin(), successors.end());
        }

        global_block_miners.insert(block_miners.begin(), block_miners.end());
    }
    std::map<NodeID, std::pair<BlockID, std::set<BlockID>>> node_positions_in_block_tree;
    for (auto const& node : nodes)
        node_positions_in_block_tree[node->node_id] = node->collect_frontier();

    std::set<BlockID> frontier_blocks;
    std::map<BlockID, std::set<NodeID>> longest_chain_frontier_block_nodes;
    for (auto [node_id, frontier_info] : node_positions_in_block_tree) {
        auto [longest_chain_frontier_block_id, frontier_block_ids] = frontier_info;
        frontier_blocks.insert(frontier_block_ids.begin(), frontier_block_ids.end());
        auto it = longest_chain_frontier_block_nodes.find(longest_chain_frontier_block_id);
        if (it == longest_chain_frontier_block_nodes.end())
            longest_chain_frontier_block_nodes[longest_chain_frontier_block_id] = { node_id };
        else
            it->second.insert(node_id);
    }

    write_global_block_tree_to_dot_file(output_dir + "/global_block_tree.dot", global_block_tree, longest_chain_frontier_block_nodes, frontier_blocks);

    Stats stats = compute_stats(global_block_tree);
    std::map<NodeID, size_t> nr_blocks_by_node, nr_blocks_in_longest_chain_by_node;
    for (auto node_id : node_ids) {
        nr_blocks_by_node[node_id] = 0;
        nr_blocks_in_longest_chain_by_node[node_id] = 0;
    }
    for (auto [block_id, node_id] : global_block_miners) {
        nr_blocks_by_node[node_id]++;
        if (stats.longest_chain.count(block_id) > 0)
            nr_blocks_in_longest_chain_by_node[node_id]++;
    }

    std::map<size_t, size_t> branch_length_distribution;
    for (auto [_, branch_terminal_length] : stats.branch_terminal_lengths) {
        if (branch_length_distribution.count(branch_terminal_length) == 0)
            branch_length_distribution[branch_terminal_length] = 1;
        else
            branch_length_distribution[branch_terminal_length]++;
    }
    std::map<size_t, size_t> chain_length_distribution;
    for (auto [_, chain_terminal_length] : stats.chain_terminal_lengths) {
        if (chain_length_distribution.count(chain_terminal_length) == 0)
            chain_length_distribution[chain_terminal_length] = 1;
        else
            chain_length_distribution[chain_terminal_length]++;
    }

    size_t blocks_fast_highcpu = 0, blocks_fast_lowcpu = 0, blocks_slow_highcpu = 0, blocks_slow_lowcpu = 0;
    for (auto [node_id, nr] : nr_blocks_by_node) {
        auto p = node_id_to_node[node_id];
        if (p->is_slow_node) {
            if (p->is_lowcpu_node)
                blocks_slow_lowcpu += nr;
            else
                blocks_slow_highcpu += nr;
        } else {
            if (p->is_lowcpu_node)
                blocks_fast_lowcpu += nr;
            else
                blocks_fast_highcpu += nr;
        }
    }
    size_t blocks_in_longest_chain_fast_highcpu = 0, blocks_in_longest_chain_fast_lowcpu = 0, blocks_in_longest_chain_slow_highcpu = 0, blocks_in_longest_chain_slow_lowcpu = 0;
    for (auto [node_id, nr] : nr_blocks_in_longest_chain_by_node) {
        auto p = node_id_to_node[node_id];
        if (p->is_slow_node) {
            if (p->is_lowcpu_node)
                blocks_in_longest_chain_slow_lowcpu += nr;
            else
                blocks_in_longest_chain_slow_highcpu += nr;
        } else {
            if (p->is_lowcpu_node)
                blocks_in_longest_chain_fast_lowcpu += nr;
            else
                blocks_in_longest_chain_fast_highcpu += nr;
        }
    }
    OutputStats output_stats {
        global_block_miners.size(),
        blocks_fast_highcpu,
        blocks_fast_lowcpu,
        blocks_slow_highcpu,
        blocks_slow_lowcpu,
        stats.chain_terminal_lengths[stats.longest_chain_end],
        blocks_in_longest_chain_fast_highcpu,
        blocks_in_longest_chain_fast_lowcpu,
        blocks_in_longest_chain_slow_highcpu,
        blocks_in_longest_chain_slow_lowcpu,
        branch_length_distribution,
        chain_length_distribution,
    };
    write_stats_to_py_file(output_dir + "/stats.py", output_stats);

    return 0;
}

void custom_assert(bool b, char const* expr, char const* func, int line, char const* file)
{
    if (!b) {
        logfile << std::flush;
        std::cerr << "Assertion failed:\n";
        std::cerr << "\t" << expr << "\n";
        std::cerr << "in `" << func << "` at " << file << ":" << line << "\n";
        exit(1);
    }
}
