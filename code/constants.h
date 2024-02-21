#ifndef CONSTANTS_H
#define CONSTANTS_H

#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>

#define N 24
#define DISK_SIZE 200 * 1024 * 1024
#define RECORD_SIZE sizeof(Record)
#define BLOCK_SIZE sizeof(DataBlock)
#define NUM_BLOCKS DISK_SIZE/BLOCK_SIZE
#define RECORDS_PER_BLOCK BLOCK_SIZE/RECORD_SIZE

#define ASSERT(cond, msg, args...) assert((cond) || !fprintf(stderr, (msg "\n"), args))

#define LEAF 0xF
#define INTERNAL 0xA
#define DUPLICATES 0xD

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

typedef struct DuplicatesBlock {
    uint8_t nodeType;
    uint8_t numKeys;
    uint16_t _; // for alignment
    RecordPointer pointers[N*2];
    RecordPointer next;
} DuplicatesBlock;

typedef struct TraversalData {
    RecordPointer pointer;
    IndexBlock* block;
} TraversalData;

typedef struct vData {
    RecordPointer p;
    uint32_t n1;
    uint32_t n2;
} vData;


#endif