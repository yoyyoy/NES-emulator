#include "nes.h"
#include <iomanip>

uint8_t NES::PPUGet2002()
{
    PPUstatus.firstRead=true;
    uint8_t output=0;
    if(PPUstatus.spriteOverflow) output |= 0b100000;
    if(PPUstatus.hitSprite0) output |= 0b1000000;
    if(PPUstatus.VBlanking) output |= 0b10000000;
    PPUstatus.VBlanking =false;
    return output;
}

uint8_t NES::PPUHandleRegRead(uint8_t reg)
{
    switch(reg)
    {
    case 0:
    case 1:
    case 3:
    case 5:
    case 6: return nesMemory[reg + 0x2000];
    
    case 2: return PPUGet2002();
    case 4: return PPUOAM[PPUstatus.OAMcurrentAddress];
    case 7: return PPUReadMemory();

    }
}

uint8_t NES::PPUReadMemory()
{
    uint16_t oldVRAMAddress = PPUstatus.VRAMaddress;
    if (PPUstatus.incrementBy32) PPUstatus.VRAMaddress += 32;
    else PPUstatus.VRAMaddress++;

    if ((oldVRAMAddress & 0x3FFF) > 0x3F00)
        return PPUmemory[oldVRAMAddress & 0x3FFF];

    uint8_t value = PPUstatus.dataReadBuffer;
    PPUstatus.dataReadBuffer = PPUmemory[oldVRAMAddress];

    return value;
}

void NES::DebugPrintAttribute()
{
    for (int i = 0; i < 8; i++)
    {
        for (int j = 0; j < 8; j++)
        {
            std::cout << std::setfill('0') << std::setw(2) << std::hex << ((uint16_t)(PPUmemory[0x23C0 + j + i * 8]) & 0xFF) << " ";
        }

        for (int j = 0; j < 8; j++)
        {
            std::cout << std::setfill('0') << std::setw(2) << std::hex << ((uint16_t)PPUmemory[0x27C0 + j + i * 8] & 0xFF) << " ";
        }
        std::cout << '\n';
    }
}

void NES::DebugPrintTables()
{
    for(int i=0; i<30; i++)
    {
        for(int j=0; j<32; j++)
        {
            std::cout << std::setfill('0') << std::setw(2) << std::hex << ((uint16_t)(PPUmemory[0x2000 + j + i * 32]) & 0xFF) << " ";
        }

        for (int j = 0; j < 32; j++)
        {
            std::cout << std::setfill('0') << std::setw(2) << std::hex << ((uint16_t)PPUmemory[0x2400 + j + i * 32] & 0xFF) << " ";
        }
        std::cout << '\n';
    }
}

void NES::PPUWrite(uint8_t value)
{//TODO write only to real addresses and not mirrors
    //if(value == 0x3A)
    //std::cout << "attempted to write 0x" << std::hex << (uint32_t)value << " to 0x" << PPUstatus.VRAMaddress << "\n"; 
    //limit to original copy
    auto temp = PPUstatus.VRAMaddress & 0x3FFF;
    
    //handle palette mirrors
    if(temp>=0x3F00)
    {
        temp &= 0x3F1F;

        if (!(temp & 0x0003))
            temp &= 0x3F0F;
    }

    PPUmemory[temp] = value;
    if(PPUstatus.incrementBy32)
        PPUstatus.VRAMaddress += 32;
    else
        PPUstatus.VRAMaddress++;
    //DebugPrintTables();
}

