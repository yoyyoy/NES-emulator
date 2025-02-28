#include <iostream>
#include <fstream>
#include <cstdint>
#include <vector>
#include <string.h>

class NES
{   
public:
    NES(std::ifstream& file)
    {
        ParseHeader(file);
        InitMemory(file);
    }
    
    void Run();
private:
    struct Header
    {
        uint8_t PRGROMsize;
        uint8_t CHRROMsize;
        bool isHorizontalArrangement;
        bool hasBatteryBackedRam;
        bool hasTrainer;
        bool is4ScreenVRAM;
        uint8_t mapperType;
        bool isVSsystem;
        uint8_t RAMsize;
        bool isPAL;
    } header;

    struct NESregisters
    {
        uint16_t programCounter;
        uint8_t stackPointer = 0xFF;
        uint8_t accumulator;
        uint8_t Xregister;
        uint8_t Yregister;
        uint8_t processorStatus = 0;
    } registers;

    void ParseHeader(std::ifstream &romFile);
    void InitMemory(std::ifstream &romFile);

    char nesMemory[0x10000];
    vector<char[0x4000]> ROMbanks;
    vector<char[0x2000]> VROMbanks;
};