#ifndef DISK_H
#define DISK_H

class Disk {
    private:
        uint8_t* disk;
    public:
        Disk();
        ~Disk();
        Disk(uint8_t* from);
        uint8_t* ReadBlock(uint32_t blockNumber);
        void WriteBlock(uint32_t blockNumber, uint8_t* block);
        void Copy(uint8_t* to);
};

#endif