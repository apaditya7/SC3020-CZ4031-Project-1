#include <iomanip>
#include <sstream>
#include <iostream>
#include <fstream>
#include <stack>
#include <queue>
#include <set>
#include <chrono>
#include <algorithm>

#include "constants.h"
#include "disk.h"

using namespace std;

uint32_t numRecords;
uint32_t numDataBlocks;
uint32_t currentFreeBlock = FIRST_DATA_BLOCK;
uint32_t LAST_DATA_BLOCK;

void Experiment1(Disk* disk, string filename) {
    cout << "Running Experiment 1" << endl;
    ifstream tsv(filename);
    assert(tsv.good());
    
    string line;
    getline(tsv, line); // consume the header line
    
    DataBlock* block = (DataBlock*)disk->ReadBlock(currentFreeBlock);
    uint32_t index = 0;
    
    while(getline(tsv, line)) {
        if(line.empty()) continue; // ignore empty lines
        numRecords++;

        size_t tabPos1 = line.find("\t");
        size_t tabPos2 = line.substr(tabPos1+1).find("\t");

        size_t idSize = sizeof(block->records[index].tconst);
        memcpy(block->records[index].tconst, line.c_str(), idSize);

        // replace tab with 0 in case its copied as the last character for ids of length 9
        if (tabPos1 == idSize - 1) block->records[index].tconst[idSize - 1] = 0; 

        block->records[index].occupied = true;
        block->records[index].averageRating = std::stof(line.substr(tabPos1 + 1, tabPos2));
        block->records[index].numVotes = std::stoi(line.substr(tabPos1 + tabPos2 + 2));

        ++index;
        if(index == RECORDS_PER_BLOCK) {
            disk->WriteBlock(currentFreeBlock, (uint8_t*)block);
            free(block);
            currentFreeBlock += 1;
            block = (DataBlock*)disk->ReadBlock(currentFreeBlock);
            index = 0;
        }
    }

    if(index != 0) {
        disk->WriteBlock(currentFreeBlock, (uint8_t*)block);
        currentFreeBlock += 1;
    }
    free(block);

    numDataBlocks = currentFreeBlock - 1;
    LAST_DATA_BLOCK = numDataBlocks;

    cout << "Statistics:" << endl;
    cout << "a) " << numRecords << " Records" << endl;
    cout << "b) Each record has a size of " << RECORD_SIZE <<  "B" << endl;
    cout << "c) " << RECORDS_PER_BLOCK << " Records per Block" << endl;
    cout << "d) " << numDataBlocks << " Blocks used" << endl;
}

uint32_t numLevels = 1;
uint32_t numLeaf = 1;
uint32_t numInternal;
uint32_t numDuplicates;
set<uint32_t> keySet;
RecordPointer* rootPointer;

