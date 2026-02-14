#ifndef TXN_BLOCK_H
#define TXN_BLOCK_H

#include "assert.h"
#include "node.h"
#include "simulator.h"

#include <memory>
#include <vector>

class Txn : public Sendable {
private:
    static size_t next_txn_id;

public:
    static size_t constexpr SIZE = 8;

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
        n->receive(std::static_pointer_cast<Txn>(shared_from_this()), from);
    }
};

class Block : public Sendable {
private:
    static size_t next_block_id;

private:
    static size_t constexpr MAX_SIZE = 8000;
    static size_t constexpr EMPTY_SIZE = 8;

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
        n->receive(std::static_pointer_cast<Block>(shared_from_this()), from);
    }
};

#endif // TXN_BLOCK_H
