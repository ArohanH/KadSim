#ifndef STATS_H
#define STATS_H

#include "id.h"

#include <map>
#include <set>

struct Stats {
    BlockID longest_chain_end;
    std::set<BlockID> longest_chain;
    std::map<BlockID, BlockID> block_parents;

    std::map<BlockID, size_t> chain_terminal_lengths;
    std::map<BlockID, size_t> branch_terminal_lengths;
};

Stats compute_stats(std::map<BlockID, std::set<BlockID>> const& global_block_tree);

#endif // STATS_H
