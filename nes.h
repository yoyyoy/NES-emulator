#include <iostream>
#include <fstream>
#include <cstdint>
#include <vector>
#include <string.h>
#include <SDL2/SDL.h>
#include <array>

class NES
{   
public:
    NES(std::ifstream& file)
    {
        ParseHeader(file);
        InitMemory(file);
        InitSDL();

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
    
    enum MirrorType
    {
        HORIZONTAL,
        VERTICAL,
        SINGLE_SCREEN,
        FOUR_SCREEN
    };
    
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
        MirrorType nameTableMirror;
    } PPUstatus;

    struct SpriteData
    {
        uint8_t yPos;
        uint8_t tileNumber;
        uint8_t attributes;
        uint8_t xPos;
        uint8_t id;
    };

    struct RGB
    {
        uint8_t r;
        uint8_t g;
        uint8_t b;
    };

    enum Instruction
    {
        ADC, AND, ASL, BCC, BCS, BEQ, BIT, BMI, BNE, BPL, BRK, BVC, BVS, CLC, CLD, CLI, CLV, CMP, CPX, CPY,
        DEC, DEX, DEY, EOR, INC, INX, INY, JMP, JSR, LDA, LDX, LDY, LSR, NOP, ORA, PHA, PHP, PLA, PLP, ROL,
        ROR, RTI, RTS, SBC, SEC, SED, SEI, STA, STX, STY, TAX, TAY, TSX, TXA, TXS, TYA, INVALID_INSTRUCTION
    };

    enum AddressMode
    {
        ACCUMULATOR, J_ABSOLUTE, ABSOLUTE, ABSOLUTE_X_INDEX, ABSOLUTE_Y_INDEX, IMMEDIATE, IMPLIED, INDIRECT,
        INDIRECT_X_INDEX, INDIRECT_Y_INDEX, RELATIVE, ZEROPAGE, ZEROPAGE_X_INDEX, ZEROPAGE_Y_INDEX, INVALID_ADDRESS_MODE
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
    void InitSDL();

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
    void CalcNameTableCoords(uint8_t& nameTable, uint8_t& x, uint8_t& y, uint8_t currentPixel);
    uint8_t PPUGetNameTableByte(int nametable, int x, int y);
    uint8_t GetPaletteIndex(uint8_t patternIndex);
    uint8_t GetColor(uint8_t nametable, uint8_t x, uint8_t y, uint8_t paletteIndex);
    void UpdateSprites();

    char nesMemory[0x10000];
    char PPUmemory[0x4000];
    char PPUOAM[256];
    std::vector<std::array<char,0x4000>> ROMbanks;
    std::vector<std::array<char, 0x2000>> VROMbanks;
    int scanline=0;
    int PPUcycles=0;
    const int PPUcyclesPerLine=341;

    int activeROMBank1;
    int activeROMBank2;
    int activeVROMBank=0;

    //differ depending on NTSC or PAL
    int numVBlankLines;
    int numTotalLines;
    float msPerFrame;

    std::vector<SpriteData> spritesOnScanLine;

    SDL_Window* win;
    SDL_Surface* windowSurface;
    SDL_Surface* nesSurface;
    SDL_Rect stretchRect;

    const std::string debugInstructionToString[57] = {
        "ADC", "AND", "ASL", "BCC", "BCS", "BEQ", "BIT", "BMI", "BNE", "BPL", "BRK", "BVC", "BVS", "CLC", "CLD", "CLI", "CLV", "CMP", "CPX", "CPY",
        "DEC", "DEX", "DEY", "EOR", "INC", "INX", "INY", "JMP", "JSR", "LDA", "LDX", "LDY", "LSR", "NOP", "ORA", "PHA", "PHP", "PLA", "PLP", "ROL",
        "ROR", "RTI", "RTS", "SBC", "SEC", "SED", "SEI", "STA", "STX", "STY", "TAX", "TAY", "TSX", "TXA", "TXS", "TYA", "INVALID_INSTRUCTION"};
    const std::string debugAddressModeToString[15] = {
        "ACCUMULATOR", "J_ABSOLUTE", "ABSOLUTE", "ABSOLUTE_X_INDEX", "ABSOLUTE_Y_INDEX", "IMMEDIATE", "IMPLIED", "INDIRECT",
        "INDIRECT_X_INDEX", "INDIRECT_Y_INDEX", "RELATIVE", "ZEROPAGE", "ZEROPAGE_X_INDEX", "ZEROPAGE_Y_INDEX", "INVALID_ADDRESS_MODE"
    };

    const RGB nesPalette[0x40] = {
        RGB{124,124,124 },
        RGB{0,0,252},
        RGB{0,0,188},
        RGB{68,40,188},
        RGB{148,0,132},
        RGB{168,0,32},
        RGB{168,16,0},
        RGB{136,20,0},
        RGB{80,48,0},
        RGB{0,120,0},
        RGB{0,104,0},
        RGB{0,88,0},
        RGB{0,64,88},
        RGB{0,0,0},
        RGB{0,0,0},
        RGB{0,0,0},
        RGB{188,188,188},
        RGB{0,120,248},
        RGB{0,88,248},
        RGB{104,68,252},
        RGB{216,0,204},
        RGB{228,0,88},
        RGB{248,56,0},
        RGB{228,92,16},
        RGB{172,124,0},
        RGB{0,184,0},
        RGB{0,168,0},
        RGB{0,168,68},
        RGB{0,136,136},
        RGB{0,0,0},
        RGB{0,0,0},
        RGB{0,0,0},
        RGB{248,248,248},
        RGB{60,188,252},
        RGB{104,136,252},
        RGB{152,120,248},
        RGB{248,120,248},
        RGB{248,88,152},
        RGB{248,120,88},
        RGB{252,160,68},
        RGB{248,184,0},
        RGB{184,248,24},
        RGB{88,216,84},
        RGB{88,248,152},
        RGB{0,232,216},
        RGB{120,120,120},
        RGB{0,0,0},
        RGB{0,0,0},
        RGB{252,252,252},
        RGB{164,228,252},
        RGB{184,184,248},
        RGB{216,184,248},
        RGB{248,184,248},
        RGB{248,164,192},
        RGB{240,208,176},
        RGB{252,224,168},
        RGB{248,216,120},
        RGB{216,248,120},
        RGB{184,248,184},
        RGB{184,248,216},
        RGB{0,252,252},
        RGB{248,216,248},
        RGB{0,0,0},
        RGB{0,0,0}
    };
};