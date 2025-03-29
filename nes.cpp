#include "nes.h"
#include <chrono>
#include <thread>
#include <iomanip>
#include <sstream>
using namespace std;

void NES::ParseHeader(ifstream &romFile)
{
    char magicBytes[4];
    romFile.read(magicBytes, 4);

    if (!(magicBytes[0] == 0x4E && magicBytes[1] == 0x45 && magicBytes[2] == 0x53 && magicBytes[3] == 0x1A))
    {
        cerr << "Invalid header\n";
        exit(3);
    }

    romFile.read((char *)&header.PRGROMsize, 1);
    romFile.read((char *)&header.CHRROMsize, 1);

    char flags6;
    romFile.read(&flags6, 1);
    header.isHorizontalArrangement = flags6 & 1;
    header.hasBatteryBackedRam = flags6 & 0b10;
    header.hasTrainer = flags6 & 0b100;
    header.is4ScreenVRAM = flags6 & 0b1000;

    char flags7;
    romFile.read(&flags7, 1);
    header.isVSsystem = flags7 & 1;
    header.mapperType = ((flags6 >> 4) & 0x0F) | (flags7 & 0xF0);

    romFile.read((char *)&header.RAMsize, 1);
    if (header.RAMsize == 0)
        header.RAMsize = 1;

    char flags9;
    romFile.read(&flags9, 1);
    header.isPAL = flags9 & 1;

    char unusedData[6];
    romFile.read(unusedData, 6);
}

void NES::InitMemory(ifstream &romFile, string name)
{
    if (header.hasTrainer)
    {
        cout << "Warning: ROM file indicates it includes a 512 Byte trainer. This is rare and untested on this emulator\n";
        romFile.read(nesMemory + 0x7000, 512);
    }

    switch(header.mapperType)
    {
    case 0:
        mapper = make_unique<NROM>(romFile, header);
    break;
    case 1:
        mapper = make_unique<MMC1>(romFile, header, name);
    break;
    default:
        cerr << "Mapper not supported: " << (int)header.mapperType << "\n";
        exit(5);
    }
    
    if (romFile.fail())
    {
        cerr << "Error while reading file\n";
        exit(4);
    }

    if(header.isPAL)
    {
        numVBlankLines=72;
        numTotalLines=312;
        msPerFrame=20;
        DMC.frequencyDecoded = 398;
    }
    else
    {
        numVBlankLines=30;
        numTotalLines=262;
        msPerFrame=16.66666;
        scanline=8;
        DMC.frequencyDecoded = 428;
    }
    registers.programCounter = Read16Bit(0xFFFC, false);
}

void UpdateAudioBuffer(void* userdata, Uint8* stream, int len)
{
    NES* cast = (NES*)userdata;
    cast->dataQueueLock.lock();
    cast->SDLAudioCallback(stream, len);
    cast->dataQueueLock.unlock();
}

void NES::InitSDL()
{
    win = SDL_CreateWindow("NES Emulator", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 256 * 4, header.isPAL ? 240 * 4 : 224 * 4, SDL_WINDOW_SHOWN);
    if (!win)
    {
        std::cerr << "failed to create SDL window: " << SDL_GetError() << "\n";
        exit(9);
    }
    renderer = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer)
    {
        std::cerr << "failed to create SDL renderer: " << SDL_GetError() << "\n";
        exit(10);
    }
    nesTexture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, 256, header.isPAL ? 240 : 224);
    nesPixels = make_unique<uint32_t[]>(256* (header.isPAL ? 240 : 224));
    stretchRect.x=0;
    stretchRect.y=0;
    stretchRect.w=256*4;
    stretchRect.h = (header.isPAL ? 240 : 224)*4;

    SDL_memset(&want, 0, sizeof(want));
    want.freq=48000;
    want.format=AUDIO_S16SYS;
    want.channels=1;
    want.samples=512;
    want.userdata = this;
    want.callback = UpdateAudioBuffer;

    device = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (device == 0)
    {
        std::cerr << "failed to create SDL audio device: " << SDL_GetError() << "\n";
        exit(20);
    }
    SDL_PauseAudioDevice(device, 0);
}

uint16_t NES::GetOperandAddress(AddressMode addressMode)
{
    uint16_t address;
    switch (addressMode)
    {
    case ACCUMULATOR: return 0;
    case ABSOLUTE: return Read16Bit(registers.programCounter, true);
    case ABSOLUTE_X_INDEX: return Read16Bit(registers.programCounter, true) + registers.Xregister;
    case ABSOLUTE_Y_INDEX: return Read16Bit(registers.programCounter, true) + registers.Yregister;
    case IMMEDIATE: return 0;
    case IMPLIED: return 0;
    case INDIRECT: return Read16Bit(registers.programCounter, true);
    case INDIRECT_X_INDEX:
        address = (Read8Bit(registers.programCounter, true) + registers.Xregister) % 256;
        return Read16BitWrapAround(address);
    case INDIRECT_Y_INDEX:
        address = Read8Bit(registers.programCounter, true);
        return Read16BitWrapAround(address) + registers.Yregister;
    case RELATIVE:return Read8Bit(registers.programCounter, true);
    case ZEROPAGE: return Read8Bit(registers.programCounter, true);
    case ZEROPAGE_X_INDEX: return (Read8Bit(registers.programCounter, true) + registers.Xregister) % 256;
    case ZEROPAGE_Y_INDEX: return (Read8Bit(registers.programCounter, true) + registers.Yregister) % 256;
    }
    return -1;
}

