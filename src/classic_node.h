#ifndef CLASSIC_NODE_H
#define CLASSIC_NODE_H

#include "id.h"
#include "node.h"

#include <map>
#include <memory>
#include <set>

class ClassicNode : public Node {
private:
    struct AllBlockInfo {
        std::shared_ptr<Block> block;
        double arrival_time;
        size_t chain_length;
        BlockID next_block;
    };
    /*
     * information of every valid block this node has seen
     */
    std::map<BlockID, AllBlockInfo> all_block_info;

    struct FrontierBlockInfo {
        std::map<NodeID, double> balance;
        std::set<TxnID> txns;
    };
    /*
     * information of valid blocks at the ends of chains
     */
    std::map<BlockID, FrontierBlockInfo> frontier_block_info;

    struct OrphanBlockInfo {
        std::shared_ptr<Block> block;
        NodeID from;
        bool operator<(OrphanBlockInfo const& o) const
        {
            return from < o.from || (from == o.from && block < o.block);
        }
    };
    /*
     * information of orphan blocks received
     * mapped by claimed parent ID
     * THESE HAVE NOT BEEN PROCESSED
     */
    std::map<BlockID, std::set<OrphanBlockInfo>> orphan_block_info_by_parent;

    // Arohan
    /*
     * block id of the last block of current longest chain
     */
    BlockID longest_chain_frontier_block_id;
    /*
     * set of all txn ids
     */
    std::set<TxnID> all_txns;
    /*
     * mempool of the node
     */
    std::map<TxnID, std::shared_ptr<Txn>> mempool;
    // Arohan

protected:
    void start_mining_new_block();
    virtual void forward_block(std::shared_ptr<Block> block, NodeID from);
    virtual void forward_txn(std::shared_ptr<Txn> txn, NodeID from);
    virtual void announce_local_block(std::shared_ptr<Block> block);
    virtual void announce_local_txn(std::shared_ptr<Txn> txn);
    bool knows_block(BlockID block_id) const;
    bool knows_txn(TxnID txn_id) const;

public:
    bool const is_slow_node;
    bool const is_lowcpu_node;

    ClassicNode(Simulator& simulator, bool is_slow_node, bool is_lowcpu_node, double txn_interarrival_time_mean, double block_interarrival_time_mean, double block_validation_throughput_kb_per_ms);

    /*
     * callbacks for receiving blocks, txns
     */
    virtual void receive(std::shared_ptr<Block> block, NodeID from) override;
    void receive(std::shared_ptr<Txn> txn, NodeID from) override;

    /*
     * callbacks for mine_block, create_txn timers
     */
    virtual void process(std::shared_ptr<MineBlock> create_block) override;
    void process(std::shared_ptr<CreateTxn> create_txn) override;

    /*
     * utils for data collection
     */
    std::map<BlockID, std::set<BlockID>> collect_block_tree() const;
    std::pair<BlockID, std::set<BlockID>> collect_frontier() const;
    std::map<BlockID, double> collect_block_arrival_times() const;
    std::map<BlockID, NodeID> collect_block_miners() const;
};

#endif // CLASSIC_NODE_H
