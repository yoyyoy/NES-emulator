#include "mapper.h"
#include <string.h>
#include <iostream>

NROM::NROM(std::ifstream &romFile, Header header)
{
    for (int i = 0; i < header.PRGROMsize; i++)
        romFile.read((char*)ROM + i*0x4000, 0x4000);
    if (header.PRGROMsize == 1)
        memcpy(ROM + 0x4000, ROM, 0x4000);

    if(header.CHRROMsize!=1)
    {
        std::cerr << "NROM file does not have correct CHRROM size\n";
        exit(14);
    }

    romFile.read((char *)VROM, 0x2000);
}

uint8_t NROM::ReadCPU(uint16_t address)
{
    return ROM[address-0x8000];
}

void NROM::WriteCPU(uint16_t address, uint8_t value)
{
    return;
}

uint8_t NROM::ReadPPU(uint16_t address)
{
    return VROM[address];
}

void NROM::WritePPU(uint16_t address, uint8_t value)
{
    VROM[address]=value;
}