void NES::PPURenderLine()
{
    Uint32* scanlinePixels =(Uint32*)nesSurface->pixels;
    scanlinePixels += (scanline - (header.isPAL ? 0 : 8)) * 256;

    if(!PPUstatus.displayBackground && !PPUstatus.displaySprites)
    {
        RGB color = nesPalette[GetBackDropColor()];
        for (int i = 0; i<256; i++)
        {
            scanlinePixels[i] = *((Uint32 *)&color);
        }
    }

    UpdateSprites();
    
    char nametableBytes[32];
    char attributeTableBytes[8];

    uint8_t nametable, nametableX, nametableY;
    CalcNameTableCoords(nametable, nametableX, nametableY);
    PPUGetNameTableBytes(nametable, nametableX, nametableY, nametableBytes);
    PPUGetAttributeTableBytes(nametable, nametableX, nametableY, attributeTableBytes);
    uint8_t yOffset = (scanline + PPUstatus.Yscroll) % 8;
    bool opaqueBackground[256] = { false };
    for(int i=0; i<32; i++)
    {
        if ((PPUstatus.showLeft8PixelsBackground || i >= 1) && PPUstatus.displayBackground)
        {
            uint8_t patternIndex = nametableBytes[i];
            uint8_t firstByte = VROMbanks[activeVROMBank].at(patternIndex * 16 + yOffset + (PPUstatus.backgroundPatternTable ? 0x1000 : 0));
            uint8_t secondByte = VROMbanks[activeVROMBank].at(patternIndex * 16 + yOffset + 8 + (PPUstatus.backgroundPatternTable ? 0x1000 : 0));
            
            for(int j=0; j<8; j++)
            {
                uint8_t paletteIndex = 0;
                if (firstByte & (1 << 7-j)) paletteIndex += 1;
                if (secondByte & (1 << 7-j)) paletteIndex += 2;
                
                if(paletteIndex) opaqueBackground[i*8+j] = true;

                uint8_t nesColor = GetBackgroundColor(attributeTableBytes[i/4], nametableX, nametableY, paletteIndex);
                RGB color = nesPalette[nesColor];
                scanlinePixels[i*8+j] = *((Uint32 *)&color);
            }
            
            nametableX++;
            if (nametableX >= 32)
            {
                nametableX -= 32;
                if (nametable & 1)
                    nametable = GetRealNameTable(nametable - 1);
                else
                    nametable = GetRealNameTable(nametable + 1);
            }
        }
        else
        {
            RGB color = nesPalette[PPUmemory[0x3F00]];
            for(int j=0; j<8; j++)
            {
                scanlinePixels[i*8+j] = *((Uint32 *)&color);
            }
        }
    }

    if (!PPUstatus.displaySprites || (scanline - (header.isPAL ? 0 : 8)) == 0)
        return;

    for(int j = spritesOnScanLine.size()-1; j>=0; j--)
    {
        auto sprite = spritesOnScanLine[j];
        int yPos = (scanline + PPUstatus.Yscroll) - sprite.yPos;

        //sanity check
        if (yPos < 0 || yPos >= (PPUstatus.is8x16Sprites ? 16 : 8))
        {
            std::cerr << "sprite position out of bounds\n";
            exit(15);
        }

        uint8_t firstByte;
        uint8_t secondByte;
        
        if (PPUstatus.is8x16Sprites)
        {
            if(yPos>=8)
            {
                firstByte = VROMbanks[activeVROMBank].at(((sprite.tileNumber & 0xFE) + 1) * 16 + (yPos - 8) + (sprite.tileNumber & 1 ? 0x1000 : 0));
                secondByte = VROMbanks[activeVROMBank].at(((sprite.tileNumber & 0xFE) + 1) * 16 + (yPos - 8) + 8 + (sprite.tileNumber & 1 ? 0x1000 : 0));
            }
            else
            {
                firstByte = VROMbanks[activeVROMBank].at((sprite.tileNumber & 0xFE) * 16 + yPos + (sprite.tileNumber & 1 ? 0x1000 : 0));
                secondByte = VROMbanks[activeVROMBank].at((sprite.tileNumber & 0xFE) * 16 + yPos + 8 + (sprite.tileNumber & 1 ? 0x1000 : 0));
            }
        }
        else
        {
            firstByte = VROMbanks[activeVROMBank].at(sprite.tileNumber * 16 + yPos + (PPUstatus.spritePatternTable ? 0x1000 : 0));
            secondByte = VROMbanks[activeVROMBank].at(sprite.tileNumber * 16 + yPos + 8 + (PPUstatus.spritePatternTable ? 0x1000 : 0));
        }

        for(int i=0; i<8; i++)
        {
            uint8_t paletteIndex = 0;
            if (firstByte & (1 << 7-i)) paletteIndex += 1;
            if (secondByte & (1 << 7-i)) paletteIndex += 2;
            int pixelPos = (sprite.xPos - PPUstatus.Xscroll) + (sprite.attributes & 0b1000000 ? 7-i : i);
            
            if(pixelPos<8 && !PPUstatus.showLeft8PixelsSprites)
                continue;

            if (pixelPos >= 0 && pixelPos<256)
            {
                if(paletteIndex && opaqueBackground[pixelPos] && sprite.id==0)
                    PPUstatus.hitSprite0=true;

                if((!(sprite.attributes & 0b100000) || !opaqueBackground[pixelPos]) && paletteIndex!=0)
                {
                    uint8_t nesColor = GetSpriteColor(sprite.attributes, paletteIndex);
                    RGB color = nesPalette[nesColor];
                    scanlinePixels[pixelPos] = *((Uint32 *)&color);
                }

            }
        }
    }
    
}

uint8_t NES::GetBackDropColor()
{
    if (!PPUstatus.VRAMaddress >= 0x3F00)
        return PPUmemory[0x3F00];

    auto temp = PPUstatus.VRAMaddress;
    temp -= 0x3F00;
    temp %= 0x20;
    if (temp & 0x0C)
        temp &= 0xEF;
    temp += 0x3F00;
    return PPUmemory[temp];
}

