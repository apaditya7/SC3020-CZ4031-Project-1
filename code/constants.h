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

#define RECORD_SIZE sizeof(NBARecord) 
#define BLOCK_SIZE sizeof(DataBlock)
#define NUM_BLOCKS DISK_SIZE/BLOCK_SIZE
#define RECORDS_PER_BLOCK BLOCK_SIZE/RECORD_SIZE

#define ASSERT(cond, msg, args...) assert((cond) || !fprintf(stderr, (msg "\n"), args))

#define TYPE_LEAF 0xF
#define TYPE_INTERNAL 0xA
#define TYPE_OVERFLOW 0xD

struct NBARecord {
    std::string game_date_est;
    int team_id_home;
    int pts_home;
    float fg_pct_home;
    float ft_pct_home;
    float fg3_pct_home;
    int ast_home;
    int reb_home;
    int home_team_wins;

    NBARecord() : game_date_est(""), team_id_home(0), pts_home(0), fg_pct_home(0.0), ft_pct_home(0.0),
                  fg3_pct_home(0.0), ast_home(0), reb_home(0), home_team_wins(0) {}
};

struct DataBlock {
    NBARecord records[10];
    bool occupied[10]; 
};

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