std::pair<uint16_t, uint16_t> NES::GetOperandAddressValue(AddressMode addressMode)
{
    uint16_t address;
    switch(addressMode)
    {
    case ACCUMULATOR:
        return pair<uint16_t,uint16_t>(registers.accumulator,0);
    case ABSOLUTE:
        address = Read16Bit(registers.programCounter, true);
        return pair<uint16_t,uint16_t>(Read8Bit(address, false), address);
    case ABSOLUTE_X_INDEX: 
        address = Read16Bit(registers.programCounter, true) + registers.Xregister;
        return pair<uint16_t, uint16_t>(Read8Bit(address, false), address);
    case ABSOLUTE_Y_INDEX:
        address = Read16Bit(registers.programCounter, true) + registers.Yregister;
        return pair<uint16_t, uint16_t>(Read8Bit(address, false), address);
    case IMMEDIATE: return pair<uint16_t,uint16_t>(Read8Bit(registers.programCounter, true),0);
    case IMPLIED: return pair<uint16_t,uint16_t>(0,0);
    case INDIRECT:
        address = Read16Bit(registers.programCounter, true);
        return pair<uint16_t, uint16_t>(Read16BitWrapAround(address), address);
    case INDIRECT_X_INDEX:
        address = (Read8Bit(registers.programCounter, true) + registers.Xregister) % 256;
        address = Read16BitWrapAround(address);
        return pair<uint16_t, uint16_t>(Read8Bit(address, false), address);
    case INDIRECT_Y_INDEX:
        address = Read8Bit(registers.programCounter, true);
        address = Read16BitWrapAround(address) + registers.Yregister;
        return pair<uint16_t, uint16_t>(Read16Bit(address, false), address);
    case RELATIVE:
        return pair<uint16_t, uint16_t>(Read8Bit(registers.programCounter, true),0);
    case ZEROPAGE:
        address = Read8Bit(registers.programCounter, true) & 0xFF;
        return pair<uint16_t, uint16_t>(Read8Bit(address, false), address);
    case ZEROPAGE_X_INDEX:
        address = (Read8Bit(registers.programCounter, true)+registers.Xregister)%256;
        return pair<uint16_t, uint16_t>(Read8Bit(address, false), address);
    case ZEROPAGE_Y_INDEX:
        address = (Read8Bit(registers.programCounter, true) + registers.Yregister) % 256;
        return pair<uint16_t, uint16_t>(Read8Bit(address, false), address);
    }
    return pair<uint16_t,uint16_t>(-1,-1);
}

void NES::SetProcessorStatusFlag(int bitNum, bool set)
{
    uint8_t setBit = 1 << bitNum;
    uint8_t mask = ~setBit;
    if(set)
        registers.processorStatus |= setBit;
    else
        registers.processorStatus &= mask;
}

uint8_t NES::GetProcessorStatusFlag(int bitNum)
{
    return registers.processorStatus & (1 << bitNum);
}

void NES::UpdateZeroAndNegativeFlags(uint16_t result)
{
    SetProcessorStatusFlag(ZERO, (result & 0xFF)==0);
    SetProcessorStatusFlag(NEGATIVE, result & 0b10000000);
}

void NES::break6502()
{
    if(!GetProcessorStatusFlag(INTERRUPT_DISABLE))
    {
        PushStack16Bit(registers.programCounter+1);
        SetProcessorStatusFlag(BREAK, true);
        registers.processorStatus |= 0b100000;
        PushStack8Bit(registers.processorStatus);
        registers.programCounter = Read16Bit(0xFFFE, false);
    }
    PPUcycles+=21;
}

void NES::or6502(uint8_t value)
{
    registers.accumulator |= value;
    UpdateZeroAndNegativeFlags(registers.accumulator);
    PPUcycles+=6;
}

void NES::and6502(uint8_t value)
{
    registers.accumulator &= value;
    UpdateZeroAndNegativeFlags(registers.accumulator);
    PPUcycles += 6;
}

void NES::eor6502(uint8_t value)
{
    registers.accumulator ^= value;
    UpdateZeroAndNegativeFlags(registers.accumulator);
    PPUcycles += 6;
}

void NES::bit6502(uint8_t value)
{
    SetProcessorStatusFlag(ZERO, (registers.accumulator & value) ==0);
    SetProcessorStatusFlag(OVERFLOW, value & 0b1000000);
    SetProcessorStatusFlag(NEGATIVE, value & 0b10000000);
    PPUcycles += 9;
}

void NES::shift6502(bool left, bool rotate, bool acc, std::pair<uint16_t, uint16_t> valueAddress)
{
    bool oldCarry = GetProcessorStatusFlag(CARRY);
    uint8_t value = valueAddress.first;
    SetProcessorStatusFlag(CARRY, (left ? (value & 0b10000000) : (value & 1)));
    value = left ? (value << 1) : (value >> 1);
    if (oldCarry && rotate)
        value |= (left ? 1 : 0b10000000);
    UpdateZeroAndNegativeFlags(value);
    if(acc) registers.accumulator=value;
    else Write8Bit(valueAddress.second, value);
    PPUcycles+=6;
}

void NES::branch6502(StatusFlags flag, bool set, int8_t value)
{
    if(((bool)GetProcessorStatusFlag(flag)) == set)
        registers.programCounter += value;
    PPUcycles+=6;
}

void NES::jsr6502()
{
    PushStack16Bit(registers.programCounter+1);
    registers.programCounter = Read16Bit(registers.programCounter, false);
    PPUcycles+=18;
}

void NES::rti6502()
{
    registers.processorStatus = PullStack8Bit() & 0b11001111;
    registers.programCounter = PullStack16Bit();
    PPUcycles += 18;
}

void NES::add6502(uint8_t value)
{
    uint16_t result = value + registers.accumulator + GetProcessorStatusFlag(CARRY);
    int16_t signedResult = (int8_t)value + (int8_t)registers.accumulator + GetProcessorStatusFlag(CARRY);
    registers.accumulator=result;
    SetProcessorStatusFlag(CARRY, result > 255);
    SetProcessorStatusFlag(OVERFLOW, signedResult > 127 || signedResult < -128);
    UpdateZeroAndNegativeFlags(registers.accumulator);
    PPUcycles+=6;
}

void NES::subtract6502(uint8_t value)
{
    uint8_t notCarry = 1 - GetProcessorStatusFlag(CARRY);
    uint16_t result = registers.accumulator - value - notCarry;
    int16_t signedResult = (int8_t)registers.accumulator - (int8_t)value - notCarry;
    SetProcessorStatusFlag(CARRY, (uint16_t)registers.accumulator >= (uint16_t)value + notCarry);
    SetProcessorStatusFlag(OVERFLOW, signedResult > 127 || signedResult < -128);
    registers.accumulator = result;
    UpdateZeroAndNegativeFlags(registers.accumulator);
    PPUcycles += 6;
}

