#ifndef KADCAST_NODE_H
#define KADCAST_NODE_H

#include "classic_node.h"
#include "id.h"

#include <map>
#include <memory>
#include <set>
#include <vector>

class KadcastNode : public ClassicNode {
private:
    struct ChunkAssembly {
        NodeID from;
        size_t broadcast_height;
        size_t total_chunks;
        std::vector<bool> received_chunks;
        std::shared_ptr<Block> block;
        std::shared_ptr<Txn> txn;
        std::map<size_t, std::vector<NodeID>> cached_delegates;

        ChunkAssembly(NodeID from, size_t broadcast_height, size_t total_chunks)
            : from(from), broadcast_height(broadcast_height), total_chunks(total_chunks), received_chunks(total_chunks, false)
        {
        }

        bool complete() const
        {
            for (bool received : received_chunks)
                if (!received)
                    return false;
            return true;
        }
    };

    size_t const lbit_width;
    size_t const redundancy_factor;
    double const proximity_alpha;
    bool const adaptive_beta;
    bool const dynamic_beta;
    std::vector<std::vector<NodeID>> buckets;
    std::map<BlockID, ChunkAssembly> pending_block_chunks;
    std::map<TxnID, ChunkAssembly> pending_txn_chunks;
    std::map<BlockID, size_t> delegated_block_heights;
    std::map<TxnID, size_t> delegated_txn_heights;
    std::set<BlockID> cutthrough_forwarded_blocks;

    // Dynamic β state: current effective β, adjusted per-broadcast
    size_t dynamic_beta_current;
    static constexpr size_t DYNAMIC_BETA_MIN = 1;
    static constexpr size_t DYNAMIC_BETA_MAX = 10;
    static constexpr double DYNAMIC_BETA_W_HIGH = 50.0;  // ms: increase threshold
    static constexpr double DYNAMIC_BETA_W_LOW  = 5.0;   // ms: decrease threshold

    size_t effective_beta(size_t bucket_index, size_t max_height) const;

    std::vector<NodeID> select_delegates(size_t bucket_index, size_t max_height) const;
    void broadcast_block_via_kadcast(std::shared_ptr<Block> block, size_t max_height);
    void broadcast_txn_via_kadcast(std::shared_ptr<Txn> txn, size_t max_height);
    void send_block_to_delegate(std::shared_ptr<Block> block, NodeID delegate, size_t delegated_height);
    void send_txn_to_delegate(std::shared_ptr<Txn> txn, NodeID delegate, size_t delegated_height);

public:
    LbitID const lbit_id;

    KadcastNode(Simulator& simulator, bool is_slow_node, bool is_lowcpu_node, double txn_interarrival_time_mean, double block_interarrival_time_mean, double block_validation_throughput_kb_per_ms, LbitID lbit_id, size_t lbit_width, size_t redundancy_factor, double proximity_alpha = 1.0, bool adaptive_beta = false, bool dynamic_beta = false);

    void initialize_routing(std::map<NodeID, LbitID> const& node_lbit_ids);
    void receive(std::shared_ptr<Chunk> chunk, NodeID from) override;

protected:
    void forward_block(std::shared_ptr<Block> block, NodeID from) override;
    void forward_txn(std::shared_ptr<Txn> txn, NodeID from) override;
    void announce_local_block(std::shared_ptr<Block> block) override;
    void announce_local_txn(std::shared_ptr<Txn> txn) override;
};

#endif // KADCAST_NODE_H