void Insert(Disk* disk, int key, RecordPointer pointer) {
    keySet.insert(key);
    IndexBlock* block = (IndexBlock*)disk->ReadBlock(rootPointer->blockNumber);
    stack<IndexBlock*> blockStack;
    stack<RecordPointer> pointerStack;
    pointerStack.push(*rootPointer);
    while (block->nodeType != LEAF) {
        int i;
        for(i = 0; i < block->numKeys; i++) {
            if (key < block->keys[i]) break;
        }
        blockStack.push(block);
        pointerStack.push(block->pointers[i]);
        block = (IndexBlock*)disk->ReadBlock(block->pointers[i].blockNumber);
    }

    bool keyExists = false;
    int index;
    for(index = 0; index < block->numKeys; index++) {
        if(block->keys[index] == key) {
            keyExists = true;
            break;
        }
    }

    if(keyExists) {
        DuplicatesBlock* db = (DuplicatesBlock*)disk->ReadBlock(block->pointers[index].blockNumber);
        if(db->nodeType == DUPLICATES) {
            if(db->numKeys == 2*N) {
                numDuplicates++;
                DuplicatesBlock* newDb = (DuplicatesBlock*)calloc(BLOCK_SIZE, sizeof(uint8_t));
                newDb->nodeType = DUPLICATES;
                newDb->numKeys = 1;
                newDb->pointers[0] = pointer;
                newDb->next = (RecordPointer){block->pointers[index].blockNumber, 0};
                block->pointers[index] = (RecordPointer){currentFreeBlock, 0};
                disk->WriteBlock(pointerStack.top().blockNumber,(uint8_t*)block);
                pointerStack.pop();
                disk->WriteBlock(currentFreeBlock++, (uint8_t*)newDb);
                free(newDb);
            } else {
                db->pointers[db->numKeys++] = pointer;
                disk->WriteBlock(block->pointers[index].blockNumber, (uint8_t*)db);
            }
        } else {
            numDuplicates++;
            DuplicatesBlock* newDb = (DuplicatesBlock*)calloc(BLOCK_SIZE, sizeof(uint8_t));
            newDb->nodeType = DUPLICATES;
            newDb->numKeys = 2;
            newDb->pointers[0] = block->pointers[index];
            newDb->pointers[1] = pointer;
            newDb->next = (RecordPointer){0,0};
            block->pointers[index] = (RecordPointer){currentFreeBlock, 0};
            disk->WriteBlock(pointerStack.top().blockNumber,(uint8_t*)block);
            pointerStack.pop();
            disk->WriteBlock(currentFreeBlock++, (uint8_t*)newDb);
            free(newDb);
        }
        free(db);
        free(block);
    } else if(block->numKeys < N) {
        int i;
        for(i = block->numKeys; i > 0; i--){
            if (block->keys[i-1] > key) {
                block->keys[i] = block->keys[i-1];
                block->pointers[i] = block->pointers[i-1];
            } else break;
        }
        block->keys[i] = key;
        block->pointers[i] = pointer;
        block->numKeys++;
        disk->WriteBlock(pointerStack.top().blockNumber, (uint8_t*)block);
        free(block);
    } else {
        IndexBlock* newBlock = (IndexBlock*)calloc(BLOCK_SIZE, sizeof(uint8_t));
        newBlock->nodeType = LEAF;
        numLeaf++;

        uint32_t newKeys[N+1];
        RecordPointer newPointers[N+1];
        bool added = false;
        int c = 0;
        for(int i = 0; i < block->numKeys; i++){
            if (key < block->keys[i] && !added) {
                newKeys[c] = key;
                newPointers[c++] = pointer;
                added = true;
            }
            newKeys[c] = block->keys[i];
            newPointers[c++] = block->pointers[i];
        }

        if(!added) {
            newKeys[N] = key;
            newPointers[N] = pointer;
        }

        int k = ceil((N+1)/2.0f);
        for(int i = 0; i < k; i++) {
            block->keys[i] = newKeys[i];
            block->pointers[i] = newPointers[i];
        }
        block->numKeys = k;

        for(int i = k; i < N+1; i++) {
            newBlock->keys[i-k] = newKeys[i];
            newBlock->pointers[i-k] = newPointers[i];
            newBlock->numKeys++;
        }

        RecordPointer left = pointerStack.top();
        pointerStack.pop();
        RecordPointer right = (RecordPointer){currentFreeBlock, 0};
        uint32_t keyToAdd = newBlock->keys[0];
        bool split = true;

        newBlock->pointers[N] = block->pointers[N];
        block->pointers[N] = right;

        disk->WriteBlock(left.blockNumber, (uint8_t*)block);
        disk->WriteBlock(currentFreeBlock++, (uint8_t*)newBlock);
        free(newBlock);
        free(block);

        while(blockStack.size() > 0){
            IndexBlock* parent = blockStack.top();
            blockStack.pop();

            if(parent->numKeys < N) {
                int i;
                for(i = parent->numKeys; i > 0; i--){
                    if(parent->keys[i-1] > keyToAdd) {
                        parent->keys[i] = parent->keys[i-1];
                        parent->pointers[i+1] = parent->pointers[i];
                    } else break;
                }
                parent->keys[i] = keyToAdd;
                parent->pointers[i+1] = right;
                parent->numKeys++;
                split = false;
                disk->WriteBlock(pointerStack.top().blockNumber, (uint8_t*)parent);
                free(parent);
                break;
            } else {
                uint32_t newKeys[N+1];
                RecordPointer newPointers[N+1];
                bool added = false;
                int c = 0;
                for(int i = 0; i < parent->numKeys; i++){
                    if (key < parent->keys[i] && !added) {
                        newKeys[c] = keyToAdd;
                        newPointers[c++] = right;
                        added = true;
                    }
                    newKeys[c] = parent->keys[i];
                    newPointers[c++] = parent->pointers[i+1];
                }

                if(!added) {
                    newKeys[N] = keyToAdd;
                    newPointers[N] = right;
                }

                IndexBlock* newBlock = (IndexBlock*)calloc(BLOCK_SIZE, sizeof(uint8_t));
                newBlock->nodeType = INTERNAL;
                numInternal++;

                int k = ceil(N/2.0f);
                for(int i = 0; i < k; i++) {
                    parent->keys[i] = newKeys[i];
                    parent->pointers[i+1] = newPointers[i];
                }
                parent->numKeys = k;

                for(int i = k; i < N; i++) {
                    newBlock->keys[i-k] = newKeys[i+1];
                    newBlock->pointers[i-k] = newPointers[i];
                    newBlock->numKeys++;
                }
                newBlock->pointers[newBlock->numKeys] = newPointers[N];

                left = pointerStack.top();
                pointerStack.pop();
                right = (RecordPointer){currentFreeBlock, 0};
                keyToAdd = newKeys[k];

                disk->WriteBlock(left.blockNumber, (uint8_t*)parent);
                disk->WriteBlock(currentFreeBlock++, (uint8_t*)newBlock);
                free(parent);
                free(newBlock);
            }
        }

        if (split) {
            numLevels++;
            numInternal++;
            IndexBlock* newRoot = (IndexBlock*)calloc(BLOCK_SIZE, sizeof(uint8_t));
            newRoot->nodeType = INTERNAL;
            newRoot->pointers[0] = left;
            newRoot->pointers[1] = right;
            newRoot->keys[0] = keyToAdd;
            newRoot->numKeys = 1;
            *rootPointer = (RecordPointer){currentFreeBlock, 0};
            disk->WriteBlock(currentFreeBlock++, (uint8_t*)newRoot);
            free(newRoot);
        }
    }

    while(blockStack.size() > 0){
        free(blockStack.top());
        blockStack.pop();
    }
}

void Experiment2(Disk* disk) {
    cout << endl << "Running Experiment 2" << endl;
    rootPointer = new RecordPointer();
    rootPointer->blockNumber = currentFreeBlock++;

    IndexBlock* root = (IndexBlock*)disk->ReadBlock(rootPointer->blockNumber);
    root->nodeType = LEAF;
    root->numKeys = 0;
    disk->WriteBlock(rootPointer->blockNumber,(uint8_t*) root);
    free(root);

    for(uint32_t i = FIRST_DATA_BLOCK; i < LAST_DATA_BLOCK + FIRST_DATA_BLOCK; i++){
        DataBlock* dataBlock = (DataBlock*)disk->ReadBlock(i);
        for(uint32_t k = 0; k < RECORDS_PER_BLOCK; k++){
            if(dataBlock->records[k].occupied) {
                Insert(disk, dataBlock->records[k].numVotes, (RecordPointer){i, k});
            }
        }
        free(dataBlock);
    }

    cout << "Statistics:" << endl;
    cout << "a) N = " << N << endl;
    cout << "b) The tree has " << numLevels << " levels" << endl;
    cout << "c) " << numInternal << " Internal Nodes, " << numLeaf << " Leaf Nodes, " << numDuplicates << " Duplicate Nodes. Total is " << numInternal + numLeaf + numDuplicates << endl;

    IndexBlock* rootBlock = (IndexBlock*)disk->ReadBlock(rootPointer->blockNumber);
    cout << "d) Root has " << (uint32_t)rootBlock->numKeys << " keys:";
    for(int i = 0; i < rootBlock->numKeys; i++) {
        cout << " " << (uint32_t)rootBlock->keys[i];
    }
    free(rootBlock);
    cout << endl;
}

