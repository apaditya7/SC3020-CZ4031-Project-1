#include <iomanip>
#include <stdio.h>
#include <sstream>
#include <iostream>
#include <fstream>

#include "constants.h"
#include "disk.h"

using namespace std;

uint32_t numRecords;
uint32_t currentFreeBlock = 1;

void Experiment1(Disk* disk, string filename) {
    cout << "Running Experiment 1" << endl;
    ifstream tsv(filename);
    assert(tsv.good());
    
    int numRecords = 0;
    string line;
    getline(tsv, line); // consume the header line
    
    DataBlock* block = (DataBlock*)disk->ReadBlock(currentFreeBlock);
    uint32_t index = 0;
    
    while(getline(tsv, line)) {
        if(line.empty()) continue; // ignore empty lines
        ++numRecords;

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

    cout << "Statistics:" << endl;
    cout << "a) " << numRecords << " Records" << endl;
    cout << "b) Each record has a size of " << RECORD_SIZE <<  "B" << endl;
    cout << "c) " << RECORDS_PER_BLOCK << " Records per Block" << endl;
    cout << "d) " << currentFreeBlock - 1 << " Blocks used" << endl;
}

int main() {
    Disk* disk = new Disk();
    Experiment1(disk, "data.tsv");
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