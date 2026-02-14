#include "assert.h"
#include "classic_node.h"
#include "simulator.h"
#include "txn_block.h"

#include <memory>
#include <random>

NodeID Node::next_node_id = 0;
double constexpr BLOCK_REWARD = 50.0;

ClassicNode::ClassicNode(Simulator& simulator, bool is_slow_node, bool is_lowcpu_node, double txn_interarrival_time_mean, double block_interarrival_time_mean)
    : Node(simulator, txn_interarrival_time_mean, block_interarrival_time_mean), is_slow_node(is_slow_node), is_lowcpu_node(is_lowcpu_node)
{
}

std::map<BlockID, std::set<BlockID>> ClassicNode::collect_block_tree() const
{
    std::map<BlockID, std::set<BlockID>> block_tree;
    for (auto [block_id, block_info] : all_block_info) {
        BlockID parent_block_id = block_info.block->parent_block_id;
        if (parent_block_id == NO_BLOCK_ID)
            continue;

        auto it = block_tree.find(parent_block_id);
        if (it != block_tree.end())
            it->second.insert(block_id);
        else
            block_tree[parent_block_id] = { block_id };
    }
    return block_tree;
}
std::pair<BlockID, std::set<BlockID>> ClassicNode::collect_frontier() const
{
    std::set<BlockID> frontier;
    for (auto const& [block_id, _] : frontier_block_info)
        frontier.insert(block_id);
    return { longest_chain_frontier_block_id, frontier };
}
std::map<BlockID, double> ClassicNode::collect_block_arrival_times() const
{
    std::map<BlockID, double> times;
    for (auto const& [block_id, block_info] : all_block_info)
        times[block_id] = block_info.arrival_time;
    return times;
}
std::map<BlockID, NodeID> ClassicNode::collect_block_miners() const
{
    std::map<BlockID, NodeID> miners;
    for (auto const& [block_id, block_info] : all_block_info)
        miners[block_id] = block_info.block->miner;
    return miners;
}

static bool apply_block(std::map<NodeID, double>& balance, std::set<TxnID>& txns, std::shared_ptr<Block> const& block)
{
    for (auto t : block->txns)
        if (balance.count(t->payer) == 0 || (balance[t->payer] -= t->amount) < 0.0 || txns.count(t->txn_id) > 0)
            return false;
    for (auto t : block->txns) {
        if (balance.count(t->recipient) == 0)
            balance[t->recipient] = t->amount;
        else
            balance[t->recipient] += t->amount;
        txns.insert(t->txn_id);
    }
    if (balance.count(block->miner) == 0)
        balance[block->miner] = BLOCK_REWARD;
    else
        balance[block->miner] += BLOCK_REWARD;
    return true;
}

static void rollback_block(std::map<NodeID, double>& balance, std::set<TxnID>& txns, std::shared_ptr<Block> const& block)
{
    for (auto t : block->txns) {
        balance[t->payer] += t->amount;
        balance[t->recipient] -= t->amount;
        txns.erase(t->txn_id);
    }
    balance[block->miner] -= BLOCK_REWARD;
}

void ClassicNode::start_mining_new_block()
{
    auto balance = frontier_block_info[longest_chain_frontier_block_id].balance;
    std::vector<std::shared_ptr<Txn>> txns_to_be_in_new_block;

    /*
     * get mempool txns that are valid
     */
    for (auto [txn_id, txn] : mempool) {
        assert(frontier_block_info[longest_chain_frontier_block_id].txns.count(txn->txn_id) == 0);
        if (balance.count(txn->payer) == 0 || balance[txn->payer] < txn->amount)
            continue;
        if (txns_to_be_in_new_block.size() == Block::MAX_NR_TXNS)
            break;
        balance[txn->payer] -= txn->amount;
        txns_to_be_in_new_block.push_back(txn);
    }

    auto new_block = std::make_shared<Block>(longest_chain_frontier_block_id, node_id, txns_to_be_in_new_block);
    Node::start_mining_new_block(new_block);
}

