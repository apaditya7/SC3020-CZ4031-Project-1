#include <iomanip>
#include <sstream>
#include <iostream>
#include <fstream>
#include <chrono>
#include <algorithm>
#include <vector>
#include <string.h>
#include "constants.h"
#include "disk.h"
#include "bptree.h"
#include "wchar.h"

using namespace std;

Disk* disk = new Disk();
BPTree* bptree;
uint32_t LAST_DATA_BLOCK;

void Experiment1(string filename) {
    cout << "Running Experiment 1" << endl;
    uint32_t numRecords = 0;
    uint32_t numDataBlocks = 0;
    uint32_t currentFreeBlock = FIRST_DATA_BLOCK;

    ifstream txt(filename);
    assert(txt.good());

    string line;
    getline(txt, line); // consume the header line

    DataBlock* block = (DataBlock*)disk->ReadBlock(currentFreeBlock);
    uint32_t index = 0;

    while(getline(txt, line)) {
        if(line.empty()) continue; // ignore empty lines
        numRecords++;

        stringstream ss(line);
        NBARecord record;
        getline(ss, record.game_date_est, '\t');
        ss >> record.team_id_home;
        ss.ignore(1);  // skip the tab character
        ss >> record.pts_home;
        ss.ignore(1);
        ss >> record.fg_pct_home;
        ss.ignore(1);
        ss >> record.ft_pct_home;
        ss.ignore(1);
        ss >> record.fg3_pct_home;
        ss.ignore(1);
        ss >> record.ast_home;
        ss.ignore(1);
        ss >> record.reb_home;
        ss.ignore(1);
        ss >> record.home_team_wins;

        block->records[index] = record;
        block->occupied[index] = true;

        ++index;
        if(index == RECORDS_PER_BLOCK) {
            disk->WriteBlock(currentFreeBlock, (uint8_t*)block);
            free(block);
            currentFreeBlock += 1;
            block = (DataBlock*)disk->ReadBlock(currentFreeBlock);
            index = 0;
        }
    }

    // Write remaining records in the block
    if(index != 0) {
        disk->WriteBlock(currentFreeBlock, (uint8_t*)block);
        currentFreeBlock += 1;
    }
    free(block);

    numDataBlocks = currentFreeBlock - 1;
    LAST_DATA_BLOCK = numDataBlocks;

    cout << "Statistics:" << endl;
    cout << "a) " << numRecords << " Records" << endl;
    cout << "b) Each record has a size of " << sizeof(NBARecord) << "B" << endl;
    cout << "c) " << RECORDS_PER_BLOCK << " Records per Block" << endl;
    cout << "d) " << numDataBlocks << " Blocks used" << endl;

    // Initialize B+ tree
    bptree = new BPTree(numRecords, numDataBlocks, currentFreeBlock);
    cout << "B+ Tree initialized." << endl;
}


void Experiment2() {
    cout << endl << "Running Experiment 2" << endl;
    
    // Initialize the root of the B+ tree
    bptree->rootPointer->blockNumber = bptree->currentFreeBlock++;
    IndexBlock* root = (IndexBlock*)disk->ReadBlock(bptree->rootPointer->blockNumber);
    root->nodeType = TYPE_LEAF;
    root->numKeys = 0;
    disk->WriteBlock(bptree->rootPointer->blockNumber, (uint8_t*) root);
    free(root);

    // Insert records from data blocks into the B+ tree using FG_PCT_home as the key
    for (uint32_t i = FIRST_DATA_BLOCK; i < LAST_DATA_BLOCK + FIRST_DATA_BLOCK; i++) {
        DataBlock* dataBlock = (DataBlock*)disk->ReadBlock(i);
        for (uint32_t k = 0; k < RECORDS_PER_BLOCK; k++) {
            if (dataBlock->occupied[k]) {
                // Insert using FG_PCT_home as the key instead of numVotes
                bptree->Insert(disk, dataBlock->records[k].fg_pct_home, (RecordPointer){i, k});
            }
        }
        free(dataBlock);
    }

    // Display statistics related to the B+ tree
    cout << "Statistics:" << endl;
    cout << "a) N = " << N << endl;  // Order of the B+ tree
    cout << "b) The tree has " << bptree->numLevels << " levels" << endl;  // Number of levels in the tree
    cout << "c) " << bptree->numInternal << " Internal Nodes, " << bptree->numLeaf << " Leaf Nodes, " << bptree->numOverflow << " Overflow Nodes." 
         << " Total is " << bptree->numInternal + bptree->numLeaf + bptree->numOverflow << endl;

    // Display the keys in the root node
    IndexBlock* rootBlock = (IndexBlock*)disk->ReadBlock(bptree->rootPointer->blockNumber);
    cout << "d) Root has " << (uint32_t)rootBlock->numKeys << " keys:";
    for (int i = 0; i < rootBlock->numKeys; i++) {
        cout << " " << (uint32_t)rootBlock->keys[i];
    }
    free(rootBlock);
    cout << endl;

    // Verify the integrity of the B+ tree
    bptree->VerifyTree(disk);
}

SearchResult LinearSearch(Disk* searchDisk, float min, float max) {
    uint32_t nBlocks = 0;
    uint32_t totalRecords = 0;
    float totalRating = 0;

    for (int i = FIRST_DATA_BLOCK; i < LAST_DATA_BLOCK + FIRST_DATA_BLOCK; i++) {
        DataBlock* block = (DataBlock*)searchDisk->ReadBlock(i);
        nBlocks++;
        for (int k = 0; k < RECORDS_PER_BLOCK; k++) {
            if (block->occupied[k]) {
                // Perform search on FG_PCT_home in the given range
                if (block->records[k].fg_pct_home >= min && block->records[k].fg_pct_home <= max) {
                    totalRating += block->records[k].fg3_pct_home;
                    totalRecords++;
                }
            }
        }
        free(block);
    }

    return (SearchResult){nBlocks, totalRecords, totalRating / (float)totalRecords};
}