void NES::store6502(uint16_t address, uint8_t value)
{
    //std::cout << "attempted to write 0x" << std::hex << (uint32_t)value << " to 0x" << address << "\n";
    Write8Bit(address, value);
    PPUcycles += 9;
}

void NES::load6502(uint8_t value, uint8_t& dest)
{
    //std::cout << "loaded 0x" << std::hex << (uint32_t)value << "\n";
    dest=value;
    UpdateZeroAndNegativeFlags(value);
    PPUcycles += 6;
}

void NES::inc6502(uint8_t& value, bool dec)
{
    if(dec) value--;
    else value++;
    UpdateZeroAndNegativeFlags(value);
    PPUcycles+=9;
}

void NES::transfer6502(uint8_t source, uint8_t &destination, bool updateFlags)
{
    destination=source;
    if(updateFlags) UpdateZeroAndNegativeFlags(source);
    PPUcycles += 6;
}

void NES::compare6502(uint8_t value, uint8_t value2)
{
    SetProcessorStatusFlag(CARRY, value >= value2);
    UpdateZeroAndNegativeFlags(value - value2);
    PPUcycles+=6;
}

void NES::incMem6502(std::pair<uint16_t, uint16_t> valueAddress, bool dec)
{
    uint8_t value = valueAddress.first;
    if(dec) value--;
    else value++;
    Write8Bit(valueAddress.second, value);
    UpdateZeroAndNegativeFlags(value);
    PPUcycles+=15;
}

