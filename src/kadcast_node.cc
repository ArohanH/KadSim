#include "kadcast_node.h"

#include "assert.h"
#include "txn_block.h"

#include <algorithm>
#include <numeric>
#include <random>

namespace {
size_t xor_distance_bucket(LbitID left, LbitID right)
{
    LbitID distance = left ^ right;
    assert(distance > 0);

    size_t bucket_index = 0;
    while ((distance >>= 1) > 0)
        ++bucket_index;
    return bucket_index;
}
}

KadcastNode::KadcastNode(Simulator& simulator, bool is_slow_node, bool is_lowcpu_node, double txn_interarrival_time_mean, double block_interarrival_time_mean, double block_validation_throughput_kb_per_ms, LbitID lbit_id, size_t lbit_width, size_t redundancy_factor, double proximity_alpha, bool adaptive_beta, bool dynamic_beta)
    : ClassicNode(simulator, is_slow_node, is_lowcpu_node, txn_interarrival_time_mean, block_interarrival_time_mean, block_validation_throughput_kb_per_ms),
      lbit_width(lbit_width),
      redundancy_factor(redundancy_factor),
      proximity_alpha(proximity_alpha),
      adaptive_beta(adaptive_beta),
      dynamic_beta(dynamic_beta),
      buckets(lbit_width),
      dynamic_beta_current(redundancy_factor),
      lbit_id(lbit_id)
{
}

void KadcastNode::initialize_routing(std::map<NodeID, LbitID> const& node_lbit_ids)
{
    for (auto& bucket : buckets)
        bucket.clear();

    for (auto const& [other_node_id, other_lbit_id] : node_lbit_ids) {
        if (other_node_id == node_id)
            continue;
        buckets[xor_distance_bucket(lbit_id, other_lbit_id)].push_back(other_node_id);
    }
}

size_t KadcastNode::effective_beta(size_t bucket_index, size_t max_height) const
{
    size_t base = redundancy_factor;

    // Dynamic β: AIMD based on this node's recent upload contention
    if (dynamic_beta) {
        double recent_wait = simulator.get_node_recent_upload_wait(node_id, 20);
        // Use the mutable dynamic_beta_current stored on this node.
        // We cast away const here because effective_beta is logically
        // a query but dynamic_beta_current is simulation state.
        auto* self = const_cast<KadcastNode*>(this);
        if (recent_wait > DYNAMIC_BETA_W_HIGH && self->dynamic_beta_current > DYNAMIC_BETA_MIN)
            --self->dynamic_beta_current;
        else if (recent_wait < DYNAMIC_BETA_W_LOW && self->dynamic_beta_current < DYNAMIC_BETA_MAX)
            ++self->dynamic_beta_current;
        base = self->dynamic_beta_current;
    }

    // Adaptive β per level: scale linearly with bucket index
    // High buckets (far XOR regions, large subtrees) get full β,
    // low buckets (nearby, few nodes) get β=1.
    if (adaptive_beta && max_height > 1) {
        return std::max(size_t{1}, base * (bucket_index + 1) / max_height);
    }

    return base;
}

std::vector<NodeID> KadcastNode::select_delegates(size_t bucket_index, size_t max_height) const
{
    auto const& candidates = buckets.at(bucket_index);
    if (candidates.empty())
        return {};

    size_t beta = effective_beta(bucket_index, max_height);

    // Hybrid delegate selection parameterized by proximity_alpha:
    //   score = alpha * uniform_random + (1 - alpha) * normalized_prop_delay
    // alpha=1.0 → pure random (standard KADcast)
    // alpha=0.0 → pure proximity (nearest underlay delegates)
    double max_dist = 0.0;
    for (auto c : candidates) {
        double d = simulator.get_path_distance(node_id, c);
        if (d > max_dist)
            max_dist = d;
    }
    if (max_dist < 1e-9)
        max_dist = 1.0;

    std::uniform_real_distribution<double> unif(0.0, 1.0);
    std::vector<std::pair<double, NodeID>> scored;
    scored.reserve(candidates.size());
    for (auto c : candidates) {
        double prop_norm = simulator.get_path_distance(node_id, c) / max_dist;
        double score = proximity_alpha * unif(simulator.rng) + (1.0 - proximity_alpha) * prop_norm;
        scored.push_back({ score, c });
    }

    std::sort(scored.begin(), scored.end());

    std::vector<NodeID> delegates;
    size_t count = std::min(scored.size(), beta);
    for (size_t i = 0; i < count; ++i)
        delegates.push_back(scored[i].second);
    return delegates;
}

void KadcastNode::send_block_to_delegate(std::shared_ptr<Block> block, NodeID delegate, size_t delegated_height)
{
    size_t total_chunks = (block->size_kb() + Chunk::MAX_SIZE - 1) / Chunk::MAX_SIZE;
    for (size_t chunk_index = 0; chunk_index < total_chunks; ++chunk_index)
        simulator.deliver_sendable_direct(node_id, std::make_shared<Chunk>(block, delegated_height, chunk_index), delegate);
}