// Verifies if the tree is correct
void VerifyTree(Disk* disk) {
    cout << endl << "Verifying Tree… ";
    uint32_t nLeaf = 0;
    uint32_t nInternal = 0;
    uint32_t nDuplicates = 0;
    set<uint32_t> dataBlocks;
    set<uint32_t> pointers;
    set<uint32_t> internalKeySet;
    set<uint32_t> leafKeySet;

    queue<vData> q;
    q.push((vData){*rootPointer, 5, 2279223});
    bool skip = true; // skip min key amount check for the first time, i.e, for the root

    while(q.size() > 0) {
        vData data = q.front();
        q.pop();

        IndexBlock* block = (IndexBlock*)disk->ReadBlock(data.p.blockNumber);
        bool dontFree = false;
        if(block->nodeType == INTERNAL){
            nInternal++;
            ASSERT(block->keys[0] >= data.n1, "[INTERNAL] Minimum is %d, found %d", data.n1, block->keys[0]);
            ASSERT(block->keys[block->numKeys - 1] <= data.n2, "[INTERNAL] Maximum is %d, found %d", data.n2, block->keys[block->numKeys - 1]);
            ASSERT(skip || block->numKeys >= floor(N/2.0f), "[INTERNAL] Expected atleast floor(n/2) keys. Found %d", block->numKeys);
            skip = false;
            uint32_t min = data.n1;
            for(int i = 0; i < block->numKeys; i++) {
                ASSERT(internalKeySet.count(block->keys[i]) == 0, "[INTERNAL] Found duplicate key %d within internal nodes", block->keys[i]);
                internalKeySet.insert(block->keys[i]);
                if(i >= 1) ASSERT(block->keys[i] > block->keys[i-1], "[INTERNAL] Node not sorted (%d is after %d)", block->keys[i], block->keys[i-1]);
                q.push((vData){block->pointers[i],min,block->keys[i]-1});
                min = block->keys[i];
            }
            q.push((vData){block->pointers[block->numKeys],min,data.n2});


        } else if(block->nodeType == LEAF) {
            ASSERT(nInternal == numInternal, "Expected %d internal nodes, traveresed %d", numInternal, nInternal);
            nLeaf++;
            ASSERT((skip && block->numKeys == 0) || block->keys[0] >= data.n1, "[LEAF] Minimum is %d, found %d", data.n1, block->keys[0]);
            ASSERT((skip && block->numKeys == 0) || block->keys[block->numKeys - 1] <= data.n2, "[LEAF] Maximum is %d, found %d", data.n2, block->keys[block->numKeys - 1]);
            ASSERT(skip || block->numKeys >= floor((N+1)/2.0f), "[LEAF] Expected atleast floor((n+1)/2). Found %d", block->numKeys);
            IndexBlock* nextBlock = (IndexBlock*)disk->ReadBlock(q.front().p.blockNumber);
            ASSERT(skip || block->pointers[N].blockNumber == q.front().p.blockNumber || nextBlock->nodeType == DUPLICATES || nextBlock->nodeType == 0 || nextBlock->nodeType == 1, "[LEAF] next (%d) != q.next (%d)", block->pointers[N].blockNumber, q.front().p.blockNumber);
            free(nextBlock);
            skip = false;
            for(int i = 0; i < block->numKeys; i++) {
                ASSERT(leafKeySet.count(block->keys[i]) == 0, "[LEAD] Found duplicate key %d within leaf nodes", block->keys[i]);
                leafKeySet.insert(block->keys[i]);
                if(internalKeySet.count(block->keys[i])) internalKeySet.erase(block->keys[i]);
                if(i >= 1) ASSERT(block->keys[i] > block->keys[i-1], "[LEAF] Node not sorted (%d is after %d)", block->keys[i], block->keys[i-1]);
                q.push((vData){block->pointers[i],block->keys[i],0});
            }

        } else if(block->nodeType == DUPLICATES) {
            DuplicatesBlock* dB = (DuplicatesBlock*)block;
            nDuplicates++;
            ASSERT(dB->numKeys > 0 && (dB->numKeys > 1 || dB->next.blockNumber != 0), "Duplicate Block has %d keys!", dB->numKeys);
            for(int i = 0; i < dB->numKeys; i++) {
                q.push((vData){dB->pointers[i],data.n1,data.n2});
            }
            if(dB->next.blockNumber != 0) {
                while(dB->next.blockNumber != 0) {
                    nDuplicates++;
                    uint32_t nextBlock = dB->next.blockNumber;
                    free(dB);
                    dB = (DuplicatesBlock*)disk->ReadBlock(nextBlock);
                    ASSERT(dB->numKeys == N*2, "Inner Duplicate Block has %d keys", dB->numKeys);
                    for(int i = 0; i < dB->numKeys; i++) {
                        q.push((vData){dB->pointers[i],data.n1,data.n2});
                    }
                }
                free(dB);
                dontFree = true;
            }
        } else if(block->nodeType == 0 || block->nodeType == 1) {
            dataBlocks.insert(data.p.blockNumber);
            pointers.insert((data.p.blockNumber << 4) + data.p.recordIndex);
            uint32_t i = data.p.recordIndex;
            ASSERT(i >= 0 && i < RECORDS_PER_BLOCK, "Record Index %d is out of range!", i);
            DataBlock* dataBlock = (DataBlock*)block;
            ASSERT(dataBlock->records[i].numVotes == data.n1, "Record Mismatch! Expected %d but found %d", data.n1, dataBlock->records[i].numVotes);
        } else {
            ASSERT(0 == 1, "Invalid Block Type %d", block->nodeType);
        }
        if (!dontFree) free(block);
    }

    ASSERT(nLeaf == numLeaf, "Expected %d leaf nodes, traveresed %d", numLeaf, nLeaf);
    ASSERT(keySet == leafKeySet, "Mismatch of %d keys between expected keys and leaf keys", abs((int)(keySet.size() - leafKeySet.size())));
    ASSERT(internalKeySet.size() == 0, "Found %lu internal keys that do not appear in leaves", internalKeySet.size());
    ASSERT(nDuplicates == numDuplicates, "Expected %d duplicate nodes, traveresed %d", numDuplicates, nDuplicates);
    ASSERT(dataBlocks.size() == numDataBlocks, "Expected %u data blocks, traveresed %lu", numDataBlocks, dataBlocks.size());
    ASSERT(pointers.size() == numRecords, "Expected %u records, traveresed %lu", numRecords, pointers.size());

    cout << "Passed!" << endl;
}

