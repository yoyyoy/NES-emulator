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
        uint8_t dataReadBuffer=0;
        uint8_t tempAddress;
    } PPUstatus;

    struct SpriteData
    {
        uint16_t yPos;
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
        uint8_t a;
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

    struct ControllerData
    {
        bool A=false;
        bool B = false;
        bool select = false;
        bool start = false;
        bool up = false;
        bool down = false;
        bool left = false;
        bool right = false;
    };

    void ParseHeader(std::ifstream &romFile);
    void InitMemory(std::ifstream &romFile);
    void InitSDL();

    void ExecuteStep(uint8_t opcode);

    uint16_t Read16Bit(uint16_t address, bool incrementPC);
    uint16_t Read16BitWrapAround(uint16_t address);
    uint8_t Read8Bit(uint16_t address, bool incrementPC);

    void Write8Bit(uint16_t address, uint8_t value);

    void PushStack16Bit(uint16_t value);
    void PushStack8Bit(uint8_t value);

    uint16_t PullStack16Bit();
    uint8_t PullStack8Bit();
    
    uint16_t GetOperandAddress(AddressMode addressMode);
    std::pair<uint16_t, uint16_t> GetOperandAddressValue(AddressMode addressMode);
    void UpdateZeroAndNegativeFlags(uint16_t result);
    void SetProcessorStatusFlag(int bitNum, bool set);
    uint8_t GetProcessorStatusFlag(int bitNum);

    uint8_t PPUHandleRegRead(uint8_t reg);
    uint8_t PPUGet2002();
    uint8_t PPUReadMemory();
    void PPUWrite(uint8_t value);
    void PPURenderLine();
    void PPUHandleRegisterWrite(uint8_t reg, uint8_t value);
    void CalcNameTableCoords(uint8_t& nameTable, uint8_t& x, uint8_t& y);
    void PPUGetNameTableBytes(uint8_t nametable, uint8_t x, uint8_t y, char *outBytes);
    void PPUGetAttributeTableBytes(uint8_t nametable, uint8_t x, uint8_t y, char *outBytes);
    uint8_t GetRealNameTable(uint8_t nametable);
    uint8_t GetBackgroundColor(uint8_t attributeByte, uint8_t x, uint8_t y, uint8_t paletteIndex);
    uint8_t GetBackDropColor();
    uint8_t GetSpriteColor(uint8_t attributeByte, uint8_t paletteIndex);
    void UpdateSprites();

    void add6502(uint8_t value);
    void subtract6502(uint8_t value);
    void and6502(uint8_t value);
    void or6502(uint8_t value);
    void eor6502(uint8_t value);
    void bit6502(uint8_t value);
    void shift6502(bool left, bool rotate, bool acc, std::pair<uint16_t, uint16_t> valueAddress);
    void compare6502(uint8_t value, uint8_t value2);
    void branch6502(StatusFlags flag, bool set, int8_t value);
    void break6502();
    void jsr6502();
    void rti6502();
    void store6502(uint16_t address, uint8_t value);
    void load6502(uint8_t value, uint8_t &dest);
    void transfer6502(uint8_t source, uint8_t& destination, bool updateFlags);
    void inc6502(uint8_t& value, bool dec);
    void incMem6502(std::pair<uint16_t, uint16_t> valueAddress, bool dec);

    uint8_t GetPlayer1Bit();
    uint8_t GetPlayer2Bit();

    void DebugRenderFrame(bool fullFrame);
    void DebugPrint();
    void DebugShowMemory(std::string page);
    void DebugPrintTables();
    void DebugPrintAttribute();
    void DebugPrintOAM();

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

    bool strobingControllers;
    uint8_t player1ReadCount;
    uint8_t player2ReadCount;
    ControllerData currentStatePlayer1;
    ControllerData prevStatePlayer1;
    ControllerData currentStatePlayer2;
    ControllerData prevStatePlayer2;

    const std::string debugInstructionToString[57] = {
        "ADC", "AND", "ASL", "BCC", "BCS", "BEQ", "BIT", "BMI", "BNE", "BPL", "BRK", "BVC", "BVS", "CLC", "CLD", "CLI", "CLV", "CMP", "CPX", "CPY",
        "DEC", "DEX", "DEY", "EOR", "INC", "INX", "INY", "JMP", "JSR", "LDA", "LDX", "LDY", "LSR", "NOP", "ORA", "PHA", "PHP", "PLA", "PLP", "ROL",
        "ROR", "RTI", "RTS", "SBC", "SEC", "SED", "SEI", "STA", "STX", "STY", "TAX", "TAY", "TSX", "TXA", "TXS", "TYA", "INVALID_INSTRUCTION"};
    const std::string debugAddressModeToString[15] = {
        "ACCUMULATOR", "J_ABSOLUTE", "ABSOLUTE", "ABSOLUTE_X_INDEX", "ABSOLUTE_Y_INDEX", "IMMEDIATE", "IMPLIED", "INDIRECT",
        "INDIRECT_X_INDEX", "INDIRECT_Y_INDEX", "RELATIVE", "ZEROPAGE", "ZEROPAGE_X_INDEX", "ZEROPAGE_Y_INDEX", "INVALID_ADDRESS_MODE"
    };

    bool debug = false;

    const RGB nesPalette[0x40] = {
        RGB{124,124,124,255},
        RGB{0,0,252,255},
        RGB{0,0,188,255},
        RGB{68,40,188,255},
        RGB{148,0,132,255},
        RGB{168,0,32,255},
        RGB{168,16,0,255},
        RGB{136,20,0,255},
        RGB{80,48,0,255},
        RGB{0,120,0,255},
        RGB{0,104,0,255},
        RGB{0,88,0,255},
        RGB{0,64,88,255},
        RGB{0,0,0,255},
        RGB{0,0,0,255},
        RGB{0,0,0,255},
        RGB{188,188,188,255},
        RGB{0,120,248,255},
        RGB{0,88,248,255},
        RGB{104,68,252,255},
        RGB{216,0,204,255},
        RGB{228,0,88,255},
        RGB{248,56,0,255},
        RGB{228,92,16,255},
        RGB{172,124,0,255},
        RGB{0,184,0,255},
        RGB{0,168,0,255},
        RGB{0,168,68,255},
        RGB{0,136,136,255},
        RGB{0,0,0,255},
        RGB{0,0,0,255},
        RGB{0,0,0,255},
        RGB{248,248,248,255},
        RGB{60,188,252,255},
        RGB{104,136,252,255},
        RGB{152,120,248,255},
        RGB{248,120,248,255},
        RGB{248,88,152,255},
        RGB{248,120,88,255},
        RGB{252,160,68,255},
        RGB{248,184,0,255},
        RGB{184,248,24,255},
        RGB{88,216,84,255},
        RGB{88,248,152,255},
        RGB{0,232,216,255},
        RGB{120,120,120,255},
        RGB{0,0,0,255},
        RGB{0,0,0,255},
        RGB{252,252,252,255},
        RGB{164,228,252,255},
        RGB{184,184,248,255},
        RGB{216,184,248,255},
        RGB{248,184,248,255},
        RGB{248,164,192,255},
        RGB{240,208,176,255},
        RGB{252,224,168,255},
        RGB{248,216,120,255},
        RGB{216,248,120,255},
        RGB{184,248,184,255},
        RGB{184,248,216,255},
        RGB{0,252,252,255},
        RGB{248,216,248,255},
        RGB{0,0,0,255},
        RGB{0,0,0,255}
    };
};