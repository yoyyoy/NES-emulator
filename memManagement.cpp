#include "nes.h"

uint16_t NES::Read16Bit(uint16_t address, bool incrementPC)
{
    uint8_t lowByte = Read8Bit(address, incrementPC);
    uint8_t highByte = Read8Bit(address + 1, incrementPC);
    return ((uint16_t)highByte<<8) | lowByte;
}

uint8_t NES::Read8Bit(uint16_t address, bool incrementPC)
{
    if(incrementPC)
        registers.programCounter++;

    //mirrored RAM
    if(address < 0x2000) return nesMemory[address % 0x0800];
    
    //mirrored PPU registers
    if(address < 0x4000) return nesMemory[(address%8)+0x2000];

    //IO registers
    if(address<0x4020) return nesMemory[address];

    if(address<0x8000)
    {
        //TODO potentially mapper related stuff
        return nesMemory[address];
    }

    //TODO i dunno if mappers do stuff when reading
    //ROM
    if(address<0xC000) return ROMbanks[activeROMBank1][address-0x8000];
    else return ROMbanks[activeROMBank2][address-0xC000];

}

void NES::Write8Bit(uint16_t address, uint8_t value)
{
    if(address < 0x2000) 
        nesMemory[address%0x0800] = value;
    else if(address < 0x4020)
    {
        //TODO handle PPU stuff and io stuff
    }
    else
    {
        //handle mapper controls
    }
}

void NES::PushStack16Bit(uint16_t value)
{
    PushStack8Bit(value);
    PushStack8Bit(value >> 8);
}

void NES::PushStack8Bit(uint8_t value)
{
    nesMemory[registers.stackPointer + 0x100] = value;
    registers.stackPointer--;
}

uint16_t NES::PullStack16Bit()
{
    uint8_t highByte = PullStack8Bit();
    uint8_t lowByte = PullStack8Bit();
    return ((uint16_t)highByte << 8) | lowByte;
}

uint8_t NES::PullStack8Bit()
{
    registers.stackPointer++;
    return nesMemory[registers.stackPointer + 0x100];
}