void printOpcode(uint8_t opcode)
{
    switch(opcode)
    {
    case 0x00: cout << "BRK" << '\n'; return;
    case 0x01: cout << "ORA x ind" << '\n'; return;
    case 0x05: cout << "ORA zpg" << '\n'; return;
    case 0x06: cout << "ASL zpg" << '\n'; return;
    case 0x08: cout << "PHP" << '\n'; return;
    case 0x09: cout << "ORA #" << '\n'; return;
    case 0x0A: cout << "ASL A" << '\n'; return;
    case 0x0D: cout << "ORA abs" << '\n'; return;
    case 0x0E: cout << "ASL abs" << '\n'; return;
    case 0x10: cout << "BPL" << '\n'; return;
    case 0x11: cout << "ORA ind y" << '\n'; return;
    case 0x15: cout << "ORA zpg x" << '\n'; return;
    case 0x16: cout << "ASL zpg x" << '\n'; return;
    case 0x18: cout << "CLC" << '\n'; return;
    case 0x19: cout << "ORA abs y" << '\n'; return;
    case 0x1D: cout << "ORA abs x" << '\n'; return;
    case 0x1E: cout << "ASL abs x" << '\n'; return;
    case 0x20: cout << "JSR" << '\n'; return;
    case 0x21: cout << "AND x ind" << '\n'; return;
    case 0x24: cout << "BIT zpg" << '\n'; return;
    case 0x25: cout << "AND zpg" << '\n'; return;
    case 0x26: cout << "ROL zpg" << '\n'; return;
    case 0x28: cout << "PLP" << '\n'; return;
    case 0x29: cout << "AND #" << '\n'; return;
    case 0x2A: cout << "ROL A" << '\n'; return;
    case 0x2C: cout << "BIT abs" << '\n'; return;
    case 0x2D: cout << "AND abs" << '\n'; return;
    case 0x2E: cout << "ROL abs" << '\n'; return;
    case 0x30: cout << "BMI" << '\n'; return;
    case 0x31: cout << "AND ind y" << '\n'; return;
    case 0x35: cout << "AND zpg x" << '\n'; return;
    case 0x36: cout << "ROL zpg x" << '\n'; return;
    case 0x38: cout << "SEC" << '\n'; return;
    case 0x39: cout << "AND abs y" << '\n'; return;
    case 0x3D: cout << "AND abs x" << '\n'; return;
    case 0x3E: cout << "ROL abs x" << '\n'; return;
    case 0x40: cout << "RTI" << '\n'; return;
    case 0x41: cout << "EOR x ind" << '\n'; return;
    case 0x45: cout << "EOR zpg" << '\n'; return;
    case 0x46: cout << "LSR zpg" << '\n'; return;
    case 0x48: cout << "PHA" << '\n'; return;
    case 0x49: cout << "EOR #" << '\n'; return;
    case 0x4A: cout << "LSR A" << '\n'; return;
    case 0x4C: cout << "JMP abs" << '\n'; return;
    case 0x4D: cout << "EOR abs" << '\n'; return;
    case 0x4E: cout << "LSR abs" << '\n'; return;
    case 0x50: cout << "BVC" << '\n'; return;
    case 0x51: cout << "EOR ind y" << '\n'; return;
    case 0x55: cout << "EOR zpg x" << '\n'; return;
    case 0x56: cout << "LSR zpg x" << '\n'; return;
    case 0x58: cout << "CLI" << '\n'; return;
    case 0x59: cout << "EOR abs y" << '\n'; return;
    case 0x5D: cout << "EOR abs x" << '\n'; return;
    case 0x5E: cout << "LSR abs X" << '\n'; return;
    case 0x60: cout << "RTS" << '\n'; return;
    case 0x61: cout << "ADC x ind" << '\n'; return;
    case 0x65: cout << "ADC zpg" << '\n'; return;
    case 0x66: cout << "ROR zpg" << '\n'; return;
    case 0x68: cout << "PLA" << '\n'; return;
    case 0x69: cout << "ADC #" << '\n'; return;
    case 0x6A: cout << "ROR A" << '\n'; return;
    case 0x6C: cout << "JMP ind" << '\n'; return;
    case 0x6D: cout << "ADC abs" << '\n'; return;
    case 0x6E: cout << "ROR abs" << '\n'; return;
    case 0x70: cout << "BVS" << '\n'; return;
    case 0x71: cout << "ADC ind y" << '\n'; return;
    case 0x75: cout << "ADC zpg x" << '\n'; return;
    case 0x76: cout << "ROR zpg x" << '\n'; return;
    case 0x78: cout << "SEI" << '\n'; return;
    case 0x79: cout << "ADC abs y" << '\n'; return;
    case 0x7D: cout << "ADC abs x" << '\n'; return;
    case 0x7E: cout << "ROR abs x" << '\n'; return;
    case 0x81: cout << "STA x ind" << '\n'; return;
    case 0x84: cout << "STY zpg" << '\n'; return;
    case 0x85: cout << "STA zpg" << '\n'; return;
    case 0x86: cout << "STX zpg" << '\n'; return;
    case 0x88: cout << "DEY" << '\n'; return;
    case 0x8A: cout << "TXA" << '\n'; return;
    case 0x8C: cout << "STY abs" << '\n'; return;
    case 0x8D: cout << "STA abs" << '\n'; return;
    case 0x8E: cout << "STX abs" << '\n'; return;
    case 0x90: cout << "BCC" << '\n'; return;
    case 0x91: cout << "STA ind Y" << '\n'; return;
    case 0x94: cout << "STY zpg x" << '\n'; return;
    case 0x95: cout << "STA zpg x" << '\n'; return;
    case 0x96: cout << "STX zpg y" << '\n'; return;
    case 0x98: cout << "TYA" << '\n'; return;
    case 0x99: cout << "STA abs y" << '\n'; return;
    case 0x9A: cout << "TXS" << '\n'; return;
    case 0x9D: cout << "STA abs x" << '\n'; return;
    case 0xA0: cout << "LDY #" << '\n'; return;
    case 0xA1: cout << "LDA x ind" << '\n'; return;
    case 0xA2: cout << "LDX #" << '\n'; return;
    case 0xA4: cout << "LDY zpg" << '\n'; return;
    case 0xA5: cout << "LDA zpg" << '\n'; return;
    case 0xA6: cout << "LDX zpg" << '\n'; return;
    case 0xA8: cout << "TAY" << '\n'; return;
    case 0xA9: cout << "LDA #" << '\n'; return;
    case 0xAA: cout << "TAX" << '\n'; return;
    case 0xAC: cout << "LDY abs" << '\n'; return;
    case 0xAD: cout << "LDA abs" << '\n'; return;
    case 0xAE: cout << "LDX abs" << '\n'; return;
    case 0xB0: cout << "BCS" << '\n'; return;
    case 0xB1: cout << "LDA ind y" << '\n'; return;
    case 0xB4: cout << "LDY zpg x" << '\n'; return;
    case 0xB5: cout << "LDA zpg x" << '\n'; return;
    case 0xB6: cout << "LDX zpg y" << '\n'; return;
    case 0xB8: cout << "CLV" << '\n'; return;
    case 0xB9: cout << "LDA abs y" << '\n'; return;
    case 0xBA: cout << "TSX" << '\n'; return;
    case 0xBC: cout << "LDY abs x" << '\n'; return;
    case 0xBD: cout << "LDA abs x" << '\n'; return;
    case 0xBE: cout << "LDX abs y" << '\n'; return;
    case 0xC0: cout << "CPY #" << '\n'; return;
    case 0xC1: cout << "CMP x ind" << '\n'; return;
    case 0xC4: cout << "CPY zpg" << '\n'; return;
    case 0xC5: cout << "CMP zpg" << '\n'; return;
    case 0xC6: cout << "DEC zpg" << '\n'; return;
    case 0xC8: cout << "INY" << '\n'; return;
    case 0xC9: cout << "CMP #" << '\n'; return;
    case 0xCA: cout << "DEX" << '\n'; return;
    case 0xCC: cout << "CPY abs" << '\n'; return;
    case 0xCD: cout << "CMP abs" << '\n'; return;
    case 0xCE: cout << "DEC abs" << '\n'; return;
    case 0xD0: cout << "BNE" << '\n'; return;
    case 0xD1: cout << "CMP ind y" << '\n'; return;
    case 0xD5: cout << "CMP zpg x" << '\n'; return;
    case 0xD6: cout << "DEC zpg x" << '\n'; return;
    case 0xD8: cout << "CLD" << '\n'; return;
    case 0xD9: cout << "CMP abs y" << '\n'; return;
    case 0xDD: cout << "CMP abs x" << '\n'; return;
    case 0xDE: cout << "DEC abs x" << '\n'; return;
    case 0xE0: cout << "CPX #" << '\n'; return;
    case 0xE1: cout << "SBC x ind" << '\n'; return;
    case 0xE4: cout << "CPX zpg" << '\n'; return;
    case 0xE5: cout << "SBC zpg" << '\n'; return;
    case 0xE6: cout << "INC zpg" << '\n'; return;
    case 0xE8: cout << "INX" << '\n'; return;
    case 0xE9: cout << "SBC #" << '\n'; return;
    case 0xEA: cout << "NOP" << '\n'; return;
    case 0xEC: cout << "CPX abs" << '\n'; return;
    case 0xED: cout << "SBC abs" << '\n'; return;
    case 0xEE: cout << "INC abs" << '\n'; return;
    case 0xF0: cout << "BEQ" << '\n'; return;
    case 0xF1: cout << "SBC ind y" << '\n'; return;
    case 0xF5: cout << "SBC zpg x" << '\n'; return;
    case 0xF6: cout << "INC zpg x" << '\n'; return;
    case 0xF8: cout << "SED" << '\n'; return;
    case 0xF9: cout << "SBC abs y" << '\n'; return;
    case 0xFD: cout << "SBC abs x" << '\n'; return;
    case 0xFE: cout << "INC abs x" << '\n'; return;
    default:
        cerr << "invalid opcode " << std::setfill('0') << std::setw(2) << std::hex << ((int)opcode & 0xFF) << "\n";
    }
}

