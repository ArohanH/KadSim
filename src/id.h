#ifndef ID_H
#define ID_H

#include <cstddef>

using BlockID = size_t;
using NodeID = size_t;
using TxnID = size_t;
using ChunkID = size_t;
using LbitID = size_t;

BlockID constexpr NO_BLOCK_ID = -1;
NodeID constexpr NO_NODE_ID = -1;

#endif // ID_H
