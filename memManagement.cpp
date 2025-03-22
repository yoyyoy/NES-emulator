#include "nes.h"
#include <iomanip>
#include <sstream>

uint8_t NES::GetPlayer1Bit()
{
    if(strobingControllers) return currentStatePlayer1.A ? 1 : 0;

    switch(player1ReadCount++)
    {
    case 0: return prevStatePlayer1.A ? 1 : 0;
    case 1: return prevStatePlayer1.B ? 1 : 0;
    case 2: return prevStatePlayer1.select ? 1 : 0;
    case 3: return prevStatePlayer1.start ? 1 : 0;
    case 4: return prevStatePlayer1.up ? 1 : 0;
    case 5: return prevStatePlayer1.down ? 1 : 0;
    case 6: return prevStatePlayer1.left ? 1 : 0;
    case 7: return prevStatePlayer1.right ? 1 : 0;
    default: return 0;
    }
}

uint8_t NES::GetPlayer2Bit()
{
    if(strobingControllers) return currentStatePlayer2.A ? 1 : 0;

    switch(player2ReadCount++)
    {
    case 0: return prevStatePlayer2.A ? 1 : 0;
    case 1: return prevStatePlayer2.B ? 1 : 0;
    case 2: return prevStatePlayer2.select ? 1 : 0;
    case 3: return prevStatePlayer2.start ? 1 : 0;
    case 4: return prevStatePlayer2.up ? 1 : 0;
    case 5: return prevStatePlayer2.down ? 1 : 0;
    case 6: return prevStatePlayer2.left ? 1 : 0;
    case 7: return prevStatePlayer2.right ? 1 : 0;
    default: return 0;
    }
}


uint16_t NES::Read16Bit(uint16_t address, bool incrementPC)
{
    uint8_t lowByte = Read8Bit(address, incrementPC);
    uint8_t highByte = Read8Bit(address + 1, incrementPC);
    return ((uint16_t)highByte<<8) | lowByte;
}

uint16_t NES::Read16BitWrapAround(uint16_t address)
{
    uint8_t lowByte = Read8Bit(address, false);
    uint8_t highByte = Read8Bit(((address + 1) % 256) + (address & 0xFF00), false);
    return ((uint16_t)highByte << 8) | lowByte;
}

uint8_t NES::Read8Bit(uint16_t address, bool incrementPC)
{
    if(incrementPC)
        registers.programCounter++;

    //mirrored RAM
    if(address < 0x2000) return nesMemory[address % 0x0800];
    
    //mirrored PPU registers
    if(address < 0x4000) 
        return PPUHandleRegRead(address%8);
    
        

    //IO registers
    if(address<0x4020) 
    {
        switch(address-0x4000)
        {
        case 0x16: return GetPlayer1Bit();
        case 0x17: return GetPlayer2Bit();
        default:
            return APUHandleRegisterRead(address-0x4000);
        }
    }

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
    else if(address < 0x4000)
    {
        
        address%=8;
        nesMemory[address+0x2000]=value;
        PPUHandleRegisterWrite(address, value);
    }
    else if(address== 0x4014)
    {
        nesMemory[address] = value;
        PPUHandleRegisterWrite(address-0x4000, value);
    }
    else if(address < 0x4020)
    {
        address-=0x4000;
        
        if(address==0x16)
        {
            bool doStrobe = value & 1;
            if (!doStrobe && strobingControllers)
            { // update prev state only when strobe is turned off
                prevStatePlayer1 = currentStatePlayer1;
                prevStatePlayer2 = currentStatePlayer2;
                player1ReadCount = 0;
                player2ReadCount = 0;
            }
            strobingControllers = doStrobe;
        }
        else if(address < 0x18)
            APUHandleRegisterWrite(address, value);
        
    }
    else
    {
        //handle mapper controls
    }
    //std::stringstream debugString;
    //debugString << std::hex << ((address &0xFF00) >> 8);
    //DebugShowMemory(debugString.str());
}

void NES::PushStack16Bit(uint16_t value)
{
    PushStack8Bit(value >> 8);
    PushStack8Bit(value);
}

void NES::PushStack8Bit(uint8_t value)
{
    nesMemory[registers.stackPointer + 0x100] = value;
    registers.stackPointer--;
    //std::cout << "SP: " << std::setfill('0') << std::setw(2) << std::hex << ((uint16_t)registers.stackPointer & 0xff) << "\n";
    //std::cout << "added value: " << std::setfill('0') << std::setw(2) << std::hex << ((uint16_t)value & 0xff) << "\n";
}

uint16_t NES::PullStack16Bit()
{
    uint8_t lowByte = PullStack8Bit();
    uint8_t highByte = PullStack8Bit();
    return (((uint16_t)highByte << 8) | lowByte);
}

uint8_t NES::PullStack8Bit()
{
    registers.stackPointer++;
    //std::cout << "SP: " << std::setfill('0') << std::setw(2) << std::hex <<((uint16_t)registers.stackPointer& 0xff)<< "\n";
    //std::cout << "removed value: " << std::setfill('0') << std::setw(2) << std::hex << ((uint16_t)nesMemory[registers.stackPointer + 0x100]& 0xff) << "\n";
    return nesMemory[registers.stackPointer + 0x100];
}