IndexedSearchResult IndexedSearch(Disk* disk, int min, int max) {
    uint32_t nInternal = 0;
    uint32_t nLeaf = 0;
    uint32_t nDuplicates = 0;

    std::set<int> visitedData;
    uint32_t totalRecords = 0;
    float totalRating = 0;

    IndexBlock *block = (IndexBlock *)disk->ReadBlock(rootPointer->blockNumber);

    while (block->nodeType != LEAF) // search the key until leaf node
    {
        int i;
        for (i = 0; i < block->numKeys; i++)
        {
            if (min < block->keys[i])
                break;
        }
        uint32_t nextBlock = block->pointers[i].blockNumber;
        free(block);
        block = (IndexBlock *)disk->ReadBlock(nextBlock);
        nInternal++;
    }

    nLeaf++;

    int keyIndex;
    for (keyIndex = 0; keyIndex < block->numKeys; keyIndex++)
    {
        if (block->keys[keyIndex] >= min)
        {
            break;
        }
    }

    while(block->keys[keyIndex] <= max) {
        DuplicatesBlock *db = (DuplicatesBlock *)disk->ReadBlock(block->pointers[keyIndex].blockNumber);
        if(db->nodeType == DUPLICATES) {
            while (true) // to iterate through duplicatesblocksss
            {
                nDuplicates++;
                for (int j = 0; j < db->numKeys; j++) // to iterate the keys in a duplicatesblock
                {
                    visitedData.insert(db->pointers[j].blockNumber);
                    DataBlock *temp = (DataBlock *)disk->ReadBlock(db->pointers[j].blockNumber);
                    totalRating += temp->records[db->pointers[j].recordIndex].averageRating;
                    totalRecords++;
                    free(temp);
                }
                if (db->next.blockNumber == 0)
                {
                    break;
                }
                uint32_t nextBlock = db->next.blockNumber;
                free(db);
                db = (DuplicatesBlock *)disk->ReadBlock(nextBlock); // go to the next duplicatesblock
            }
        } else {
            DataBlock* temp = (DataBlock*)db;
            visitedData.insert(block->pointers[keyIndex].blockNumber);
            totalRating += temp->records[block->pointers[keyIndex].recordIndex].averageRating;
            totalRecords++;
        }
        free(db);

        keyIndex++;

        if(keyIndex == block->numKeys) {
            uint32_t nextBlock = block->pointers[N].blockNumber;
            if(block->keys[keyIndex] == max || nextBlock == 0) break;
            free(block);
            block = (IndexBlock*)disk->ReadBlock(nextBlock);
            nLeaf++;
            keyIndex = 0;
        }
    }

    free(block);

    return (IndexedSearchResult) { nInternal, nLeaf, nDuplicates, visitedData.size(), totalRecords, totalRating/(float)totalRecords };
}

SearchResult LinearSearch(Disk* disk, int min, int max) {
    uint32_t nBlocks = 0;
    uint32_t totalRecords = 0;
    float totalRating = 0;

    if(min == max) {
        for(int i = FIRST_DATA_BLOCK; i < LAST_DATA_BLOCK + FIRST_DATA_BLOCK; i++){
            DataBlock* block = (DataBlock*)disk->ReadBlock(i);
            nBlocks++;
            for(int k = 0; k < RECORDS_PER_BLOCK; k++){
                if(block->records[k].occupied) {
                    if(block->records[k].numVotes == min) {
                        totalRating += block->records[k].averageRating;
                        totalRecords++;
                    }
                }
            }
            free(block);
        }
    } else {
        for(int i = FIRST_DATA_BLOCK; i < LAST_DATA_BLOCK + FIRST_DATA_BLOCK; i++){
            DataBlock* block = (DataBlock*)disk->ReadBlock(i);
            nBlocks++;
            for(int k = 0; k < RECORDS_PER_BLOCK; k++){
                if(block->records[k].occupied) {
                    if(block->records[k].numVotes >= min && block->records[k].numVotes <= max) {
                        totalRating += block->records[k].averageRating;
                        totalRecords++;
                    }
                }
            }
            free(block);
        }
    }
    
    return (SearchResult) { nBlocks, totalRecords, totalRating/(float)totalRecords };
}

void PrintSearchResult(SearchResult sr, long long timeTaken) {
    cout << "> Linear Search Statistics" << endl;
    cout << "a) Data Blocks Accessed: " << sr.nData << endl;
    cout << "b) Found " << sr.recordsFound << " records" << endl;
    cout << "c) Average Rating: " << sr.averageRating << endl;
    cout << "d) Time Taken: " << timeTaken/1000.0f << "ms (median of " << TRIALS << " trials)" << endl;
}

void PrintSearchResult(IndexedSearchResult isr, long long timeTaken) {
    cout << "> Indexed Search Statistics" << endl;
    cout << "a) Index Blocks Accessed: " << isr.nInternal << " Internal, " << isr.nLeaf << " Leaf, " << isr.nDuplicates << " Duplicates. Total is " << isr.nInternal + isr.nLeaf + isr.nDuplicates << endl;
    cout << "b) Data Blocks Accessed: " << isr.nData << endl;
    cout << "c) Found " << isr.recordsFound << " records" << endl;
    cout << "d) Average Rating: " << isr.averageRating << endl;
    cout << "e) Time Taken: " << timeTaken/1000.0f << "ms (median of " << TRIALS << " trials)" << endl;
}

