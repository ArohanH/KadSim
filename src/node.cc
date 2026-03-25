#include "node.h"
#include "txn_block.h"

void CreateTxn::trigger(std::shared_ptr<Node> n)
{
    auto t = std::static_pointer_cast<CreateTxn>(shared_from_this());
    n->simulator.register_timer(n->node_id, t, interarrival_time_dist(n->simulator.rng));
    n->process(t);
}

void MineBlock::trigger(std::shared_ptr<Node> n)
{
    n->process(std::static_pointer_cast<MineBlock>(shared_from_this()));
}

void ValidateBlock::trigger(std::shared_ptr<Node> n)
{
    n->process(std::static_pointer_cast<ValidateBlock>(shared_from_this()));
}

void Node::start_mining_new_block(std::shared_ptr<Block> new_block)
{
    auto mine_block = std::make_shared<MineBlock>(new_block);
    simulator.register_timer(node_id, mine_block, block_interarrival_time_dist(simulator.rng));
}

double Node::block_validation_time(std::shared_ptr<Block> const& block) const
{
    if (block->parent_block_id == NO_BLOCK_ID)
        return 0.0;
    return block->size_kb() / block_validation_throughput_kb_per_ms;
}

void Node::receive_from_network(std::shared_ptr<Block> block, NodeID from)
{
    if (seen_network_blocks.count(block->block_id) > 0)
        return;

    seen_network_blocks.insert(block->block_id);
    scheduled_block_validations.insert(block->block_id);

    auto validate_block = std::make_shared<ValidateBlock>(block, from);
    simulator.register_timer(node_id, validate_block, block_validation_time(block));
}

void Node::receive_from_network(std::shared_ptr<Txn> txn, NodeID from)
{
    receive(txn, from);
}

void Node::receive_from_network(std::shared_ptr<Chunk> chunk, NodeID from)
{
    receive(chunk, from);
}

void Node::process(std::shared_ptr<ValidateBlock> validate_block)
{
    scheduled_block_validations.erase(validate_block->block->block_id);
    receive(validate_block->block, validate_block->from);
}
