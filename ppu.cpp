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

void NES::PPUWrite(uint8_t value)
{//TODO write only to real addresses and not mirrors
    //std::cout << "attempted to write 0x" << std::hex << (uint32_t)value << " to 0x" << PPUstatus.VRAMaddress << "\n"; 
    PPUmemory[PPUstatus.VRAMaddress % 0x4000] = value;
    if(PPUstatus.incrementBy32)
        PPUstatus.VRAMaddress += 32;
    else
        PPUstatus.VRAMaddress++;
}

void NES::PPURenderLine()
{
    Uint32* scanlinePixels =(Uint32*)nesSurface->pixels;
    scanlinePixels += (scanline - (header.isPAL ? 0 : 8)) * 256;
    UpdateSprites();

    uint16_t nameTableAddress;
    for(int i=0; i<256; i++)
    {
        //draw background
        if((PPUstatus.showLeft8PixelsBackground || i>=8) && PPUstatus.displayBackground)
        {
            uint8_t nametable, nametableX, nametableY;
            CalcNameTableCoords(nametable, nametableX, nametableY, i);
            uint8_t patternIndex = PPUGetNameTableByte(nametable, nametableX, nametableY);
            uint8_t yOffset = (scanline+ PPUstatus.Yscroll) % 8;
            uint8_t xOffset = 7 - ((i + PPUstatus.Xscroll) % 8);
            uint8_t firstByte = VROMbanks[activeVROMBank].at(patternIndex*16 + yOffset + (PPUstatus.backgroundPatternTable ? 0x1000 : 0));
            uint8_t secondByte = VROMbanks[activeVROMBank].at(patternIndex * 16 + yOffset + 8 + (PPUstatus.backgroundPatternTable ? 0x1000 : 0));
            uint8_t paletteIndex=0;
            if(firstByte & (1 << xOffset))
                paletteIndex+=1;

            if (secondByte & (1 << xOffset))
                paletteIndex += 2;

            uint8_t nesColor = GetColor(nametable, nametableX, nametableY, paletteIndex);
            RGB color = nesPalette[nesColor];
            scanlinePixels[i]=*((Uint32*) &color);
        }
        else
        {
            RGB color = nesPalette[PPUmemory[0x3F00]];
            scanlinePixels[i] = *((Uint32 *)&color);
        }

        //draw sprites
        if(PPUstatus.showLeft8PixelsSprites || i>=8)
        {

        }
    }
}

void NES::CalcNameTableCoords(uint8_t &nameTable, uint8_t &x, uint8_t &y, uint8_t currentPixel)
{
    nameTable = PPUstatus.currentNameTable;
    x = (currentPixel + PPUstatus.Xscroll) / 8;
    if(x>32)
    {
        x-=32;
        if(nameTable & 1)
            nameTable--;
        else
            nameTable++;
    }
    y = (scanline + PPUstatus.Yscroll) / 8;
    if(y>30)
    {
        y-=30;
        if (nameTable & 0b10)
            nameTable-=2;
        else
            nameTable+=2;
    }
}

uint8_t NES::PPUGetNameTableByte(int nametable, int x, int y)
{
    switch(PPUstatus.nameTableMirror)
    {
    case HORIZONTAL:
        nametable &=0xFE;
    break;
    case VERTICAL:
        nametable &= 0xFD;
    break;
    case SINGLE_SCREEN:
        nametable=0;
    break;
    }
    uint16_t desired = (nametable << 10) + x + (y*32);
    desired += 0x2000;
    return PPUmemory[desired];
}

uint8_t NES::GetColor(uint8_t nametable, uint8_t x, uint8_t y, uint8_t paletteIndex)
{
    uint16_t attributeAddress = 0x23C0 + (nametable << 10) + (x/4) + ((y/4)*8);
    uint8_t attributeByte = PPUmemory[attributeAddress];
    uint8_t mask = 0b11;
    uint8_t maskShift=0;
    if(x&1)
        maskShift++;
    if(y&2)
        maskShift+=2;
    uint8_t paletteNum = (attributeByte & (mask << (maskShift*2))) >> (maskShift*2);
    return PPUmemory[0x3F00 + (paletteNum * 4) + paletteIndex];
}

void NES::UpdateSprites()
{
    spritesOnScanLine.clear();
    for(int i=0; i<64; i++)
    {
        uint32_t* sprite = (uint32_t*)PPUOAM+i;
        int yPos = ((uint8_t*)sprite)[0]+1;
        if(yPos>=scanline && yPos <= scanline + (PPUstatus.is8x16Sprites ? 16 : 8))
        {
            if(spritesOnScanLine.size()>8)
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