void KadcastNode::send_txn_to_delegate(std::shared_ptr<Txn> txn, NodeID delegate, size_t delegated_height)
{
    simulator.deliver_sendable_direct(node_id, std::make_shared<Chunk>(txn, delegated_height), delegate);
}

void KadcastNode::broadcast_block_via_kadcast(std::shared_ptr<Block> block, size_t max_height)
{
    size_t upper_bound = std::min(max_height, lbit_width);
    for (size_t i = 0; i < upper_bound; ++i)
        for (auto delegate : select_delegates(i, upper_bound))
            send_block_to_delegate(block, delegate, i);
}

void KadcastNode::broadcast_txn_via_kadcast(std::shared_ptr<Txn> txn, size_t max_height)
{
    size_t upper_bound = std::min(max_height, lbit_width);
    for (size_t i = 0; i < upper_bound; ++i)
        for (auto delegate : select_delegates(i, upper_bound))
            send_txn_to_delegate(txn, delegate, i);
}

void KadcastNode::receive(std::shared_ptr<Chunk> chunk, NodeID from)
{
    if (chunk->is_block()) {
        BlockID block_id = chunk->block_id();
        if (knows_block(block_id))
            return;

        auto it = pending_block_chunks.try_emplace(block_id, from, chunk->broadcast_height, chunk->total_chunks).first;
        auto& assembly = it->second;
        assembly.block = chunk->block;

        // Skip duplicate chunks
        if (assembly.received_chunks.at(chunk->chunk_index))
            return;
        assembly.received_chunks.at(chunk->chunk_index) = true;

        // Cut-through: immediately relay this chunk to delegates
        if (assembly.broadcast_height > 0) {
            // Cache delegates on first chunk so all chunks go to the same peers
            if (assembly.cached_delegates.empty()) {
                size_t upper_bound = std::min(assembly.broadcast_height, lbit_width);
                for (size_t bucket_index = 0; bucket_index < upper_bound; ++bucket_index)
                    assembly.cached_delegates[bucket_index] = select_delegates(bucket_index, upper_bound);
            }
            // Relay this chunk to all cached delegates
            for (auto const& [bucket_index, delegates] : assembly.cached_delegates) {
                for (auto delegate : delegates) {
                    auto relay_chunk = std::make_shared<Chunk>(chunk->block, bucket_index, chunk->chunk_index);
                    simulator.deliver_sendable_direct(node_id, relay_chunk, delegate);
                }
            }
        }

        if (!assembly.complete())
            return;

        // All chunks received — validate locally; forwarding already done via cut-through
        cutthrough_forwarded_blocks.insert(block_id);
        pending_block_chunks.erase(it);
        receive_from_network(chunk->block, from);
        return;
    }

    TxnID txn_id = chunk->txn_id();
    if (knows_txn(txn_id))
        return;

    auto it = pending_txn_chunks.try_emplace(txn_id, from, chunk->broadcast_height, chunk->total_chunks).first;
    auto& assembly = it->second;
    assembly.txn = chunk->txn;
    assembly.received_chunks.at(chunk->chunk_index) = true;

    if (!assembly.complete())
        return;

    delegated_txn_heights[txn_id] = assembly.broadcast_height;
    pending_txn_chunks.erase(it);
    receive_from_network(chunk->txn, from);
}

void KadcastNode::forward_block(std::shared_ptr<Block> block, NodeID from)
{
    (void)from;
    // If already forwarded via cut-through, skip re-broadcasting
    auto ct_it = cutthrough_forwarded_blocks.find(block->block_id);
    if (ct_it != cutthrough_forwarded_blocks.end()) {
        cutthrough_forwarded_blocks.erase(ct_it);
        delegated_block_heights.erase(block->block_id);
        return;
    }
    // Fallback for locally mined blocks (not received via chunks)
    size_t max_height = lbit_width;
    auto it = delegated_block_heights.find(block->block_id);
    if (it != delegated_block_heights.end()) {
        max_height = it->second;
        delegated_block_heights.erase(it);
    }
    broadcast_block_via_kadcast(block, max_height);
}

void KadcastNode::forward_txn(std::shared_ptr<Txn> txn, NodeID from)
{
    (void)from;
    size_t max_height = lbit_width;
    auto it = delegated_txn_heights.find(txn->txn_id);
    if (it != delegated_txn_heights.end()) {
        max_height = it->second;
        delegated_txn_heights.erase(it);
    }
    broadcast_txn_via_kadcast(txn, max_height);
}

void KadcastNode::announce_local_block(std::shared_ptr<Block> block)
{
    broadcast_block_via_kadcast(block, lbit_width);
}

void KadcastNode::announce_local_txn(std::shared_ptr<Txn> txn)
{
    broadcast_txn_via_kadcast(txn, lbit_width);
}