long long Median(vector<long long> timings) {
    vector<long long>::iterator it = timings.begin() + timings.size() / 2;
    nth_element(timings.begin(), it, timings.end());
    return timings[timings.size() / 2];
}

void Experiment3(Disk *disk)
{
    cout << endl;
    cout << "Running Experiment 3" << endl;
    cout << "Searching for [500]" << endl;
    
    SearchResult sr;
    IndexedSearchResult isr;
    chrono::steady_clock::time_point start, end;
    vector<long long> timings;

    for(int i = 0; i < TRIALS; i++) {
        start = chrono::steady_clock::now();
        isr = IndexedSearch(disk, 500, 500);
        end = chrono::steady_clock::now();
        timings.push_back(chrono::duration_cast<chrono::microseconds>(end - start).count());
    }
    
    PrintSearchResult(isr, Median(timings));
    timings.clear();
    
    for(int i = 0; i < TRIALS; i++) {
        start = chrono::steady_clock::now();
        sr = LinearSearch(disk, 500, 500);
        end = chrono::steady_clock::now();
        timings.push_back(chrono::duration_cast<chrono::microseconds>(end - start).count());
    }
    
    PrintSearchResult(sr,  Median(timings));
}

void Experiment4(Disk *disk)
{
    cout << endl;
    cout << "Running Experiment 4" << endl;
    cout << "Searching for [30000, 40000]" << endl;
    
    SearchResult sr;
    IndexedSearchResult isr;
    chrono::steady_clock::time_point start, end;
    vector<long long> timings;

    for(int i = 0; i < TRIALS; i++) {
        start = chrono::steady_clock::now();
        isr = IndexedSearch(disk, 30000, 40000);
        end = chrono::steady_clock::now();
        timings.push_back(chrono::duration_cast<chrono::microseconds>(end - start).count());
    }
    
    PrintSearchResult(isr, Median(timings));
    timings.clear();
    
    for(int i = 0; i < TRIALS; i++) {
        start = chrono::steady_clock::now();
        sr = LinearSearch(disk, 30000, 40000);
        end = chrono::steady_clock::now();
        timings.push_back(chrono::duration_cast<chrono::microseconds>(end - start).count());
    }
    
    PrintSearchResult(sr,  Median(timings));
}

uint32_t findInOrderSuccessor(Disk* disk, IndexBlock* node, uint32_t key) {
    int keyIndex = 0;
    while (keyIndex < node->numKeys && node->keys[keyIndex] <= key) keyIndex++;
    IndexBlock* current = (IndexBlock*)disk->ReadBlock(node->pointers[keyIndex].blockNumber);
    while (current->nodeType != LEAF) {
        RecordPointer nextBlock = current->pointers[0];
        free(current);
        current = (IndexBlock*)disk->ReadBlock(nextBlock.blockNumber);

    }
    uint32_t successor = current->keys[0];
    free(current);
    return successor;
}


int keyIndexInNode(IndexBlock* node, uint32_t key) {
    for (int i = 0; i < node->numKeys; i++) {
        if (node->keys[i] == key) return i;
    }
    return -1;
}

bool isEmpty(DataBlock* block) {
    for(int i = 0; i < RECORDS_PER_BLOCK; i++) {
        if(block->records[i].occupied) return false;
    }
    return true;
}