void PrintSearchResult(SearchResult sr, long long timeTaken) {
    cout << "> Linear Search Statistics" << endl;
    cout << "a) Data Blocks Accessed: " << sr.nData << endl;
    cout << "b) Found " << sr.recordsFound << " records" << endl;
    cout << "c) Average FG3_PCT_home: " << sr.averageRating << endl;
    cout << "d) Time Taken: " << timeTaken / 1000.0f << "ms (median of " << TRIALS << " trials)" << endl;
}

void PrintSearchResult(IndexedSearchResult isr, long long timeTaken) {
    cout << "> Indexed Search Statistics" << endl;
    cout << "a) Index Blocks Accessed: " << isr.nInternal << " Internal, " << isr.nLeaf << " Leaf, " << isr.nOverflow << " Overflow. Total is " << isr.nInternal + isr.nLeaf + isr.nOverflow << endl;
    cout << "b) Data Blocks Accessed: " << isr.nData << endl;
    cout << "c) Found " << isr.recordsFound << " records" << endl;
    cout << "d) Average FG3_PCT_home: " << isr.averageRating << endl;
    cout << "e) Time Taken: " << timeTaken / 1000.0f << "ms (median of " << TRIALS << " trials)" << endl;
}

long long Median(vector<long long> timings) {
    vector<long long>::iterator it = timings.begin() + timings.size() / 2;
    nth_element(timings.begin(), it, timings.end());
    return timings[timings.size() / 2];
}


void Experiment3() {
    cout << endl << "Running Experiment 3" << endl;

    float minFG = 0.5;
    float maxFG = 0.8;

    // Perform indexed search using the B+ tree
    vector<long long> indexedSearchTimes;
    IndexedSearchResult isr;
    for (int trial = 0; trial < TRIALS; trial++) {
        auto start = chrono::high_resolution_clock::now();
        isr = bptree->Search(disk, minFG, maxFG);
        auto end = chrono::high_resolution_clock::now();
        indexedSearchTimes.push_back(chrono::duration_cast<chrono::microseconds>(end - start).count());
    }

    long long indexedMedianTime = Median(indexedSearchTimes);
    PrintSearchResult(isr, indexedMedianTime);

    // Perform linear scan as a comparison
    vector<long long> linearSearchTimes;
    SearchResult sr;
    for (int trial = 0; trial < TRIALS; trial++) {
        auto start = chrono::high_resolution_clock::now();
        sr = LinearSearch(disk, minFG, maxFG);
        auto end = chrono::high_resolution_clock::now();
        linearSearchTimes.push_back(chrono::duration_cast<chrono::microseconds>(end - start).count());
    }

    long long linearMedianTime = Median(linearSearchTimes);
    PrintSearchResult(sr, linearMedianTime);

    // Compare the number of data blocks accessed by both methods
    cout << "Comparison:" << endl;
    cout << "Indexed search accessed " << isr.nData << " data blocks." << endl;
    cout << "Linear search accessed " << sr.nData << " data blocks." << endl;
}

IndexedSearchResult BPTree::Search(Disk* disk, float minFG, float maxFG) {
    // Initialize search result structure
    IndexedSearchResult result = {0, 0, 0, 0, 0, 0};

    // Start from the root node
    RecordPointer currentPointer = *rootPointer;
    IndexBlock* currentBlock = (IndexBlock*)disk->ReadBlock(currentPointer.blockNumber);  // Changed from currentPointer->blockNumber

    // Traverse down to the leaf nodes
    while (currentBlock->nodeType != TYPE_LEAF) {
        result.nInternal++;  // Count internal node access
        int i = 0;
        while (i < currentBlock->numKeys && currentBlock->keys[i] < minFG) {
            i++;
        }
        currentPointer = currentBlock->pointers[i];  // Access the next block
        free(currentBlock);
        currentBlock = (IndexBlock*)disk->ReadBlock(currentPointer.blockNumber);  // Changed from currentPointer->blockNumber
    }

    // Continue with leaf node processing as needed...



    // Traverse through the leaf nodes to find records in the range
    while (currentPointer.blockNumber != 0) {
        result.nLeaf++;  // Count leaf node access
        IndexBlock* leafBlock = (IndexBlock*)disk->ReadBlock(currentPointer.blockNumber);
        for (int i = 0; i < leafBlock->numKeys; i++) {
            if (leafBlock->keys[i] >= minFG && leafBlock->keys[i] <= maxFG) {
                result.nData++;  // Count data block access
                DataBlock* dataBlock = (DataBlock*)disk->ReadBlock(leafBlock->pointers[i].blockNumber);
                for (int j = 0; j < RECORDS_PER_BLOCK; j++) {
                    if (dataBlock->occupied[j] && dataBlock->records[j].fg_pct_home >= minFG && dataBlock->records[j].fg_pct_home <= maxFG) {
                        result.averageRating += dataBlock->records[j].fg3_pct_home;
                        result.recordsFound++;
                    }
                }
                free(dataBlock);
            }
        }
        currentPointer = leafBlock->pointers[leafBlock->numKeys];
        free(leafBlock);
    }

    if (result.recordsFound > 0) {
        result.averageRating /= result.recordsFound;  // Compute the average FG3_PCT_home
    }

    return result;
}


int main() {
    Experiment1("/Users/adityaap/task1_DBSP/games.txt");
    Experiment2();
    Experiment3();

    delete disk;
    delete bptree;
}

