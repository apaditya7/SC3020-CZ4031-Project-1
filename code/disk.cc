#include "constants.h"
#include "disk.h"

Disk::Disk() {
    this->disk = (uint8_t*)calloc(DISK_SIZE, sizeof(uint8_t));
}

Disk::Disk(uint8_t* from) {
    this->disk = from;
}

uint8_t* Disk::ReadBlock(uint32_t blockNumber) {
    assert(blockNumber >= FIRST_DATA_BLOCK && blockNumber <= NUM_BLOCKS);
    blockNumber -= FIRST_DATA_BLOCK;

    // pretend to seek and rotate
    uint8_t* blockPointer = (uint8_t*)((uintptr_t)this->disk + (blockNumber * BLOCK_SIZE));
    uint8_t* block = (uint8_t*)calloc(BLOCK_SIZE, sizeof(uint8_t));

    // pretend to transfer
    memcpy(block, blockPointer, BLOCK_SIZE);

    return block;
}

void Disk::WriteBlock(uint32_t blockNumber, uint8_t* block) {
    assert(blockNumber >= FIRST_DATA_BLOCK && blockNumber <= NUM_BLOCKS);
    blockNumber -= FIRST_DATA_BLOCK;

    // pretend to seek and rotate
    uint8_t* blockPointer = (uint8_t*)((uintptr_t)this->disk + (blockNumber * BLOCK_SIZE));

    // pretend to transfer
    memcpy(blockPointer, block, BLOCK_SIZE);
}

void Disk::Copy(uint8_t* to) {
    memcpy(to, this->disk, DISK_SIZE);
}