void Delete(Disk* disk, uint32_t key) {
    IndexBlock* block = (IndexBlock*)disk->ReadBlock(rootPointer->blockNumber);
    stack<IndexBlock*> blockStack;
    stack<RecordPointer> pointerStack;
    pointerStack.push(*rootPointer);
    while (block->nodeType != LEAF) {
        int i;
        for(i = 0; i < block->numKeys; i++) {
            if (key < block->keys[i]) break;
        }
        blockStack.push(block);
        pointerStack.push(block->pointers[i]);
        block = (IndexBlock*)disk->ReadBlock(block->pointers[i].blockNumber);
    }

    bool keyFound = false;
    int keyIndex;
    for(keyIndex = 0; keyIndex < block->numKeys; keyIndex++) {
        if (block->keys[keyIndex] == key) {
            keyFound = true;
            break;
        }
    }
     
    if (!keyFound) {
        cout << "Key " << key << " not found in the tree." << endl;
        return;
    }

    keySet.erase(key);

    DuplicatesBlock* db = (DuplicatesBlock*) disk->ReadBlock(block->pointers[keyIndex].blockNumber);
    if(db->nodeType == DUPLICATES) {
        do {
            for(int i = 0; i < db->numKeys; i++) {
                DataBlock* b = (DataBlock*) disk->ReadBlock(db->pointers[i].blockNumber);
                b->records[db->pointers[i].recordIndex].occupied = false;
                disk->WriteBlock(db->pointers[i].blockNumber, (uint8_t*)b);
                if(isEmpty(b)) numDataBlocks--;
                free(b);
                numRecords--;
            }

            DuplicatesBlock* next = nullptr;
            if(db->next.blockNumber != 0) {
                next = (DuplicatesBlock*)disk->ReadBlock(db->next.blockNumber);
            }
            
            free(db);
            numDuplicates--;
            db = next;
        } while(db != nullptr);
    } else {
        DataBlock* b = (DataBlock*) db;
        b->records[block->pointers[keyIndex].recordIndex].occupied = false;
        disk->WriteBlock(block->pointers[keyIndex].blockNumber, (uint8_t*)b);
        numRecords--;
        if(isEmpty(b)) numDataBlocks--;
        free(b);
    }

    for(int i = keyIndex; i < block->numKeys - 1; i++) {
        block->keys[i] = block->keys[i + 1];
        block->pointers[i] = block->pointers[i + 1];
    }
    block->numKeys--;

    RecordPointer childPointer = pointerStack.top();
    pointerStack.pop();

    disk->WriteBlock(childPointer.blockNumber, (uint8_t*)block);

    if (block->numKeys < ceil(N / 2.0f)) {
        IndexBlock* parent = nullptr;
        IndexBlock* leftSibling = nullptr;
        IndexBlock* rightSibling = nullptr;
        int indexOfBlockInParent = -1;

        if (!blockStack.empty()) {
            parent = blockStack.top();
            RecordPointer parentPointer = pointerStack.top();
            
            for (int i = 0; i <= parent->numKeys; i++) {
                if (parent->pointers[i].blockNumber == childPointer.blockNumber) {
                    indexOfBlockInParent = i;
                    break;
                }
            }
            
            if (indexOfBlockInParent > 0) leftSibling = (IndexBlock*)disk->ReadBlock(parent->pointers[indexOfBlockInParent - 1].blockNumber);
            if (indexOfBlockInParent < parent->numKeys) rightSibling = (IndexBlock*)disk->ReadBlock(parent->pointers[indexOfBlockInParent + 1].blockNumber);

            if (leftSibling != nullptr && leftSibling->numKeys > ceil(N / 2.0f)) {
                for(int i = block->numKeys; i > 0; i--) {
                    block->keys[i] = block->keys[i - 1];
                    block->pointers[i] = block->pointers[i-1];
                }
                block->pointers[1] = block->pointers[0];
                block->keys[0] = leftSibling->keys[leftSibling->numKeys - 1];

                parent->keys[indexOfBlockInParent - 1] = leftSibling->keys[leftSibling->numKeys - 1];
                block->pointers[0] = leftSibling->pointers[leftSibling->numKeys-1];
                leftSibling->numKeys--;
                block->numKeys++;

                disk->WriteBlock(parentPointer.blockNumber, (uint8_t*) parent);
                disk->WriteBlock(childPointer.blockNumber, (uint8_t*) block);
                disk->WriteBlock(parent->pointers[indexOfBlockInParent - 1].blockNumber, (uint8_t*) leftSibling);
            
            } 
            else if (rightSibling != nullptr && rightSibling->numKeys > ceil(N / 2.0f)) {
                block->keys[block->numKeys] = parent->keys[indexOfBlockInParent];
                block->pointers[block->numKeys] = rightSibling->pointers[0];
                
                for (int i = 0; i < rightSibling->numKeys - 1; i++) {
                    rightSibling->keys[i] = rightSibling->keys[i + 1];
                    rightSibling->pointers[i] = rightSibling->pointers[i + 1];
                }

                rightSibling->pointers[rightSibling->numKeys - 1] = rightSibling->pointers[rightSibling->numKeys];
                parent->keys[indexOfBlockInParent] = rightSibling->keys[0];
                block->numKeys++;
                rightSibling->numKeys--;

                disk->WriteBlock(parentPointer.blockNumber, (uint8_t*) parent);
                disk->WriteBlock(childPointer.blockNumber, (uint8_t*) block);
                disk->WriteBlock(parent->pointers[indexOfBlockInParent + 1].blockNumber, (uint8_t*) rightSibling);
            }

            else {
                int siblingIndex = (leftSibling != nullptr) ? indexOfBlockInParent - 1 : indexOfBlockInParent;
                
                IndexBlock* mergeInto = (leftSibling != nullptr) ? leftSibling : block;
                IndexBlock* mergeFrom = (leftSibling != nullptr) ? block : rightSibling;
                RecordPointer mergePointer = (leftSibling != nullptr) ? parent->pointers[indexOfBlockInParent - 1] : childPointer;

                for (int i = 0; i < mergeFrom->numKeys; i++) {
                    mergeInto->keys[mergeInto->numKeys] = mergeFrom->keys[i];
                    mergeInto->pointers[mergeInto->numKeys] = mergeFrom->pointers[i];
                    mergeInto->numKeys++;
                }   

                mergeInto->pointers[N]=mergeFrom->pointers[N];

                for (int i = siblingIndex + 1; i < parent->numKeys; i++) {
                    parent->keys[i - 1] = parent->keys[i];
                    parent->pointers[i] = parent->pointers[i + 1];
                }
                parent->numKeys--;
                numLeaf--;

                disk->WriteBlock(mergePointer.blockNumber, (uint8_t*) mergeInto);
                disk->WriteBlock(parentPointer.blockNumber, (uint8_t*) parent);
            }

            if(leftSibling != nullptr) free(leftSibling);
            if(rightSibling != nullptr) free(rightSibling);

            while (!blockStack.empty()) {
                IndexBlock* parentNode = blockStack.top();
                blockStack.pop();
                RecordPointer parentNodePointer = pointerStack.top();
                pointerStack.pop();

                if (parentNode->numKeys < floor(N / 2.0f)) {
                    IndexBlock* grandParent = !blockStack.empty() ? blockStack.top() : nullptr;
                    RecordPointer grandParentPointer = !pointerStack.empty() ? pointerStack.top() : (RecordPointer){0,0};
                    IndexBlock* leftParentSibling = nullptr;
                    IndexBlock* rightParentSibling = nullptr;
                    int indexOfParentInGrandparent = -1;


                    if (grandParent != nullptr) {
                        for (int i = 0; i <= grandParent->numKeys; ++i) {
                            if (grandParent->pointers[i].blockNumber == parentNodePointer.blockNumber) {
                                indexOfParentInGrandparent = i;
                                if (i > 0) leftParentSibling = (IndexBlock*)disk->ReadBlock(grandParent->pointers[i - 1].blockNumber);
                                if (i < grandParent->numKeys) rightParentSibling = (IndexBlock*)disk->ReadBlock(grandParent->pointers[i + 1].blockNumber);
                                break;
                            }
                        }
                    }

                    if (leftParentSibling != nullptr && leftParentSibling->numKeys > floor(N / 2.0f)) {
                        parentNode->pointers[parentNode->numKeys + 1] = parentNode->pointers[parentNode->numKeys]; // Shift the pointer
                        for (int i = parentNode->numKeys; i > 0; i--) {
                            parentNode->keys[i] = parentNode->keys[i - 1];
                            parentNode->pointers[i] = parentNode->pointers[i - 1];
                        }
                        parentNode->keys[0] = grandParent->keys[indexOfParentInGrandparent - 1];
                        grandParent->keys[indexOfParentInGrandparent - 1] = leftParentSibling->keys[leftParentSibling->numKeys - 1];
                        parentNode->pointers[0] = leftParentSibling->pointers[leftParentSibling->numKeys];
                        parentNode->numKeys++;
                        leftParentSibling->numKeys--;

                        disk->WriteBlock(parentNodePointer.blockNumber, (uint8_t*) parentNode);
                        disk->WriteBlock(grandParentPointer.blockNumber, (uint8_t*) grandParent);
                        disk->WriteBlock(grandParent->pointers[indexOfParentInGrandparent - 1].blockNumber, (uint8_t*) leftParentSibling);
                    } 
                    else if (rightParentSibling != nullptr && rightParentSibling->numKeys > floor(N / 2.0f)) {
                        parentNode->keys[parentNode->numKeys] = grandParent->keys[indexOfParentInGrandparent];
                        parentNode->pointers[parentNode->numKeys + 1] = rightParentSibling->pointers[0];
                        grandParent->keys[indexOfParentInGrandparent] = rightParentSibling->keys[0];
                        for (int i = 0; i < rightParentSibling->numKeys - 1; i++) {
                            rightParentSibling->keys[i] = rightParentSibling->keys[i + 1];
                            rightParentSibling->pointers[i] = rightParentSibling->pointers[i + 1];
                        }
                        rightParentSibling->pointers[rightParentSibling->numKeys - 1] = rightParentSibling->pointers[rightParentSibling->numKeys];
                        parentNode->numKeys++;
                        rightParentSibling->numKeys--;

                        disk->WriteBlock(parentNodePointer.blockNumber, (uint8_t*) parentNode);
                        disk->WriteBlock(grandParentPointer.blockNumber, (uint8_t*) grandParent);
                        disk->WriteBlock(grandParent->pointers[indexOfParentInGrandparent + 1].blockNumber, (uint8_t*) rightParentSibling);
                    }

                    else {
                        if (leftParentSibling != nullptr && leftParentSibling->numKeys <= floor(N / 2.0f)) {
                            RecordPointer leftPointer = grandParent->pointers[indexOfParentInGrandparent - 1];
                            IndexBlock* successor = (IndexBlock*)disk->ReadBlock(parentNode->pointers[0].blockNumber);
                            while(successor->nodeType != LEAF)  {
                                RecordPointer nextBlock = successor->pointers[0];
                                free(successor);
                                successor = (IndexBlock*)disk->ReadBlock(nextBlock.blockNumber);
                            }
                            leftParentSibling->keys[leftParentSibling->numKeys] = successor->keys[0];
                            free(successor);

                            for (int i = 0; i < parentNode->numKeys; ++i) {
                                leftParentSibling->keys[leftParentSibling->numKeys + 1 + i] = parentNode->keys[i];
                                leftParentSibling->pointers[leftParentSibling->numKeys + 1 + i] = parentNode->pointers[i];
                            }

                            leftParentSibling->pointers[leftParentSibling->numKeys + 1 + parentNode->numKeys] = parentNode->pointers[parentNode->numKeys];
                            leftParentSibling->numKeys += (1 + parentNode->numKeys);

                            for (int i = indexOfParentInGrandparent; i < grandParent->numKeys; ++i) {
                                grandParent->keys[i - 1] = grandParent->keys[i];
                                grandParent->pointers[i] = grandParent->pointers[i + 1];
                            }
                            
                            grandParent->numKeys--;
                            numInternal--;

                            disk->WriteBlock(grandParentPointer.blockNumber, (uint8_t*) grandParent);
                            disk->WriteBlock(leftPointer.blockNumber, (uint8_t*) leftParentSibling);
                        } 

                        else if (rightParentSibling != nullptr && rightParentSibling->numKeys <= floor(N / 2.0f)) {
                            parentNode->keys[parentNode->numKeys] = grandParent->keys[indexOfParentInGrandparent];

                            for (int i = 0; i < rightParentSibling->numKeys; ++i) {
                                parentNode->keys[parentNode->numKeys + 1 + i] = rightParentSibling->keys[i];
                                parentNode->pointers[parentNode->numKeys + 1 + i] = rightParentSibling->pointers[i];
                            }
                            parentNode->pointers[parentNode->numKeys + rightParentSibling->numKeys + 1] = rightParentSibling->pointers[rightParentSibling->numKeys];
                            parentNode->numKeys += rightParentSibling->numKeys + 1;

                            for (int i = indexOfParentInGrandparent + 1; i < grandParent->numKeys; ++i) {
                                grandParent->keys[i - 1] = grandParent->keys[i];
                                grandParent->pointers[i] = grandParent->pointers[i + 1];
                            }
                            grandParent->numKeys--;
                            numInternal--;

                            disk->WriteBlock(grandParentPointer.blockNumber, (uint8_t*) grandParent);
                            disk->WriteBlock(parentNodePointer.blockNumber, (uint8_t*) parentNode);
                        }

                    }

                    if(leftParentSibling != nullptr) free(leftParentSibling);
                    if(rightParentSibling != nullptr) free(rightParentSibling);
                }

                int parentUpdate = keyIndexInNode(parentNode, key);
                // Check if the current node is an internal node and contains the key
                if (parentNode->nodeType != LEAF && parentUpdate != -1) {
                    
                    // Find the in-order successor of the key
                    uint32_t inOrderSuccessor = findInOrderSuccessor(disk, parentNode, key);

                    // Replace the key in the internal node with the in-order successor
                    parentNode->keys[parentUpdate] = inOrderSuccessor;
                    disk->WriteBlock(parentNodePointer.blockNumber, (uint8_t*)parentNode);
                }

                block = parentNode;
            }
        }
    }

    free(block);

    IndexBlock* root = (IndexBlock*)disk->ReadBlock(rootPointer->blockNumber);
    if (root->numKeys == 0 && root->nodeType != LEAF) {
        rootPointer->blockNumber = root->pointers[0].blockNumber;
        numInternal--;
        numLevels--;
    }

    int rootUpdate = keyIndexInNode(root, key);
    if (root->nodeType != LEAF && rootUpdate != -1) {
        // Navigate to the right subtree of the deleted key
        IndexBlock* current = (IndexBlock*)disk->ReadBlock(root->pointers[rootUpdate + 1].blockNumber);

        // Find the in-order successor (leftmost key in the right subtree)
        while (current->nodeType != LEAF) {
            RecordPointer nextBlock = current->pointers[0];
            free(current);
            current = (IndexBlock*)disk->ReadBlock(nextBlock.blockNumber);
        }

        // Replace the root key with the in-order successor key
        root->keys[rootUpdate] = current->keys[0];
        disk->WriteBlock(rootPointer->blockNumber, (uint8_t*)root);
        free(current);
    }
    free(root);
    
    while (!blockStack.empty()) {
        IndexBlock* currentNode = blockStack.top();
        blockStack.pop();
        RecordPointer nodePointer = pointerStack.top();
        pointerStack.pop();

        // Check if the current node is an internal node and contains the key
        int keyUpdate = keyIndexInNode(currentNode, key);
        if (currentNode->nodeType != LEAF && keyUpdate != -1) {
            // Find the in-order successor of the key
            uint32_t inOrderSuccessor = findInOrderSuccessor(disk, currentNode, key);

            // Replace the key in the internal node with the in-order successor
            currentNode->keys[keyUpdate] = inOrderSuccessor;
            disk->WriteBlock(nodePointer.blockNumber,(uint8_t*) currentNode);
        }
        free(currentNode);
    }
}

