#include <cstdint>
#include <array>
#include <fstream>
#include <vector>
#include <filesystem>

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
};

class NESMapper
{
public:
    virtual uint8_t ReadCPU(uint16_t address)=0;
    virtual void WriteCPU(uint16_t address, uint8_t value)=0;

    virtual uint8_t ReadPPU(uint16_t address)=0;
    virtual void WritePPU(uint16_t address, uint8_t value)=0;

    virtual void SaveGame()=0;
protected:
    enum NametableLayout
    {
        HORIZONTAL,
        VERTICAL,
        SINGLE_SCREEN,
        FOUR_SCREEN
    } currentLayout;

    uint16_t GetRealNameTable(uint16_t address)
    {
        if(address<0x2000)
            return address;
        switch (currentLayout)
        {
        case HORIZONTAL:
            return address & 0xF7FF;
        case VERTICAL:
            return address & 0xFBFF;
        case SINGLE_SCREEN:
            return address & 0xF3FF;
        default:
            return address;
        }
    }
};

class NROM : public NESMapper
{
public:
    NROM(std::ifstream &romFile, Header header);

    uint8_t ReadCPU(uint16_t address) override;
    void WriteCPU(uint16_t address, uint8_t value) override;

    uint8_t ReadPPU(uint16_t address) override;
    void WritePPU(uint16_t address, uint8_t value) override;

    void SaveGame() override;

private:
    uint8_t ROM[0x8000];
    uint8_t VROM[0x4000];
};

class MMC1 : public NESMapper
{
public:
    MMC1(std::ifstream &romFile, Header header, const std::string& saveName);

    uint8_t ReadCPU(uint16_t address) override;
    void WriteCPU(uint16_t address, uint8_t value) override;

    uint8_t ReadPPU(uint16_t address) override;
    void WritePPU(uint16_t address, uint8_t value) override;

    void SaveGame() override;
private:
    std::vector<std::array<char,0x4000>> ROMBanks;
    std::vector<std::array<char,0x1000>> VROMBanks;
    uint8_t PPUnametable1[0x400];
    uint8_t PPUnametable2[0x400];
    uint8_t persistentMemory[0x2000];    

    uint8_t nametableArrangement;
    uint8_t PRGmode=3;
    bool CHRmode;

    uint8_t CHRBank0;
    uint8_t CHRBank1;
    uint8_t PRGBank;

    uint8_t shiftReg;
    uint8_t shiftCount;

    std::filesystem::path savePath;
    bool hasPersistent=false;
};

class MMC3 : public NESMapper
{
public:
    MMC3(std::ifstream &romFile, Header header, const std::string &saveName);

    uint8_t ReadCPU(uint16_t address) override;
    void WriteCPU(uint16_t address, uint8_t value) override;

    uint8_t ReadPPU(uint16_t address) override;
    void WritePPU(uint16_t address, uint8_t value) override;

    void SaveGame() override;
private:
    
};