#include <iomanip>
#include <sstream>
#include <iostream>
#include <fstream>
#include <chrono>
#include <algorithm>

#include "constants.h"
#include "disk.h"
#include "bptree.h"

using namespace std;

Disk* disk = new Disk();
BPTree* bptree;
uint32_t LAST_DATA_BLOCK;

void Experiment1(string filename) {
    cout << "Running Experiment 1" << endl;
    uint32_t numRecords = 0;
    uint32_t numDataBlocks = 0;
    uint32_t currentFreeBlock = FIRST_DATA_BLOCK;

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

    bptree = new BPTree(numRecords, numDataBlocks, currentFreeBlock);
}

void Experiment2() {
    cout << endl << "Running Experiment 2" << endl;
    bptree->rootPointer->blockNumber = bptree->currentFreeBlock++;

    IndexBlock* root = (IndexBlock*)disk->ReadBlock(bptree->rootPointer->blockNumber);
    root->nodeType = TYPE_LEAF;
    root->numKeys = 0;
    disk->WriteBlock(bptree->rootPointer->blockNumber,(uint8_t*) root);
    free(root);

    for(uint32_t i = FIRST_DATA_BLOCK; i < LAST_DATA_BLOCK + FIRST_DATA_BLOCK; i++){
        DataBlock* dataBlock = (DataBlock*)disk->ReadBlock(i);
        for(uint32_t k = 0; k < RECORDS_PER_BLOCK; k++){
            if(dataBlock->records[k].occupied) {
                bptree->Insert(disk, dataBlock->records[k].numVotes, (RecordPointer){i, k});
            }
        }
        free(dataBlock);
    }

    cout << "Statistics:" << endl;
    cout << "a) N = " << N << endl;
    cout << "b) The tree has " << bptree->numLevels << " levels" << endl;
    cout << "c) " << bptree->numInternal << " Internal Nodes, " << bptree->numLeaf << " Leaf Nodes, " << bptree->numOverflow << " Overflow Nodes. Total is " << bptree->numInternal + bptree->numLeaf + bptree->numOverflow << endl;

    IndexBlock* rootBlock = (IndexBlock*)disk->ReadBlock(bptree->rootPointer->blockNumber);
    cout << "d) Root has " << (uint32_t)rootBlock->numKeys << " keys:";
    for(int i = 0; i < rootBlock->numKeys; i++) {
        cout << " " << (uint32_t)rootBlock->keys[i];
    }
    free(rootBlock);
    cout << endl;

    bptree->VerifyTree(disk);
}

SearchResult LinearSearch(Disk* searchDisk, int min, int max) {
    uint32_t nBlocks = 0;
    uint32_t totalRecords = 0;
    float totalRating = 0;

    if(min == max) {
        for(int i = FIRST_DATA_BLOCK; i < LAST_DATA_BLOCK + FIRST_DATA_BLOCK; i++){
            DataBlock* block = (DataBlock*)searchDisk->ReadBlock(i);
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
            DataBlock* block = (DataBlock*)searchDisk->ReadBlock(i);
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
    cout << "a) Index Blocks Accessed: " << isr.nInternal << " Internal, " << isr.nLeaf << " Leaf, " << isr.nOverflow << " Overflow. Total is " << isr.nInternal + isr.nLeaf + isr.nOverflow << endl;
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

void Experiment3()
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
        isr = bptree->Search(disk, 500, 500);
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

void Experiment4()
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
        isr = bptree->Search(disk, 30000, 40000);
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

uint32_t LinearDelete(Disk* deletionDisk, uint32_t key) {
    uint32_t nBlocks = 0;
    for(int i = FIRST_DATA_BLOCK; i < LAST_DATA_BLOCK + FIRST_DATA_BLOCK; i++){
        nBlocks++;
        DataBlock* block = (DataBlock*)deletionDisk->ReadBlock(i);
        bool modified = false;
        for(int k = 0; k < RECORDS_PER_BLOCK; k++){
            if(block->records[k].occupied && block->records[k].numVotes == key){
                block->records[k].occupied = false;
                modified = true;
            }
        }
        if(modified) deletionDisk->WriteBlock(i,(uint8_t*)block);
        free(block);
    }
    return nBlocks;
}

void Experiment5() {
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
    uint32_t nInternal = bptree->numInternal;
    uint32_t nOverflow = bptree->numOverflow;
    RecordPointer rPointer = *(bptree->rootPointer);
    uint32_t nDataBlocks = bptree->numDataBlocks;
    uint32_t nLeaf = bptree->numLeaf;
    uint32_t nRecords = bptree->numRecords;

    for(int i = 0; i < TRIALS; i++){
        disk->Copy(diskCopy);
        start = chrono::steady_clock::now();
        bptree->Delete(disk2, 1000);
        end = chrono::steady_clock::now();
        timings.push_back(chrono::duration_cast<chrono::microseconds>(end - start).count());

        // Restore Stats
        bptree->numInternal = nInternal;
        bptree->numDataBlocks = nDataBlocks;
        bptree->numOverflow = nOverflow;
        bptree->numLeaf = nLeaf;
        bptree->numRecords = nRecords;
        *(bptree->rootPointer) = rPointer;
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
    delete disk2;
    linearAvg = Median(timings);

    start = chrono::steady_clock::now();
    bptree->Delete(disk, 1000);
    end = chrono::steady_clock::now();
    indexedAvg = chrono::duration_cast<chrono::microseconds>(end - start).count();
    cout << "> B+ Tree Deletion Statistics" << endl;
    cout << "a) The tree has " << bptree->numLevels << " levels" << endl;
    cout << "b) " << bptree->numInternal << " Internal Nodes, " << bptree->numLeaf << " Leaf Nodes, " << bptree->numOverflow << " Overflow Nodes. Total is " << bptree->numInternal + bptree->numLeaf + bptree->numOverflow << endl;

    IndexBlock* rootBlock = (IndexBlock*)disk->ReadBlock(bptree->rootPointer->blockNumber);
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
    bptree->VerifyTree(disk);
}

int main() {
    Experiment1("data.tsv");
    Experiment2();
    Experiment3();
    Experiment4();
    Experiment5();

    delete disk;
    delete bptree;
}

