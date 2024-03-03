#ifndef CONSTANTS_H
#define CONSTANTS_H

#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <math.h>

#define N 24
#define TRIALS 100
#define FIRST_DATA_BLOCK 1
#define DISK_SIZE 100 * 1024 * 1024
#define RECORD_SIZE sizeof(Record)
#define BLOCK_SIZE sizeof(DataBlock)
#define NUM_BLOCKS DISK_SIZE/BLOCK_SIZE
#define RECORDS_PER_BLOCK BLOCK_SIZE/RECORD_SIZE

#define ASSERT(cond, msg, args...) assert((cond) || !fprintf(stderr, (msg "\n"), args))

#define TYPE_LEAF 0xF
#define TYPE_INTERNAL 0xA
#define TYPE_OVERFLOW 0xD

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

typedef struct RecordPointer {
    uint32_t blockNumber : 28;
    uint32_t recordIndex : 4;
} RecordPointer;

typedef struct IndexBlock {
    uint8_t nodeType;
    uint8_t numKeys;
    uint16_t _; // for alignment
    uint32_t keys[N];
    RecordPointer pointers[N+1];
} IndexBlock;

typedef struct OverflowBlock {
    uint8_t nodeType;
    uint8_t numKeys;
    uint16_t _; // for alignment
    RecordPointer pointers[N*2];
    RecordPointer next;
} OverflowBlock;

typedef struct TraversalData {
    RecordPointer pointer;
    IndexBlock* block;
} TraversalData;

typedef struct vData {
    RecordPointer p;
    uint32_t n1;
    uint32_t n2;
} vData;

typedef struct SearchResult {
    uint32_t nData;
    uint32_t recordsFound;
    float averageRating;
} SearchResult;

typedef struct IndexedSearchResult {
    uint32_t nInternal;
    uint32_t nLeaf;
    uint32_t nOverflow;
    long long unsigned int nData;
    uint32_t recordsFound;
    float averageRating;
} IndexedSearchResult;

#endif
