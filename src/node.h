#ifndef NODE_H
#define NODE_H

#include "id.h"
#include "simulator.h"

#include <memory>
#include <random>

class Simulator;

class Block;
class Chunk;
class Txn;

/*
 * timer class for txn creation at each node
 */
class CreateTxn : public Timer {
public:
    std::exponential_distribution<double> interarrival_time_dist;

    CreateTxn(std::exponential_distribution<double> interarrival_time_dist)
        : interarrival_time_dist(interarrival_time_dist)
    {
    }
    void trigger(std::shared_ptr<Node> n) override;
};

// Arohan
/*
 * timer class for block mining at each node
 */
class MineBlock : public Timer {
public:
    std::shared_ptr<Block> mined_block;

    MineBlock(std::shared_ptr<Block> block_to_mine)
        : mined_block(block_to_mine) { }

    void trigger(std::shared_ptr<Node> n) override;
};
// Arohan

class ValidateBlock : public Timer {
public:
    std::shared_ptr<Block> block;
    NodeID from;

    ValidateBlock(std::shared_ptr<Block> block, NodeID from)
        : block(block), from(from)
    {
    }

    void trigger(std::shared_ptr<Node> n) override;
};

/*
 * abstract class for a node in the blockchain simulation
 * any implementation of a node should extend this class by
 * implementing the four callbacks
 */
class Node {
private:
    static NodeID next_node_id;

    std::exponential_distribution<double> txn_interarrival_time_dist;
    // Arohan
    std::exponential_distribution<double> block_interarrival_time_dist;
    // Arohan
    double block_validation_throughput_kb_per_ms;
    std::set<BlockID> seen_network_blocks;
    std::set<BlockID> scheduled_block_validations;

protected:
    void start_mining_new_block(std::shared_ptr<Block> new_block);
    double block_validation_time(std::shared_ptr<Block> const& block) const;

public:
    Node(Simulator& simulator, double txn_interarrival_time_mean, double block_interarrival_time_mean, double block_validation_throughput_kb_per_ms)
        : txn_interarrival_time_dist(1.0 / txn_interarrival_time_mean),
          block_interarrival_time_dist(1.0 / block_interarrival_time_mean),
          block_validation_throughput_kb_per_ms(block_validation_throughput_kb_per_ms),
          simulator(simulator), node_id(next_node_id++)
    {
    }
    virtual ~Node() = default;

    Simulator& simulator;
    NodeID const node_id;

    void receive_from_network(std::shared_ptr<Block> block, NodeID from);
    void receive_from_network(std::shared_ptr<Txn> txn, NodeID from);
    void receive_from_network(std::shared_ptr<Chunk> chunk, NodeID from);

    virtual void receive(std::shared_ptr<Block> block, NodeID from) = 0;
    virtual void receive(std::shared_ptr<Txn> txn, NodeID from) = 0;
    virtual void receive(std::shared_ptr<Chunk> chunk, NodeID from)
    {
        (void)chunk;
        (void)from;
    }

    void process(std::shared_ptr<ValidateBlock> validate_block);
    virtual void process(std::shared_ptr<MineBlock> create_block) = 0;
    virtual void process(std::shared_ptr<CreateTxn> create_txn) = 0;
};

#endif // NODE_H