void ClassicNode::receive(std::shared_ptr<Block> block, NodeID from)
{
    if (all_block_info.count(block->block_id) > 0)
        return;
    if (block->parent_block_id == NO_BLOCK_ID) {
        /*
         * genesis block
         */
        all_block_info[block->block_id] = { block, simulator.get_time(), 1, NO_BLOCK_ID };
        frontier_block_info[block->block_id].balance = std::map<NodeID, double> {
            { block->miner, BLOCK_REWARD },
        };
        /*
         * update longest_chain_frontier_block_id
         * start mining next block
         */
        longest_chain_frontier_block_id = block->block_id;
        start_mining_new_block();
        return;
    }

    simulator.log(node_id, "Received block " + std::to_string(block->block_id) + " with parent " + std::to_string(block->parent_block_id));

    BlockID const parent_block_id = block->parent_block_id;

    if (frontier_block_info.count(parent_block_id) > 0) {
        /*
         * continuing existing chain
         */
        auto [new_balance, new_txns] = frontier_block_info[parent_block_id];

        if (!apply_block(new_balance, new_txns, block)) {
            simulator.log(node_id, "Rejected block " + std::to_string(block->block_id));
            return;
        }

        for (auto txn : block->txns)
            all_txns.insert(txn->txn_id);

        /*
         * update frontier_info
         */
        frontier_block_info.erase(parent_block_id);
        frontier_block_info[block->block_id] = { new_balance, new_txns };

        all_block_info[parent_block_id].next_block = block->block_id;
        all_block_info[block->block_id] = {
            block,
            simulator.get_time(),
            all_block_info[parent_block_id].chain_length + 1,
            NO_BLOCK_ID,
        };

        if (parent_block_id == longest_chain_frontier_block_id) {
            /*
             * extending current longest chain
             */

            /*
             * erase block txns from mempool
             * update longest_chain_frontier_block_id
             * start mining next block
             */
            for (auto txn : block->txns)
                mempool.erase(txn->txn_id);
            longest_chain_frontier_block_id = block->block_id;
            start_mining_new_block();

        } else if (all_block_info[longest_chain_frontier_block_id].chain_length < all_block_info[block->block_id].chain_length) {
            /*
             * longest chain has changed
             */

            std::vector<TxnID> txns_to_remove_from_mempool;
            std::map<TxnID, std::shared_ptr<Txn>> txns_to_add_to_mempool;

            /*
             * mempool has to be modified:
             *  - txns of new chain have to be removed from mempool
             *  - txns of old chain have to be added to mempool
             */

            /*
             * we first backstep along new chain (ie the longer chain)
             * by the difference in lengths of two chains
             */
            BlockID new_chain_block_id = block->block_id;
            size_t length_difference = all_block_info[block->block_id].chain_length - all_block_info[longest_chain_frontier_block_id].chain_length;
            for (size_t i = 0; i < length_difference; ++i) {
                auto const& new_chain_block = all_block_info[new_chain_block_id].block;
                for (auto const& txn : new_chain_block->txns)
                    txns_to_remove_from_mempool.push_back(txn->txn_id);
                new_chain_block_id = new_chain_block->parent_block_id;
            }
            BlockID old_chain_block_id = longest_chain_frontier_block_id;

            /*
             * now we single-step old_chain_block_id and new_chain_block_id until we arrive at last common block
             */
            while (old_chain_block_id != new_chain_block_id) {
                {
                    auto const& old_chain_block = all_block_info[old_chain_block_id].block;
                    for (auto const& txn : old_chain_block->txns)
                        txns_to_add_to_mempool[txn->txn_id] = txn;
                    old_chain_block_id = old_chain_block->parent_block_id;
                }
                {
                    auto const& new_chain_block = all_block_info[new_chain_block_id].block;
                    for (auto const& txn : new_chain_block->txns)
                        txns_to_remove_from_mempool.push_back(txn->txn_id);
                    new_chain_block_id = new_chain_block->parent_block_id;
                }
            }

            /*
             * here the order is important
             */
            mempool.insert(txns_to_add_to_mempool.begin(), txns_to_add_to_mempool.end());
            for (auto const& txn_id : txns_to_remove_from_mempool)
                mempool.erase(txn_id);

            /*
             * update longest_chain_frontier_block_id
             * start mining next block
             */
            longest_chain_frontier_block_id = block->block_id;
            start_mining_new_block();
        }

    } else if (all_block_info.count(parent_block_id) > 0) {
        /*
         * forking from old block
         */
        BlockID frontier = parent_block_id;
        BlockID q = frontier;
        while ((q = all_block_info[q].next_block) != NO_BLOCK_ID)
            frontier = q;

        auto [new_balance, new_txns] = frontier_block_info[frontier];
        BlockID old_block_id = frontier;
        auto old_block = all_block_info[old_block_id].block;
        for (; old_block_id != parent_block_id; old_block_id = old_block->parent_block_id, old_block = all_block_info[old_block_id].block)
            rollback_block(new_balance, new_txns, old_block);

        /*
         * now new_balance is balance after frontier
         * starting new chain from frontier
         */
        if (!apply_block(new_balance, new_txns, block)) {
            simulator.log(node_id, "Rejected block " + std::to_string(block->block_id));
            return;
        }
        frontier_block_info[block->block_id] = { new_balance, new_txns };
        all_block_info[block->block_id] = {
            block,
            simulator.get_time(),
            all_block_info[parent_block_id].chain_length + 1,
            NO_BLOCK_ID,
        };

        /*
         * longest chain cannot change when a fork is created
         */
    } else {
        /*
         * this is an orphan block
         */
        if (orphan_block_info_by_parent.count(parent_block_id) == 0)
            orphan_block_info_by_parent[parent_block_id] = {};
        orphan_block_info_by_parent[parent_block_id].insert(OrphanBlockInfo { block, from });
        simulator.log(node_id, "Marked orphan block " + std::to_string(block->block_id));
        return;
    }

    /*
     * loopless block forwarding
     */
    for (auto neighbor : simulator.get_neighbors(node_id))
        if (neighbor != from)
            simulator.deliver_sendable(node_id, block, neighbor);

    /*
     * clear out any children that were orphans
     */
    if (orphan_block_info_by_parent.count(block->block_id) > 0) {
        for (auto obi : orphan_block_info_by_parent[block->block_id])
            receive(obi.block, obi.from);
        orphan_block_info_by_parent.erase(block->block_id);
    }
}