uint32_t LinearDelete(Disk* disk, uint32_t key) {
    uint32_t nBlocks = 0;
    for(int i = FIRST_DATA_BLOCK; i < LAST_DATA_BLOCK + FIRST_DATA_BLOCK; i++){
        nBlocks++;
        DataBlock* block = (DataBlock*)disk->ReadBlock(i);
        bool modified = false;
        for(int k = 0; k < RECORDS_PER_BLOCK; k++){
            if(block->records[k].occupied && block->records[k].numVotes == key){
                block->records[k].occupied = false;
                modified = true;
            }
        }
        if(modified) disk->WriteBlock(i,(uint8_t*)block);
        free(block);
    }
    return nBlocks;
}

void Experiment5(Disk* disk) {
    cout << endl;
    cout << "Running Experiment 5" << endl;
    cout << "Deleting 1000" << endl;

    long long indexedAvg, linearAvg;
    uint32_t linearAccess = 0;
    chrono::steady_clock::time_point start, end;
    vector<long long> timings;

    uint8_t* diskCopy = (uint8_t*)calloc(DISK_SIZE, sizeof(uint8_t));
    Disk* disk2 = new Disk(diskCopy);

    // Save Stats
    uint32_t nInternal = numInternal;
    uint32_t nDuplicates = numDuplicates;
    RecordPointer rPointer = *rootPointer;
    uint32_t nDataBlocks = numDataBlocks;
    uint32_t nLeaf = numLeaf;
    uint32_t nRecords = numRecords;

    for(int i = 0; i < TRIALS; i++){
        disk->Copy(diskCopy);
        start = chrono::steady_clock::now();
        Delete(disk2, 1000);
        end = chrono::steady_clock::now();
        timings.push_back(chrono::duration_cast<chrono::microseconds>(end - start).count());

        // Restore Stats
        numInternal = nInternal;
        numDataBlocks = nDataBlocks;
        numDuplicates = nDuplicates;
        numLeaf = nLeaf;
        numRecords = nRecords;
        *rootPointer = rPointer;
    }
    indexedAvg = Median(timings);
    timings.clear();

    for(int i = 0; i < TRIALS; i++){
        disk->Copy(diskCopy);
        start = chrono::steady_clock::now();
        linearAccess = LinearDelete(disk2, 1000);
        end = chrono::steady_clock::now();
        timings.push_back(chrono::duration_cast<chrono::microseconds>(end - start).count());
    }
    free(diskCopy);
    linearAvg = Median(timings);

    start = chrono::steady_clock::now();
    Delete(disk, 1000);
    end = chrono::steady_clock::now();
    indexedAvg = chrono::duration_cast<chrono::microseconds>(end - start).count();
    cout << "> B+ Tree Deletion Statistics" << endl;
    cout << "a) The tree has " << numLevels << " levels" << endl;
    cout << "b) " << numInternal << " Internal Nodes, " << numLeaf << " Leaf Nodes, " << numDuplicates << " Duplicate Nodes. Total is " << numInternal + numLeaf + numDuplicates << endl;

    IndexBlock* rootBlock = (IndexBlock*)disk->ReadBlock(rootPointer->blockNumber);
    cout << "c) Root has " << (uint32_t)rootBlock->numKeys << " keys:";
    for(int i = 0; i < rootBlock->numKeys; i++) {
        cout << " " << (uint32_t)rootBlock->keys[i];
    }
    free(rootBlock);
    cout << endl;
    cout << "d) Time Taken: " << indexedAvg/1000.0f << "ms (median of " << TRIALS << " trials)" << endl;

    cout << "> Linear Deletion Statistics" << endl;
    cout << "a) Data Blocks Accessed: " << linearAccess << endl;
    cout << "b) Time Taken: " << linearAvg/1000.0f << "ms (median of " << TRIALS << " trials)" << endl;
    VerifyTree(disk);
}

int main() {
    Disk* disk = new Disk();
    Experiment1(disk, "data.tsv");
    Experiment2(disk);
    VerifyTree(disk);
    Experiment3(disk);
    Experiment4(disk);
    Experiment5(disk);
}

