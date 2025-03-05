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

    struct PPUstatus
    {
        uint8_t currentNameTable;
        bool incrementBy32;
        bool spritePatternTable;
        bool backgroundPatternTable;
        bool is8x16Sprites;
        bool doNMI;
        bool monochromeMode;
        bool showLeft8PixelsBackground;
        bool showLeft8PixelsSprites;
        bool displayBackground;
        bool displaySprites;
        uint8_t backgroundColorIntensity;
        bool hitSprite0=false;
        bool spriteOverflow=false;
        bool VBlanking;
        uint8_t OAMcurrentAddress;
        uint8_t Xscroll;
        uint8_t Yscroll;
        uint16_t VRAMaddress;
        bool firstRead=true;
    } PPUstatus;

    enum Instruction
    {
        ADC, AND, ASL, BCC, BCS, BEQ, BIT, BMI, BNE, BPL, BRK, BVC, BVS, CLC, CLD, CLI, CLV, CMP, CPX, CPY,
        DEC, DEX, DEY, EOR, INC, INX, INY, JMP, JSR, LDA, LDX, LDY, LSR, NOP, ORA, PHA, PHP, PLA, PLP, ROL,
        ROR, RTI, RTS, SBC, SEC, SED, SEI, STA, STX, STY, TAX, TAY, TSX, TXA, TXS, TYA, INVALID
    };

    enum AddressMode
    {
        ACCUMULATOR, J_ABSOLUTE, ABSOLUTE, ABSOLUTE_X_INDEX, ABSOLUTE_Y_INDEX, IMMEDIATE, IMPLIED, INDIRECT,
        INDIRECT_X_INDEX, INDIRECT_Y_INDEX, RELATIVE, ZEROPAGE, ZEROPAGE_X_INDEX, ZEROPAGE_Y_INDEX, INVALID
    };

    enum StatusFlags
    {
        CARRY=0,
        ZERO=1,
        INTERRUPT_DISABLE=2,
        DECIMAL=3,
        BREAK=4,
        OVERFLOW=6,
        NEGATIVE=7
    };

    void ParseHeader(std::ifstream &romFile);
    void InitMemory(std::ifstream &romFile);
    void Execute(Instruction instruction, AddressMode addressMode);

    //helper functions for opcode parsing
    Instruction Low159D(uint8_t upper);
    Instruction Low6E(uint8_t upper);
    void ParseOpcode(uint8_t opcode, Instruction &instruction, AddressMode &addressMode);

    uint16_t Read16Bit(uint16_t address, bool incrementPC);
    uint8_t Read8Bit(uint16_t address, bool incrementPC);

    void Write8Bit(uint16_t address, uint8_t value);

    void PushStack16Bit(uint16_t value);
    void PushStack8Bit(uint8_t value);

    uint16_t PullStack16Bit();
    uint8_t PullStack8Bit();

    uint16_t GetOperandValue(AddressMode addressMode, uint16_t &address);
    void UpdateProcessorStatus(uint16_t result, int32_t signedResult, Instruction instruction);
    void SetProcessorStatusBit(int bitNum, bool set);

    uint8_t PPUGet2002();
    void PPUWrite(uint8_t value);
    void PPURenderLine();
    void PPUHandleRegisterWrite(uint8_t reg, uint8_t value);

    char nesMemory[0x10000];
    char PPUmemory[0x4000];
    char PPUOAM[256];
    vector<char[0x4000]> ROMbanks;
    vector<char[0x2000]> VROMbanks;
    int scanline=0;
    int PPUcycles=0;
    const int PPUcyclesPerLine=340;

    int activeROMBank1;
    int activeROMBank2;
    int activeVROMBank=0;

    //differ depending on NTSC or PAL
    int numVBlankLines;
    int numTotalLines;
    float msPerFrame;
};