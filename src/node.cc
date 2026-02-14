#include "node.h"

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

void Node::start_mining_new_block(std::shared_ptr<Block> new_block)
{
    auto mine_block = std::make_shared<MineBlock>(new_block);
    simulator.register_timer(node_id, mine_block, block_interarrival_time_dist(simulator.rng));
}
