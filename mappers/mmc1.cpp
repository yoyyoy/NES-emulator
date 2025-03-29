#include "mapper.h"
#include <iostream>
#include <fstream>


MMC1::MMC1(std::ifstream &romFile, Header header, const std::string& saveName)
{
    currentLayout = (header.is4ScreenVRAM ? FOUR_SCREEN : (header.isHorizontalArrangement ? HORIZONTAL : VERTICAL));

    for (int i = 0; i < header.PRGROMsize; i++)
    {
        ROMBanks.emplace_back();
        romFile.read(ROMBanks.back().data(), 0x4000);
    }

    if(header.CHRROMsize == 0)
    {//cartridge uses RAM. create 2 empty banks
        VROMBanks.emplace_back();
        VROMBanks.emplace_back();
        CHRBank0=0;
        CHRBank1=1;
    }

    for (int i = 0; i < header.CHRROMsize; i++)
    {
        VROMBanks.emplace_back();
        romFile.read(VROMBanks.back().data(), 0x2000);
    }

    if(header.hasBatteryBackedRam)
    {
        savePath = "saves" + std::string{std::filesystem::path::preferred_separator} + saveName + ".sav";
        std::ifstream saveFile(savePath, std::ifstream::basic_ios::binary);
        if (!saveFile.is_open())
        {
            std::cout << "Unable to find save data at " << savePath << '\n';
            for(int i=0; i<0x2000; i++)
                persistentMemory[i]=0;
        }
        else
        {
            saveFile.read((char*)persistentMemory, 0x2000);
        }
    }
}

uint8_t MMC1::ReadCPU(uint16_t address)
{
    if(address < 0x8000 && address>=0x6000)
        return persistentMemory[address-0x6000];

    switch(PRGmode)
    {
    case 0:
    case 1:
        address-=0x8000;
        if(address<0x4000)
            return ROMBanks[PRGBank&0xFE][address];
        else
            return ROMBanks[PRGBank|1][address-0x4000];
    case 2:
        if(address<0xC000)
            return ROMBanks[0][address-0x8000];
        else
            return ROMBanks[PRGBank][address-0xC000];
    case 3:
        if (address < 0xC000)
            return ROMBanks[PRGBank][address - 0x8000];
        else
            return ROMBanks.back()[address - 0xC000];
    }
}

void MMC1::WriteCPU(uint16_t address, uint8_t value)
{
    if (address < 0x8000 && address >= 0x6000)
    {
        persistentMemory[address - 0x6000] = value;
        return;
    }

    if(value & 0b10000000)
    {
        shiftReg=0;
        shiftCount=0;
        PRGmode=3;
        return;
    }

    shiftReg |= (value & 1) << shiftCount++;
    if(shiftCount>=5)
    {
        switch((address & 0b110000000000000) >> 13)
        {
        case 0:
            nametableArrangement = shiftReg & 0b11;
            switch(nametableArrangement)
            {
            case 0:
            case 1:
                currentLayout = SINGLE_SCREEN;
            break;
            case 2:
                currentLayout = HORIZONTAL;
            break;
            case 3:
                currentLayout = VERTICAL;
            break;
            }
            PRGmode=(shiftReg&0b1100) >> 2;
            CHRmode = shiftReg & 0b10000;
        break;
        case 1:
            CHRBank0=shiftReg;
        break;
        case 2:
            CHRBank1=shiftReg;
        break;
        case 3:
            PRGBank=shiftReg;
        }
        shiftCount=0;
        shiftReg=0;
    }
}

uint8_t MMC1::ReadPPU(uint16_t address)
{
    if(address>=0x2000)
    {//nametables
        address = GetRealNameTable(address);
        switch(nametableArrangement)
        {
        case 0:
            return PPUnametable1[address & 0x3FF];
        case 1:
            return PPUnametable2[address & 0x3FF];
        case 2:
            if(address < 0x2400)
                return PPUnametable1[address & 0x3FF];
            else
                return PPUnametable2[address & 0x3FF];
        case 3:
            if (address < 0x2400) 
                return PPUnametable1[address & 0x3FF];
            else 
                return PPUnametable2[address & 0x3FF];
        }
    }

    if(CHRmode)
    {
        if(address<0x1000)
            return VROMBanks[CHRBank0][address];
        else
            return VROMBanks[CHRBank1][address-0x1000];
    }

    if (address < 0x1000)
        return VROMBanks[CHRBank0 & 0xFE][address];
    else
        return VROMBanks[CHRBank0 | 1][address - 0x1000];
}

void MMC1::WritePPU(uint16_t address, uint8_t value)
{
    if(address<0x2000)
    {
        if (CHRmode)
        {
            if (address < 0x1000)
                VROMBanks[CHRBank0][address] = value;
            else
                VROMBanks[CHRBank1][address - 0x1000] = value;
        }
        else
        {
            if (address < 0x1000)
                VROMBanks[CHRBank0 & 0xFE][address] = value;
            else
                VROMBanks[CHRBank0 | 1][address - 0x1000] = value;
        }
        return;
    }
    
    address = GetRealNameTable(address);
    switch (nametableArrangement)
    {
    case 0:
        PPUnametable1[address & 0x3FF] = value;
    case 1:
        PPUnametable2[address & 0x3FF] = value;
    case 2:
        if (address < 0x2400)
            PPUnametable1[address & 0x3FF] = value;
        else
            PPUnametable2[address & 0x3FF] = value;
    case 3:
        if (address < 0x2400)
            PPUnametable1[address & 0x3FF] = value;
        else
            PPUnametable2[address & 0x3FF] = value;
    }
    
}

void MMC1::SaveGame()
{
    std::ofstream saveFile(savePath, std::ifstream::basic_ios::binary);
    saveFile.write((char*)persistentMemory, 0x2000);
    if(saveFile.bad())
        std::cerr << "Failed to write save data to " << savePath.string() << "\n";
    else
        std::cout << "Successfully saved data to " << savePath.string() << "\n";
    
}