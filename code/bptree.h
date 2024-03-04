#ifndef BPTREE_H
#define BPTREE_H

#include "constants.h"
#include "disk.h"
#include <set>

class BPTree {
    public:
        uint32_t numRecords;
        uint32_t numDataBlocks;
        uint32_t currentFreeBlock;
        uint32_t numLevels;
        uint32_t numLeaf;
        uint32_t numInternal;
        uint32_t numOverflow;
        std::set<uint32_t> keySet;
        RecordPointer* rootPointer;

        BPTree(uint32_t _numRecords, uint32_t _numDataBlocks, uint32_t _currentFreeBlock) : numRecords(_numRecords), numDataBlocks(_numDataBlocks), currentFreeBlock(_currentFreeBlock), rootPointer(new RecordPointer()), numLevels(1), numLeaf(1) {};
        ~BPTree();

        void Insert(Disk* disk, int key, RecordPointer pointer);
        IndexedSearchResult Search(Disk* disk, int min, int max);
        void Delete(Disk* disk, uint32_t key);
        void VerifyTree(Disk* disk);
};

#endif