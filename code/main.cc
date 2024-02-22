#include <iomanip>
#include <stdio.h>
#include <sstream>
#include <iostream>
#include <fstream>
#include <stack>
#include <queue>
#include <set>

#include "constants.h"
#include "disk.h"

using namespace std;

uint32_t numRecords;
uint32_t numDataBlocks;
uint32_t currentFreeBlock = 1;

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
        free(block);
        currentFreeBlock += 1;
    }

    numDataBlocks = currentFreeBlock - 1;

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
RecordPointer* rootPointer;

void Insert(Disk* disk, int key, RecordPointer pointer) {
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

    for(uint32_t i = 1; i <= numDataBlocks; i++){
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
    set<uint32_t> keySet;

    queue<vData> q;
    q.push((vData){*rootPointer, 5, 2279223});
    bool skip = true; // skip min key amount check for the first time, i.e, for the root

    while(q.size() > 0) {
        vData data = q.front();
        q.pop();

        IndexBlock* block = (IndexBlock*)disk->ReadBlock(data.p.blockNumber);
        if(block->nodeType == INTERNAL){
            nInternal++;
            ASSERT(block->keys[0] >= data.n1, "[INTERNAL] Minimum is %d, found %d", data.n1, block->keys[0]);
            ASSERT(block->keys[block->numKeys - 1] <= data.n2, "[INTERNAL] Maximum is %d, found %d", data.n2, block->keys[block->numKeys - 1]);
            ASSERT(skip || block->numKeys >= floor(N/2.0f), "[INTERNAL] Expected atleast floor(n/2) keys. Found %d", block->numKeys);
            skip = false;
            uint32_t min = data.n1;
            for(int i = 0; i < block->numKeys; i++) {
                ASSERT(keySet.count(block->keys[i]) == 0, "[INTERNAL] Found duplicate key %d within internal nodes", block->keys[i]);
                keySet.insert(block->keys[i]);
                if(i >= 1) ASSERT(block->keys[i] > block->keys[i-1], "[INTERNAL] Node not sorted (%d is after %d)", block->keys[i], block->keys[i-1]);
                q.push((vData){block->pointers[i],min,block->keys[i]-1});
                min = block->keys[i];
            }
            q.push((vData){block->pointers[block->numKeys],min,data.n2});


        } else if(block->nodeType == LEAF) {
            ASSERT(nInternal == numInternal, "Expected %d internal nodes, traveresed %d", numInternal, nInternal);
            nLeaf++;
            ASSERT(block->keys[0] >= data.n1, "[LEAF] Minimum is %d, found %d", data.n1, block->keys[0]);
            ASSERT(block->keys[block->numKeys - 1] <= data.n2, "[LEAF] Maximum is %d, found %d", data.n2, block->keys[block->numKeys - 1]);
            ASSERT(block->numKeys >= floor((N+1)/2.0f), "[LEAF] Expected atleast floor((n+1)/2). Found %d", block->numKeys);
            ASSERT(block->pointers[N].blockNumber == q.front().p.blockNumber || ((IndexBlock*)disk->ReadBlock(q.front().p.blockNumber))->nodeType == DUPLICATES, "[LEAF] next (%d) != q.next (%d)", block->pointers[N].blockNumber, q.front().p.blockNumber);
            for(int i = 0; i < block->numKeys; i++) {
                if(keySet.count(block->keys[i])) keySet.erase(block->keys[i]);
                if(i >= 1) ASSERT(block->keys[i] > block->keys[i-1], "[LEAF] Node not sorted (%d is after %d)", block->keys[i], block->keys[i-1]);
                q.push((vData){block->pointers[i],block->keys[i],0});
            }

        } else if(block->nodeType == DUPLICATES) {
            ASSERT(nLeaf == numLeaf, "Expected %d leaf nodes, traveresed %d", numLeaf, nLeaf);
            ASSERT(keySet.size() == 0, "Found %lu internal keys that do not appear in leaves", keySet.size());
            DuplicatesBlock* dB = (DuplicatesBlock*)block;
            nDuplicates++;
            ASSERT(dB->numKeys > 0 && (dB->numKeys > 1 || dB->next.blockNumber != 0), "Duplicate Block has %d keys!", dB->numKeys);
            for(int i = 0; i < dB->numKeys; i++) {
                q.push((vData){dB->pointers[i],data.n1,data.n2});
            }
            while(dB->next.blockNumber != 0) {
                nDuplicates++;
                dB = (DuplicatesBlock*)disk->ReadBlock(dB->next.blockNumber);
                ASSERT(dB->numKeys == N*2, "Inner Duplicate Block has %d keys", dB->numKeys);
                for(int i = 0; i < dB->numKeys; i++) {
                    q.push((vData){dB->pointers[i],data.n1,data.n2});
                }
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
    }

    ASSERT(nDuplicates == numDuplicates, "Expected %d duplicate nodes, traveresed %d", numDuplicates, nDuplicates);
    ASSERT(dataBlocks.size() == numDataBlocks, "Expected %u data blocks, traveresed %lu", numDataBlocks, dataBlocks.size());
    ASSERT(pointers.size() == numRecords, "Expected %u records, traveresed %lu", numRecords, pointers.size());

    cout << "Passed!" << endl;
}

int main() {
    Disk* disk = new Disk();
    Experiment1(disk, "data.tsv");
    Experiment2(disk);
    VerifyTree(disk);
}

// EXTRAS

// Used for verification of loading. running diff on the output and data.tsv
void PrintAllRecords(Disk* disk) {
    cout << "tconst\taverageRating\tnumVotes" << endl;
    for(int i = 1; i < currentFreeBlock; ++i){
        DataBlock* block = (DataBlock*)disk->ReadBlock(i);
        for(int k = 0; k < RECORDS_PER_BLOCK; ++k){
            if(!block->records[k].occupied) continue;
            printf("%.*s", 10, block->records[k].tconst);
            cout << "\t" << setprecision(1) << fixed << block->records[k].averageRating << "\t" << block->records[k].numVotes << endl;
        }
        free(block);   
    }
}

// Prints block as individual bytes
void PrintBlock(uint8_t* block) {
    printf("\nReading Block\n");
    for(int i = 0; i < BLOCK_SIZE; i++) {
        printf("%02x ", block[i]);
    }
    printf("\n");
}

// Prints the tree level by level
void PrintTree(Disk* disk) {
    queue<RecordPointer> q;
    q.push(*rootPointer);

    while(q.size() > 0) {
        IndexBlock* block = (IndexBlock*)disk->ReadBlock(q.front().blockNumber);
        q.pop();

        if(block->nodeType == INTERNAL) {
            for(int i = 0; i <= block->numKeys; i++){
                cout << block->pointers[i].blockNumber << endl;
                q.push(block->pointers[i]);
            }
        } 

        for (int i = 0; i < block->numKeys; i++){
            cout << " " << block->keys[i];
            if(block->nodeType == LEAF) {
                DuplicatesBlock* db = (DuplicatesBlock*)disk->ReadBlock(block->pointers[i].blockNumber);
                if(db->nodeType == DUPLICATES) {
                    for(int j = 1; j < db->numKeys; j++){
                        cout << "-" << block->keys[i];
                    }
                    while(db->next.blockNumber != 0){
                        free(db);
                        db = (DuplicatesBlock*)disk->ReadBlock(db->next.blockNumber);
                    }
                }
                free(db);
            }
        }
        free(block);
        cout << endl;
    }
}