void NES::ExecuteStep(uint8_t opcode)
{
    //printOpcode(opcode);
    switch(opcode)
    {
    case 0x00: return break6502();
    case 0x01: return or6502(GetOperandAddressValue(INDIRECT_X_INDEX).first);
    case 0x05: return or6502(GetOperandAddressValue(ZEROPAGE).first);
    case 0x06: return shift6502(true, false, false, GetOperandAddressValue(ZEROPAGE));
    case 0x08: PPUcycles+=9; return PushStack8Bit(registers.processorStatus | 0b00110000);
    case 0x09: return or6502(GetOperandAddressValue(IMMEDIATE).first);
    case 0x0A: return shift6502(true, false, true, GetOperandAddressValue(ACCUMULATOR));
    case 0x0D: return or6502(GetOperandAddressValue(ABSOLUTE).first);
    case 0x0E: return shift6502(true, false, false, GetOperandAddressValue(ABSOLUTE));
    case 0x10: return branch6502(NEGATIVE, false, Read8Bit(registers.programCounter, true));
    case 0x11: return or6502(GetOperandAddressValue(INDIRECT_Y_INDEX).first);
    case 0x15: return or6502(GetOperandAddressValue(ZEROPAGE_X_INDEX).first);
    case 0x16: return shift6502(true, false, false, GetOperandAddressValue(ZEROPAGE_X_INDEX));
    case 0x18: PPUcycles+=6; return SetProcessorStatusFlag(CARRY, false); 
    case 0x19: return or6502(GetOperandAddressValue(ABSOLUTE_Y_INDEX).first);
    case 0x1D: return or6502(GetOperandAddressValue(ABSOLUTE_X_INDEX).first);
    case 0x1E: return shift6502(true, false, false, GetOperandAddressValue(ABSOLUTE_X_INDEX));
    case 0x20: return jsr6502();
    case 0x21: return and6502(GetOperandAddressValue(INDIRECT_X_INDEX).first);
    case 0x24: return bit6502(GetOperandAddressValue(ZEROPAGE).first);
    case 0x25: return and6502(GetOperandAddressValue(ZEROPAGE).first);
    case 0x26: return shift6502(true, true, false, GetOperandAddressValue(ZEROPAGE));
    case 0x28: PPUcycles+=12; registers.processorStatus = PullStack8Bit() & 0b11001111; return;
    case 0x29: return and6502(GetOperandAddressValue(IMMEDIATE).first);
    case 0x2A: return shift6502(true, true, true, GetOperandAddressValue(ACCUMULATOR));
    case 0x2C: return bit6502(GetOperandAddressValue(ABSOLUTE).first);
    case 0x2D: return and6502(GetOperandAddressValue(ABSOLUTE).first);
    case 0x2E: return shift6502(true, true, false, GetOperandAddressValue(ABSOLUTE));
    case 0x30: return branch6502(NEGATIVE, true, Read8Bit(registers.programCounter, true));
    case 0x31: return and6502(GetOperandAddressValue(INDIRECT_Y_INDEX).first);
    case 0x35: return and6502(GetOperandAddressValue(ZEROPAGE_X_INDEX).first);
    case 0x36: return shift6502(true, true, false, GetOperandAddressValue(ZEROPAGE_X_INDEX));
    case 0x38: PPUcycles+=6; return SetProcessorStatusFlag(CARRY, true); 
    case 0x39: return and6502(GetOperandAddressValue(ABSOLUTE_Y_INDEX).first);
    case 0x3D: return and6502(GetOperandAddressValue(ABSOLUTE_X_INDEX).first);
    case 0x3E: return shift6502(true, true, false, GetOperandAddressValue(ABSOLUTE_X_INDEX));
    case 0x40: return rti6502();
    case 0x41: return eor6502(GetOperandAddressValue(INDIRECT_X_INDEX).first);
    case 0x45: return eor6502(GetOperandAddressValue(ZEROPAGE).first);
    case 0x46: return shift6502(false, false, false, GetOperandAddressValue(ZEROPAGE));
    case 0x48: PPUcycles+=9; return PushStack8Bit(registers.accumulator); 
    case 0x49: return eor6502(GetOperandAddressValue(IMMEDIATE).first);
    case 0x4A: return shift6502(false, false, true, GetOperandAddressValue(ACCUMULATOR));
    case 0x4C: 
        PPUcycles+=9; 
        //cout << "PC: " << std::setfill('0') << std::setw(4) << std::hex << registers.programCounter << " -> ";
        registers.programCounter = GetOperandAddress(ABSOLUTE);
        //cout << std::setfill('0') << std::setw(4) << std::hex << registers.programCounter << "\n";
        return; 
    case 0x4D: return eor6502(GetOperandAddressValue(ABSOLUTE).first);
    case 0x4E: return shift6502(false, false, false, GetOperandAddressValue(ABSOLUTE));
    case 0x50: return branch6502(OVERFLOW, false, Read8Bit(registers.programCounter, true));
    case 0x51: return eor6502(GetOperandAddressValue(INDIRECT_Y_INDEX).first);
    case 0x55: return eor6502(GetOperandAddressValue(ZEROPAGE_X_INDEX).first);
    case 0x56: return shift6502(false, false, false, GetOperandAddressValue(ZEROPAGE_X_INDEX));
    case 0x58: PPUcycles+=6; return SetProcessorStatusFlag(INTERRUPT_DISABLE, false);  
    case 0x59: return eor6502(GetOperandAddressValue(ABSOLUTE_Y_INDEX).first);
    case 0x5D: return eor6502(GetOperandAddressValue(ABSOLUTE_X_INDEX).first);
    case 0x5E: return shift6502(false, false, false, GetOperandAddressValue(ABSOLUTE_X_INDEX));
    case 0x60: 
        PPUcycles+=18;
        //cout << "PC: " << std::setfill('0') << std::setw(4) << std::hex << registers.programCounter << " -> ";
        registers.programCounter = PullStack16Bit()+1;
        //cout << std::setfill('0') << std::setw(4) << std::hex << registers.programCounter << "\n";
        return; 
    case 0x61: return add6502(GetOperandAddressValue(INDIRECT_X_INDEX).first);
    case 0x65: return add6502(GetOperandAddressValue(ZEROPAGE).first);
    case 0x66: return shift6502(false, true, false, GetOperandAddressValue(ZEROPAGE));
    case 0x68: PPUcycles+=12; registers.accumulator = PullStack8Bit(); UpdateZeroAndNegativeFlags(registers.accumulator); return; 
    case 0x69: return add6502(GetOperandAddressValue(IMMEDIATE).first);
    case 0x6A: return shift6502(false, true, true, GetOperandAddressValue(ACCUMULATOR));
    case 0x6C: PPUcycles+=9; registers.programCounter = GetOperandAddressValue(INDIRECT).first; return;  
    case 0x6D: return add6502(GetOperandAddressValue(ABSOLUTE).first);
    case 0x6E: return shift6502(false, true, false, GetOperandAddressValue(ABSOLUTE));
    case 0x70: return branch6502(OVERFLOW, true, Read8Bit(registers.programCounter, true));
    case 0x71: return add6502(GetOperandAddressValue(INDIRECT_Y_INDEX).first);
    case 0x75: return add6502(GetOperandAddressValue(ZEROPAGE_X_INDEX).first);
    case 0x76: return shift6502(false, true, false, GetOperandAddressValue(ZEROPAGE_X_INDEX));
    case 0x78: PPUcycles+=6; return SetProcessorStatusFlag(INTERRUPT_DISABLE, true);   
    case 0x79: return add6502(GetOperandAddressValue(ABSOLUTE_Y_INDEX).first);
    case 0x7D: return add6502(GetOperandAddressValue(ABSOLUTE_X_INDEX).first);
    case 0x7E: return shift6502(false, true, false, GetOperandAddressValue(ABSOLUTE_X_INDEX));
    case 0x81: return store6502(GetOperandAddress(INDIRECT_X_INDEX), registers.accumulator);
    case 0x84: return store6502(GetOperandAddress(ZEROPAGE), registers.Yregister); 
    case 0x85: return store6502(GetOperandAddress(ZEROPAGE), registers.accumulator); 
    case 0x86: return store6502(GetOperandAddress(ZEROPAGE), registers.Xregister); 
    case 0x88: return inc6502(registers.Yregister, true); 
    case 0x8A: return transfer6502(registers.Xregister, registers.accumulator,true);
    case 0x8C: return store6502(GetOperandAddress(ABSOLUTE), registers.Yregister); 
    case 0x8D: return store6502(GetOperandAddress(ABSOLUTE), registers.accumulator); 
    case 0x8E: return store6502(GetOperandAddress(ABSOLUTE), registers.Xregister); 
    case 0x90: return branch6502(CARRY, false, Read8Bit(registers.programCounter, true));
    case 0x91: return store6502(GetOperandAddress(INDIRECT_Y_INDEX), registers.accumulator);  
    case 0x94: return store6502(GetOperandAddress(ZEROPAGE_X_INDEX), registers.Yregister); 
    case 0x95: return store6502(GetOperandAddress(ZEROPAGE_X_INDEX), registers.accumulator);
    case 0x96: return store6502(GetOperandAddress(ZEROPAGE_Y_INDEX), registers.Xregister); 
    case 0x98: return transfer6502(registers.Yregister, registers.accumulator,true);
    case 0x99: return store6502(GetOperandAddress(ABSOLUTE_Y_INDEX), registers.accumulator); 
    case 0x9A: return transfer6502(registers.Xregister, registers.stackPointer, false);
    case 0x9D: return store6502(GetOperandAddress(ABSOLUTE_X_INDEX), registers.accumulator);  
    case 0xA0: return load6502(GetOperandAddressValue(IMMEDIATE).first, registers.Yregister);
    case 0xA1: return load6502(GetOperandAddressValue(INDIRECT_X_INDEX).first, registers.accumulator);
    case 0xA2: return load6502(GetOperandAddressValue(IMMEDIATE).first, registers.Xregister);
    case 0xA4: return load6502(GetOperandAddressValue(ZEROPAGE).first, registers.Yregister);
    case 0xA5: return load6502(GetOperandAddressValue(ZEROPAGE).first, registers.accumulator);
    case 0xA6: return load6502(GetOperandAddressValue(ZEROPAGE).first, registers.Xregister);
    case 0xA8: return transfer6502(registers.accumulator, registers.Yregister,true);
    case 0xA9: return load6502(GetOperandAddressValue(IMMEDIATE).first, registers.accumulator);
    case 0xAA: return transfer6502(registers.accumulator, registers.Xregister,true);
    case 0xAC: return load6502(GetOperandAddressValue(ABSOLUTE).first, registers.Yregister);
    case 0xAD: return load6502(GetOperandAddressValue(ABSOLUTE).first, registers.accumulator);
    case 0xAE: return load6502(GetOperandAddressValue(ABSOLUTE).first, registers.Xregister);
    case 0xB0: return branch6502(CARRY, true, Read8Bit(registers.programCounter, true));
    case 0xB1: return load6502(GetOperandAddressValue(INDIRECT_Y_INDEX).first, registers.accumulator);
    case 0xB4: return load6502(GetOperandAddressValue(ZEROPAGE_X_INDEX).first, registers.Yregister);
    case 0xB5: return load6502(GetOperandAddressValue(ZEROPAGE_X_INDEX).first, registers.accumulator);
    case 0xB6: return load6502(GetOperandAddressValue(ZEROPAGE_Y_INDEX).first, registers.Xregister);
    case 0xB8: PPUcycles+=6; return SetProcessorStatusFlag(OVERFLOW, false);  
    case 0xB9: return load6502(GetOperandAddressValue(ABSOLUTE_Y_INDEX).first, registers.accumulator);
    case 0xBA: return transfer6502(registers.stackPointer, registers.Xregister,true);
    case 0xBC: return load6502(GetOperandAddressValue(ABSOLUTE_X_INDEX).first, registers.Yregister);
    case 0xBD: return load6502(GetOperandAddressValue(ABSOLUTE_X_INDEX).first, registers.accumulator);
    case 0xBE: return load6502(GetOperandAddressValue(ABSOLUTE_Y_INDEX).first, registers.Xregister);
    case 0xC0: return compare6502(registers.Yregister, GetOperandAddressValue(IMMEDIATE).first);
    case 0xC1: return compare6502(registers.accumulator, GetOperandAddressValue(INDIRECT_X_INDEX).first);
    case 0xC4: return compare6502(registers.Yregister, GetOperandAddressValue(ZEROPAGE).first);
    case 0xC5: return compare6502(registers.accumulator, GetOperandAddressValue(ZEROPAGE).first);
    case 0xC6: return incMem6502(GetOperandAddressValue(ZEROPAGE), true);
    case 0xC8: return inc6502(registers.Yregister, false);
    case 0xC9: return compare6502(registers.accumulator, GetOperandAddressValue(IMMEDIATE).first);
    case 0xCA: return inc6502(registers.Xregister, true);
    case 0xCC: return compare6502(registers.Yregister, GetOperandAddressValue(ABSOLUTE).first);
    case 0xCD: return compare6502(registers.accumulator, GetOperandAddressValue(ABSOLUTE).first);
    case 0xCE: return incMem6502(GetOperandAddressValue(ABSOLUTE), true);
    case 0xD0: return branch6502(ZERO, false, Read8Bit(registers.programCounter, true));
    case 0xD1: return compare6502(registers.accumulator, GetOperandAddressValue(INDIRECT_Y_INDEX).first);
    case 0xD5: return compare6502(registers.accumulator, GetOperandAddressValue(ZEROPAGE_X_INDEX).first);
    case 0xD6: return incMem6502(GetOperandAddressValue(ZEROPAGE_X_INDEX), true);
    case 0xD8: PPUcycles+=6; return SetProcessorStatusFlag(DECIMAL, false);
    case 0xD9: return compare6502(registers.accumulator, GetOperandAddressValue(ABSOLUTE_Y_INDEX).first);
    case 0xDD: return compare6502(registers.accumulator, GetOperandAddressValue(ABSOLUTE_X_INDEX).first);
    case 0xDE: return incMem6502(GetOperandAddressValue(ABSOLUTE_X_INDEX), true);
    case 0xE0: return compare6502(registers.Xregister, GetOperandAddressValue(IMMEDIATE).first);
    case 0xE1: return subtract6502(GetOperandAddressValue(INDIRECT_X_INDEX).first);
    case 0xE4: return compare6502(registers.Xregister, GetOperandAddressValue(ZEROPAGE).first);
    case 0xE5: return subtract6502(GetOperandAddressValue(ZEROPAGE).first);
    case 0xE6: return incMem6502(GetOperandAddressValue(ZEROPAGE), false);
    case 0xE8: return inc6502(registers.Xregister, false);
    case 0xE9: return subtract6502(GetOperandAddressValue(IMMEDIATE).first);
    case 0xEA: PPUcycles+=6; return; 
    case 0xEC: return compare6502(registers.Xregister, GetOperandAddressValue(ABSOLUTE).first);
    case 0xED: return subtract6502(GetOperandAddressValue(ABSOLUTE).first);
    case 0xEE: return incMem6502(GetOperandAddressValue(ABSOLUTE), false);
    case 0xF0: return branch6502(ZERO, true, Read8Bit(registers.programCounter, true));
    case 0xF1: return subtract6502(GetOperandAddressValue(INDIRECT_Y_INDEX).first);
    case 0xF5: return subtract6502(GetOperandAddressValue(ZEROPAGE_X_INDEX).first);
    case 0xF6: return incMem6502(GetOperandAddressValue(ZEROPAGE_X_INDEX), false);
    case 0xF8: PPUcycles+=6; return SetProcessorStatusFlag(DECIMAL, true); 
    case 0xF9: return subtract6502(GetOperandAddressValue(ABSOLUTE_Y_INDEX).first);
    case 0xFD: return subtract6502(GetOperandAddressValue(ABSOLUTE_X_INDEX).first);
    case 0xFE: return incMem6502(GetOperandAddressValue(ABSOLUTE_X_INDEX), false);
    //default: 
    //    cerr << "invalid opcode " << std::setfill('0') << std::setw(2) << std::hex << ((int)opcode & 0xFF) << "\n";
    //    debug=true;
    }
}

