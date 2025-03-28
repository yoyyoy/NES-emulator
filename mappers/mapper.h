#include <cstdint>
#include <array>
#include <fstream>

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
    virtual uint8_t ReadCPU(uint16_t address) =0;
    virtual void WriteCPU(uint16_t address, uint8_t value)=0;

    virtual uint8_t ReadPPU(uint16_t address)=0;
    virtual void WritePPU(uint16_t address, uint8_t value)=0;
};

class NROM : public virtual NESMapper
{
public:
    NROM(std::ifstream &romFile, Header header);

    uint8_t ReadCPU(uint16_t address) override;
    void WriteCPU(uint16_t address, uint8_t value) override;

    uint8_t ReadPPU(uint16_t address) override;
    void WritePPU(uint16_t address, uint8_t value) override;

private:
    uint8_t ROM[0x8000];
    uint8_t VROM[0x4000];
};

class MMC1 : public virtual NESMapper
{
public:
    MMC1(std::ifstream &romFile, Header header);

    uint8_t ReadCPU(uint16_t address) override;
    void WriteCPU(uint16_t address, uint8_t value) override;

    uint8_t ReadPPU(uint16_t address) override;
    void WritePPU(uint16_t address, uint8_t value) override;
private:
    
};