void NES::CalcNameTableCoords(uint8_t &nameTable, uint8_t &x, uint8_t &y)
{
    nameTable = PPUstatus.currentNameTable;
    x = (PPUstatus.Xscroll) / 8;
    if(x>=32)
    {
        x-=32;
        if(nameTable & 1)
            nameTable--;
        else
            nameTable++;
    }
    y = (scanline + PPUstatus.Yscroll) / 8;
    if(y>=30)
    {
        y-=30;
        if (nameTable & 0b10)
            nameTable-=2;
        else
            nameTable+=2;
    }
}

uint8_t NES::GetRealNameTable(uint8_t nametable)
{
    switch (PPUstatus.nameTableMirror)
    {
    case HORIZONTAL: return nametable & 0xFE;
    case VERTICAL: return nametable & 0xFD;
    case SINGLE_SCREEN: return 0;
    default: return nametable;
    }
}

void NES::PPUGetNameTableBytes(int nametable, int x, int y, char* outBytes)
{
    nametable=GetRealNameTable(nametable);
    
    uint16_t desired = 0x2000 + ((nametable << 10) + x + (y*32));
    memcpy(outBytes, PPUmemory + desired, 32 - x);
    if (x != 0)
    {
        if (nametable & 1)
            nametable = GetRealNameTable(nametable-1);
        else
            nametable = GetRealNameTable(nametable+1);
        desired = 0x2000 + ((nametable << 10) + (y * 32));
        memcpy(outBytes + 32 - x, PPUmemory + desired, x);
    }
}

void NES::PPUGetAttributeTableBytes(int nametable, int x, int y, char *outBytes)
{
    nametable = GetRealNameTable(nametable);

    uint16_t desired = 0x23C0 + (nametable << 10) + (x / 4) + ((y / 4) * 8);
    memcpy(outBytes, PPUmemory + desired, 8 - x);
    if (x != 0)
    {
        if (nametable & 1)
            nametable = GetRealNameTable(nametable - 1);
        else
            nametable = GetRealNameTable(nametable + 1);
        desired = 0x23C0 + (nametable << 10) + ((y / 4) * 8);
        memcpy(outBytes + 8 - x, PPUmemory + desired, x);
    }
}

uint8_t NES::GetBackgroundColor(uint8_t attributeByte, uint8_t x, uint8_t y, uint8_t paletteIndex)
{
    uint8_t mask = 0b11;
    uint8_t maskShift=0;
    if(x%4>=2)
        maskShift++;
    if(y%4>=2)
        maskShift+=2;
    uint8_t paletteNum = (attributeByte & (mask << (maskShift*2))) >> (maskShift*2);
    return PPUmemory[0x3F00 + (paletteNum * 4) + paletteIndex];
}

uint8_t NES::GetSpriteColor(uint8_t attributeByte, uint8_t paletteIndex)
{
    // this memory address is mirrored across all palettes
    if (paletteIndex == 0)
        return PPUmemory[0x3F00];

    uint8_t paletteNum = attributeByte & 0b11;
    return PPUmemory[0x3F10 + (paletteNum * 4) + paletteIndex];
}

void NES::UpdateSprites()
{
    spritesOnScanLine.clear();
    for(int i=0; i<64; i++)
    {
        uint32_t* sprite = ((uint32_t*)PPUOAM)+i;
        int yPos = *((uint8_t*)sprite) + 1;
        int yRange = (scanline + PPUstatus.Yscroll) -yPos;
        if (yRange>=0 && yRange < (PPUstatus.is8x16Sprites ? 16 : 8))
        {
            if(spritesOnScanLine.size()>=8)
            {
                PPUstatus.spriteOverflow=true;
                break;
            }
            else
            {
                SpriteData spriteData;
                spriteData.yPos=yPos;
                spriteData.tileNumber = ((uint8_t *)sprite)[1];
                spriteData.attributes = ((uint8_t *)sprite)[2];
                spriteData.xPos = ((uint8_t *)sprite)[3];
                spriteData.id=i;
                spritesOnScanLine.push_back(spriteData);
            }
        }
    }
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
    case 0x14:
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
            memcpy(temp, ROMbanks[activeROMBank1].data() + (value * 0x100), 256);
        }
        else
        {
            memcpy(temp, ROMbanks[activeROMBank2].data() + (value * 0x100), 256);
        }
        memcpy(PPUOAM + PPUstatus.OAMcurrentAddress, temp, 256-PPUstatus.OAMcurrentAddress);
        if(PPUstatus.OAMcurrentAddress !=0)
        {
            memcpy(PPUOAM, temp + 256-PPUstatus.OAMcurrentAddress, PPUstatus.OAMcurrentAddress);
        }
    break;
    }
}