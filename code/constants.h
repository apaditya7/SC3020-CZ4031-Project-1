#ifndef CONSTANTS_H
#define CONSTANTS_H

#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>

typedef struct Record {
    bool occupied;
    uint8_t _; // for alignment
    char tconst[10];
    float averageRating;
    uint32_t numVotes;
} Record;

typedef struct DataBlock {
    Record records[10];
} DataBlock;

typedef struct IndexBlock {
    uint8_t nodeType;
    uint8_t numKeys;
    uint16_t _; // for alignment
    uint32_t keys[24];
    uint32_t pointers[25];
} IndexBlock;

#define DISK_SIZE 200 * 1024 * 1024
#define RECORD_SIZE sizeof(Record)
#define BLOCK_SIZE sizeof(DataBlock)
#define NUM_BLOCKS DISK_SIZE/BLOCK_SIZE
#define RECORDS_PER_BLOCK BLOCK_SIZE/RECORD_SIZE

#endif