void ClassicNode::receive(std::shared_ptr<Txn> t, NodeID from)
{
    if (all_txns.count(t->txn_id) > 0)
        return;

    all_txns.insert(t->txn_id);
    mempool[t->txn_id] = t;

    /*
     * loopless txn forwarding
     */
    for (auto neighbor : simulator.get_neighbors(node_id))
        if (neighbor != from)
            simulator.deliver_sendable(node_id, t, neighbor);
}

void ClassicNode::process(std::shared_ptr<CreateTxn> create_txn)
{
    /*
     * check that we have enough coins to create a txn
     */
    auto const& balance = frontier_block_info[longest_chain_frontier_block_id].balance;
    if (balance.count(node_id) == 0 || balance.at(node_id) == 0.0)
        return;

    /*
     * generate new txn
     */
    NodeID recipient = simulator.get_random_peer(node_id);
    double amount = std::uniform_real_distribution<double>(0.0, balance.at(node_id))(simulator.rng);
    std::shared_ptr<Txn> txn = std::make_shared<Txn>(node_id, recipient, amount);
    all_txns.insert(txn->txn_id);
    mempool[txn->txn_id] = txn;

    /*
     * send txn to neighbors
     */
    for (auto neighbor : simulator.get_neighbors(node_id))
        simulator.deliver_sendable(node_id, txn, neighbor);
}

// Arohan
void ClassicNode::process(std::shared_ptr<MineBlock> mine_block)
{
    auto mined_block = mine_block->mined_block;

    if (mined_block->parent_block_id != longest_chain_frontier_block_id) {
        simulator.log(node_id, "Discarded block " + std::to_string(mined_block->block_id));
        return;
    }

    auto [new_balance, new_txns] = frontier_block_info[longest_chain_frontier_block_id];
    assert(apply_block(new_balance, new_txns, mined_block));

    /*
     * update block_info
     */
    auto& last_info = all_block_info[longest_chain_frontier_block_id];
    all_block_info[mined_block->block_id] = { mined_block, simulator.get_time(), last_info.chain_length + 1, NO_BLOCK_ID };
    last_info.next_block = mined_block->block_id;

    /*
     * update frontier_info
     */
    frontier_block_info.erase(longest_chain_frontier_block_id);
    frontier_block_info[mined_block->block_id] = { new_balance, new_txns };

    /*
     * send mined block to neighbors
     */
    simulator.log(node_id, "Mined block " + std::to_string(mined_block->block_id) + " from parent " + std::to_string(mined_block->parent_block_id));
    for (auto neighbor : simulator.get_neighbors(node_id))
        simulator.deliver_sendable(node_id, mined_block, neighbor);

    /*
     * erase block txns from mempool
     * update longest_chain_frontier_block_id
     * start mining next block
     */
    for (auto txn : mined_block->txns)
        mempool.erase(txn->txn_id);
    longest_chain_frontier_block_id = mined_block->block_id;
    start_mining_new_block();
}
// Arohan
