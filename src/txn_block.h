#ifndef TXN_BLOCK_H
#define TXN_BLOCK_H

#include "assert.h"
#include "node.h"
#include "simulator.h"

#include <algorithm>
#include <memory>
#include <vector>

class Txn : public Sendable {
private:
    static size_t next_txn_id;

public:
    static size_t constexpr SIZE = 1;

    Txn(NodeID payer, NodeID recipient, double amount)
        : txn_id(next_txn_id++), payer(payer), recipient(recipient), amount(amount)
    {
    }
    TxnID const txn_id;
    NodeID const payer;
    NodeID const recipient;
    double const amount;

    size_t size_kb() const override
    {
        return SIZE;
    }
    void receive(std::shared_ptr<Node> n, NodeID from) override
    {
        n->receive_from_network(std::static_pointer_cast<Txn>(shared_from_this()), from);
    }
};

class Block : public Sendable {
private:
    static size_t next_block_id;

private:
    static size_t constexpr MAX_SIZE = 1000;
    static size_t constexpr EMPTY_SIZE = 1;

public:
    static size_t constexpr MAX_NR_TXNS = (MAX_SIZE - EMPTY_SIZE) / Txn::SIZE;

    Block(BlockID const parent_block_id, NodeID miner, std::vector<std::shared_ptr<Txn>> txns)
        : parent_block_id(parent_block_id), block_id(next_block_id++), miner(miner), txns(txns)
    {
        assert(size_kb() <= MAX_SIZE);
    }

    BlockID const parent_block_id;
    BlockID const block_id;
    NodeID const miner;
    std::vector<std::shared_ptr<Txn>> const txns;

    size_t size_kb() const override
    {
        return EMPTY_SIZE + (txns.size() * Txn::SIZE);
    }
    void receive(std::shared_ptr<Node> n, NodeID from) override
    {
        n->receive_from_network(std::static_pointer_cast<Block>(shared_from_this()), from);
    }
};

class Chunk : public Sendable {
private:
    static size_t compute_chunk_size(size_t payload_size_kb, size_t chunk_index)
    {
        size_t remaining = payload_size_kb - (chunk_index * MAX_SIZE);
        return std::min(MAX_SIZE, remaining);
    }

public:
    static size_t constexpr MAX_SIZE = 500;

    Chunk(std::shared_ptr<Block> block, size_t broadcast_height, size_t chunk_index)
        : broadcast_height(broadcast_height),
          chunk_index(chunk_index),
          total_chunks((block->size_kb() + MAX_SIZE - 1) / MAX_SIZE),
          block(block), txn(nullptr)
    {
        assert(chunk_index < total_chunks);
    }

    Chunk(std::shared_ptr<Txn> txn, size_t broadcast_height)
        : broadcast_height(broadcast_height), chunk_index(0), total_chunks(1), block(nullptr), txn(txn)
    {
    }

    size_t const broadcast_height;
    size_t const chunk_index;
    size_t const total_chunks;
    std::shared_ptr<Block> const block;
    std::shared_ptr<Txn> const txn;

    bool is_block() const
    {
        return block != nullptr;
    }
    BlockID block_id() const
    {
        assert(is_block());
        return block->block_id;
    }
    TxnID txn_id() const
    {
        assert(!is_block());
        return txn->txn_id;
    }

    size_t size_kb() const override
    {
        if (is_block())
            return compute_chunk_size(block->size_kb(), chunk_index);
        return txn->size_kb();
    }
    void receive(std::shared_ptr<Node> n, NodeID from) override
    {
        n->receive_from_network(std::static_pointer_cast<Chunk>(shared_from_this()), from);
    }
};

#endif // TXN_BLOCK_H
