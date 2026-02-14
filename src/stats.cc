#include "assert.h"
#include "id.h"
#include "stats.h"

#include <cstddef>
#include <limits>
#include <map>
#include <set>

void update_chain_lengths(BlockID target, std::map<BlockID, BlockID> const& block_parents, std::map<BlockID, size_t>& chain_lengths)
{
    if (chain_lengths.count(target) > 0)
        return;
    else if (block_parents.count(target) == 0) {
        /*
         * genesis block
         */
        assert(chain_lengths.count(target) == 0);
        chain_lengths[target] = 1;
        return;
    }
    BlockID parent = block_parents.at(target);
    update_chain_lengths(parent, block_parents, chain_lengths);
    chain_lengths[target] = chain_lengths[parent] + 1;
}

Stats compute_stats(std::map<BlockID, std::set<BlockID>> const& global_block_tree)
{
    /*
     * block_parents contains all blocks except genesis
     */
    std::map<BlockID, BlockID> block_parents;
    for (auto const& [block_id, successors] : global_block_tree) {
        for (auto const& successor_id : successors)
            if (block_parents.count(successor_id) == 0)
                block_parents[successor_id] = block_id;
    }

    std::map<BlockID, size_t> chain_lengths;
    for (auto [block_id, _] : block_parents)
        update_chain_lengths(block_id, block_parents, chain_lengths);

    BlockID longest_chain_end;
    size_t max_len = std::numeric_limits<size_t>::min();
    for (auto [block_id, chain_length] : chain_lengths) {
        if (max_len < chain_length) {
            max_len = chain_length;
            longest_chain_end = block_id;
        }
    }

    std::map<BlockID, size_t> chain_terminal_lengths;
    for (auto [block_id, chain_length] : chain_lengths)
        if (global_block_tree.count(block_id) == 0)
            chain_terminal_lengths[block_id] = chain_length;

    std::set<BlockID> longest_chain;
    BlockID p = longest_chain_end;
    auto it = block_parents.find(p);
    while (it != block_parents.end()) {
        longest_chain.insert(p);
        p = it->second;
        it = block_parents.find(p);
    }
    longest_chain.insert(p);

    std::map<BlockID, size_t> branch_terminal_lengths;
    for (auto [block_id, _] : chain_terminal_lengths) {
        size_t branch_terminal_length = 0;
        BlockID p = block_id;
        while (longest_chain.count(p) == 0) {
            p = block_parents[p];
            branch_terminal_length++;
        }
        branch_terminal_lengths[block_id] = branch_terminal_length;
    }

    return { longest_chain_end, longest_chain, block_parents, chain_terminal_lengths, branch_terminal_lengths };
}