void NES::DebugPrint()
{
    cout << "PC: " << std::setfill('0') << std::setw(4) << std::hex << registers.programCounter << "\n";
    cout << "SP: " << std::setfill('0') << std::setw(2) << std::hex << (int)registers.stackPointer << "\n";
    cout << "A: " << std::setfill('0') << std::setw(2) << std::hex << (int)registers.accumulator << "\n";
    cout << "X: " << std::setfill('0') << std::setw(2) << std::hex << (int)registers.Xregister << "\n";
    cout << "Y: " << std::setfill('0') << std::setw(2) << std::hex << (int)registers.Yregister << "\n";
    cout << "PS: " << std::setfill('0') << std::setw(2) << std::hex << (int)registers.processorStatus << "\n\n";
    cout << "PPU\n";
    cout << "nametable: " << (int)PPUstatus.currentNameTable << "\n";
    cout << "inc by 32: " << (PPUstatus.incrementBy32 ? "true" : "false") << "\n";
    cout << "sprite pattern table: " << (PPUstatus.spritePatternTable ? "0x1000" : "0x0000") << "\n";
    cout << "background pattern table: " << (PPUstatus.backgroundPatternTable ? "0x1000" : "0x0000") << "\n";
    cout << "8x16 sprites: " << (PPUstatus.is8x16Sprites ? "true" : "false") << "\n";
    cout << "do NMI: " << (PPUstatus.doNMI ? "true" : "false") << "\n";
    cout << "monochrome mode: " << (PPUstatus.monochromeMode ? "true" : "false") << "\n";
    cout << "left 8 pixels show background: " << (PPUstatus.showLeft8PixelsBackground ? "true" : "false") << "\n";
    cout << "left 8 pixels show sprite: " << (PPUstatus.showLeft8PixelsSprites ? "true" : "false") << "\n";
    cout << "display background: " << (PPUstatus.displayBackground ? "true" : "false") << "\n";
    cout << "display sprites: " << (PPUstatus.displaySprites ? "true" : "false") << "\n";
    cout << "background color intensity: " << (int)PPUstatus.backgroundColorIntensity << "\n";
    cout << "hit sprite 0: " << (PPUstatus.hitSprite0 ? "true" : "false") << "\n";
    cout << "sprite overflow: " << (PPUstatus.spriteOverflow ? "true" : "false") << "\n";
    cout << "vblanking: " << (PPUstatus.VBlanking ? "true" : "false") << "\n";
    cout << "current OAM address: " << std::setfill('0') << std::setw(2) << std::hex << (int)PPUstatus.OAMcurrentAddress << "\n";
    cout << "X scroll: " << std::setfill('0') << std::setw(2) << std::hex << (int)PPUstatus.Xscroll << "\n";
    cout << "Y scroll: " << std::setfill('0') << std::setw(2) << std::hex << (int)PPUstatus.Yscroll << "\n";
    cout << "VRAM address: " << std::setfill('0') << std::setw(4) << std::hex << (int)PPUstatus.VRAMaddress << "\n";
    cout << "first read: " << (PPUstatus.firstRead ? "true" : "false") << "\n";
}

