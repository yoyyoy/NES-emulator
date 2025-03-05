#include "nes.h"

uint8_t NES::PPUGet2002()
{
    PPUstatus.firstRead=true;
    uint8_t output=0;
    if(PPUstatus.spriteOverflow) output |= 0b100000;
    if(PPUstatus.hitSprite0) output |= 0b1000000;
    if(PPUstatus.VBlanking) output |= 0b10000000;

    return output;
}

void NES::PPUWrite(uint8_t value)
{
    PPUmemory[PPUstatus.VRAMaddress % 0x4000] = value;
    if(PPUstatus.incrementBy32)
        PPUstatus.VRAMaddress += 32;
    else
        PPUstatus.VRAMaddress++;
}

void PPURenderLine()
{
    //TODO
}

void NES::PPUHandleRegisterWrite(uint8_t reg, uint8_t value)
{
    switch (reg)
    {
    case 0:
        PPUstatus.currentNameTable = value & 0b11;
        PPUstatus.incrementBy32 = value & 0b100;
        PPUstatus.spritePatternTable = value & 0b1000;
        PPUstatus.backgroundPatternTable = value & 0b10000;
        PPUstatus.is8x16Sprites = value & 0b100000;
        PPUstatus.doNMI = value & 0b10000000;
        break;
    case 1:
        PPUstatus.monochromeMode = value & 1;
        PPUstatus.showLeft8PixelsBackground = value & 0b10;
        PPUstatus.showLeft8PixelsSprites = value & 0b100;
        PPUstatus.displayBackground = value & 0b1000;
        PPUstatus.displaySprites = value & 0b10000;
        PPUstatus.backgroundColorIntensity = (value & 0b11100000) >> 5;
        break;
    case 3:
        PPUstatus.OAMcurrentAddress = value;
        break;
    case 4:
        PPUOAM[PPUstatus.OAMcurrentAddress] = value;
        PPUstatus.OAMcurrentAddress++;
        break;
    case 5:
        if (PPUstatus.firstRead)
            PPUstatus.Xscroll = value;
        else
            PPUstatus.Yscroll = value;
        PPUstatus.firstRead = !PPUstatus.firstRead;
        break;
    case 6:
        if (PPUstatus.firstRead)
            PPUstatus.VRAMaddress = (PPUstatus.VRAMaddress & 0x00FF) | ((uint16_t)value << 8);
        else
            PPUstatus.VRAMaddress = (PPUstatus.VRAMaddress & 0xFF00) | value;
        PPUstatus.firstRead = !PPUstatus.firstRead;
        break;
    case 7:
        PPUWrite(value);
    break;
    case 14:
        char temp[256];
        if (value < 0x20)
        {
            value %= 8;
            memcpy(temp, nesMemory + (value * 0x100), 256);
        }
        else if (value < 0x80)
        {
            memcpy(temp, nesMemory + (value * 0x100), 256);
        }
        else if (value < 0xC0)
        {
            memcpy(temp, ROMbanks[activeROMBank1] + (value * 0x100), 256);
        }
        else
        {
            memcpy(temp, ROMbanks[activeROMBank2] + (value * 0x100), 256);
        }
        memcpy(PPUOAM + PPUstatus.OAMcurrentAddress, temp, 256-PPUstatus.OAMcurrentAddress);
        if(PPUstatus.OAMcurrentAddress !=0)
        {
            memcpy(PPUOAM, temp + 256-PPUstatus.OAMcurrentAddress, PPUstatus.OAMcurrentAddress);
        }
    break;
    }
}