void NES::DebugShowMemory(string page)
{
    uint16_t pageInt;
    stringstream ss;
    ss << std::hex << page;
    ss >> pageInt;
    pageInt <<= 8;
    for(int i=0; i<16; i++)
    {
        for(int j=0; j<16; j++)
        {
            cout << std::setfill('0') << std::setw(2) << std::hex << (int)Read8Bit(pageInt + ((i*16)+j), false) << " ";
        }
        cout << '\n';
    }
}

void NES::Run()
{
    int frameCount=0;
    int64_t skipInstructions=0;
    bool skipFrame=false;
    bool running=true;
    SDL_Event ev;
    
    chrono::time_point prevFrame = chrono::high_resolution_clock::now();
    while(running)
    {
        //TODO handle IRQ from APU and potentially mapper
        uint8_t opcode = Read8Bit(registers.programCounter, true);

        ExecuteStep(opcode);

        if(skipInstructions<=0 && !skipFrame && debug)
        {
            string input = "";
            cin >> input;
            while(input != "c")
            {
                if (input[0] == 'p')
                    DebugPrint();
                else if(input[0] == 'm')
                    DebugShowMemory(input.substr(1));
                else if(input[0]=='s')
                    skipFrame=true;
                else if(input[0]=='t')
                    DebugPrintTables();
                else if (input[0] == 'a')
                    DebugPrintAttribute();
                else if(input[0] == 'o')
                    DebugPrintOAM();
                else if (input[0] >= '0' && input[0] <= '9')
                    skipInstructions = stoll(input);

                input = "";
                cin >> input;
            }
        }
        
        
        if(PPUcycles > PPUcyclesPerLine)
        {
            PPUcycles -= PPUcyclesPerLine;
            if(scanline < numTotalLines - numVBlankLines)
                PPURenderLine();
            
            scanline++;
            APUscanlineTiming+=2;
            UpdateAudio();
            if (scanline == numTotalLines - numVBlankLines)
            {
                if (PPUstatus.doNMI)
                {
                    PushStack16Bit(registers.programCounter);
                    PushStack8Bit(registers.processorStatus);
                    registers.programCounter = Read16Bit(0xFFFA, false);
                }

                PPUstatus.VBlanking = true;
                skipFrame=false;
            }
            else if (scanline >= numTotalLines)
            {
                scanline = header.isPAL ? 0 : 8;
                PPUstatus.VBlanking = false;
                PPUstatus.hitSprite0 = false;
                PPUstatus.spriteOverflow = false;

                SDL_UpdateTexture(nesTexture, NULL, nesPixels.get(), 256 * sizeof(uint32_t));
                SDL_RenderCopy(renderer, nesTexture, NULL, &stretchRect);
                SDL_RenderPresent(renderer);
                
                while (SDL_PollEvent(&ev) != 0)
                {
                    switch (ev.type)
                    {
                    case SDL_QUIT:
                        running = false;
                        break;
                    case SDL_KEYDOWN:
                    case SDL_KEYUP:
                        switch(ev.key.keysym.sym)
                        {
                        case SDLK_w:
                            currentStatePlayer1.up=ev.type == SDL_KEYDOWN;
                        break;
                        case SDLK_s:
                            currentStatePlayer1.down = ev.type == SDL_KEYDOWN;
                            break;
                        case SDLK_a:
                            currentStatePlayer1.left = ev.type == SDL_KEYDOWN;
                            break;
                        case SDLK_d:
                            currentStatePlayer1.right = ev.type == SDL_KEYDOWN;
                            break;
                        case SDLK_SPACE:
                            currentStatePlayer1.A = ev.type == SDL_KEYDOWN;
                            break;
                        case SDLK_LSHIFT:
                            currentStatePlayer1.B = ev.type == SDL_KEYDOWN;
                            break;
                        case SDLK_RSHIFT:
                            currentStatePlayer1.select = ev.type == SDL_KEYDOWN;
                            break;
                        case SDLK_ESCAPE:
                            currentStatePlayer1.start = ev.type == SDL_KEYDOWN;
                            break;
                        }
                    break;
                }
            }
            this_thread::sleep_until(prevFrame + chrono::microseconds(static_cast<int>(msPerFrame*1000)));
            prevFrame = chrono::high_resolution_clock::now();
        }
        }

        skipInstructions--;
        
    }
    mapper->SaveGame();
    SDL_DestroyWindow(win);
}

void NES::SDLAudioCallback(Uint8* stream, int len)
{
    if(audioDataQueue.empty())
        return;

    auto frontPtr = audioDataQueue.front().data();
    memcpy(stream, frontPtr, len);
    audioDataQueue.pop();
    
}

