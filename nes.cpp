#include "nes.h"
#include <chrono>
#include <thread>
#include <iomanip>
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

    //it's possible for nametable mirroring to change on the fly, so only use the header value to initialize PPUstatus
    //then change it as needed. do not use header.isHorizontalArrangement or header.is4ScreenVRAM
    PPUstatus.nameTableMirror = header.is4ScreenVRAM ? FOUR_SCREEN : header.isHorizontalArrangement ? VERTICAL : HORIZONTAL;
}

void NES::InitMemory(ifstream &romFile)
{
    if (header.hasTrainer)
        romFile.read(nesMemory + 0x7000, 512);

    for (int i = 0; i < header.PRGROMsize; i++)
    {
        ROMbanks.emplace_back();
        romFile.read(ROMbanks.back().data(), 0x4000);
    }

    for (int i = 0; i < header.CHRROMsize; i++)
    {
        VROMbanks.emplace_back();
        romFile.read(VROMbanks.back().data(), 0x2000);
    }

    if (romFile.fail())
    {
        cerr << "Error while reading file\n";
        exit(4);
    }

    // TODO add support for mappers for ROMbanks size > 2
    if (ROMbanks.size() == 1)
    {
        activeROMBank1=0;
        activeROMBank2=0;
    }
    else if (ROMbanks.size() == 2)
    {
        activeROMBank1 = 0;
        activeROMBank2 = 1;
    }
    else
    {
        cerr << "mappers not supported yet\n";
        exit(5);
    }

    if(header.isPAL)
    {
        numVBlankLines=72;
        numTotalLines=312;
        msPerFrame=20;
    }
    else
    {
        numVBlankLines=30;
        numTotalLines=262;
        msPerFrame=16.66666;
    }
    registers.programCounter = Read16Bit(0xFFFC, false);
}

void NES::InitSDL()
{
    win = SDL_CreateWindow("NES Emulator", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 256 * 4, header.isPAL ? 240 * 4 : 224 * 4, SDL_WINDOW_SHOWN);
    if (!win)
    {
        std::cerr << "failed to create SDL window: " << SDL_GetError() << "\n";
        exit(9);
    }

    windowSurface = SDL_GetWindowSurface(win);
    nesSurface = SDL_CreateRGBSurface(0, 256, header.isPAL ? 240 : 224, 32, 0xFF000000, 0x00FF0000, 0x0000FF00, 0x000000FF);

    if (!nesSurface)
    {
        std::cerr << "failed to create SDL surface: " << SDL_GetError() << "\n";
        exit(10);
    }

    stretchRect.x=0;
    stretchRect.y=0;
    stretchRect.w=256*4;
    stretchRect.h = (header.isPAL ? 240 : 224)*4;
}

NES::Instruction NES::Low159D(uint8_t upper)
{
    switch (upper >> 1)
    {
    case 0: return Instruction::ORA;
    case 1: return Instruction::AND;
    case 2: return Instruction::EOR;
    case 3: return Instruction::ADC;
    case 4: return Instruction::STA;
    case 5: return Instruction::LDA;
    case 6: return Instruction::CMP;
    case 7: return Instruction::SBC;
    }
    return INVALID_INSTRUCTION;
}

NES::Instruction NES::Low6E(uint8_t upper)
{
    switch (upper >> 1)
    {
    case 0: return Instruction::ASL;
    case 1: return Instruction::ROL;
    case 2: return Instruction::LSR;
    case 3: return Instruction::ROR;
    case 4: return Instruction::STX;
    case 5: return Instruction::LDX;
    case 6: return Instruction::DEC;
    case 7: return Instruction::INC;
    }
    return INVALID_INSTRUCTION;
}

void NES::ParseOpcode(uint8_t opcode, Instruction& instruction, AddressMode& addressMode)
{
    uint8_t upper = (opcode >> 4) & 0xF;
    uint8_t lower = opcode & 0xF;
    switch (lower)
    {
    case 0x0:

        if (upper & 0x1)
            addressMode = AddressMode::RELATIVE;
        else if (upper == 0x2)
            addressMode = AddressMode::J_ABSOLUTE;
        else if (upper < 8)
            addressMode = AddressMode::IMPLIED;
        else if (upper > 8)
            addressMode = AddressMode::IMMEDIATE;
        switch (upper)
        {
        case 0:
            instruction = Instruction::BRK;
            break;
        case 1:
            instruction = Instruction::BPL;
            break;
        case 2:
            instruction = Instruction::JSR;
            break;
        case 3:
            instruction = Instruction::BMI;
            break;
        case 4:
            instruction = Instruction::RTI;
            break;
        case 5:
            instruction = Instruction::BVC;
            break;
        case 6:
            instruction = Instruction::RTS;
            break;
        case 7:
            instruction = Instruction::BVS;
            break;
        case 9:
            instruction = Instruction::BCC;
            break;
        case 0xA:
            instruction = Instruction::LDY;
            break;
        case 0xB:
            instruction = Instruction::BCS;
            break;
        case 0xC:
            instruction = Instruction::CPY;
            break;
        case 0xD:
            instruction = Instruction::BNE;
            break;
        case 0xE:
            instruction = Instruction::CPX;
            break;
        case 0xF:
            instruction = Instruction::BEQ;
            break;
        }
        break;

    case 0x1:

        if (upper & 1)
            addressMode = AddressMode::INDIRECT_Y_INDEX;
        else
            addressMode = AddressMode::INDIRECT_X_INDEX;
        instruction = Low159D(upper);
        break;

    case 0x2:

        if (upper == 0xA)
        {
            instruction = Instruction::LDX;
            addressMode = AddressMode::IMMEDIATE;
        }

        break;

    case 0x4:

        switch (upper)
        {
        case 0x2:
            instruction = Instruction::BIT;
            addressMode = AddressMode::ZEROPAGE;
            break;
        case 0x8:
            instruction = Instruction::STY;
            addressMode = AddressMode::ZEROPAGE;
            break;
        case 0x9:
            instruction = Instruction::STY;
            addressMode = AddressMode::ZEROPAGE_X_INDEX;
            break;
        case 0xA:
            instruction = Instruction::LDY;
            addressMode = AddressMode::ZEROPAGE;
            break;
        case 0xB:
            instruction = Instruction::LDY;
            addressMode = AddressMode::ZEROPAGE_X_INDEX;
            break;
        case 0xC:
            instruction = Instruction::CPY;
            addressMode = AddressMode::ZEROPAGE;
            break;
        case 0xE:
            instruction = Instruction::CPX;
            addressMode = AddressMode::ZEROPAGE;
            break;
        }

        break;

    case 0x5:
        if (upper & 1)
            addressMode = AddressMode::ZEROPAGE_X_INDEX;
        else
            addressMode = AddressMode::ZEROPAGE;

        instruction = Low159D(upper);

        break;

    case 0x6:

        if (upper & 1)
        {
            if (upper == 9 || upper == 0xB)
                addressMode = AddressMode::ZEROPAGE_Y_INDEX;
            else
                addressMode = AddressMode::ZEROPAGE_X_INDEX;
        }
        else
            addressMode = AddressMode::ZEROPAGE;

        instruction = Low6E(upper);

        break;

    case 0x8:
        addressMode = AddressMode::IMPLIED;
        switch (upper)
        {
        case 0:
            instruction = Instruction::PHP;
            break;
        case 1:
            instruction = Instruction::CLC;
            break;
        case 2:
            instruction = Instruction::PLP;
            break;
        case 3:
            instruction = Instruction::SEC;
            break;
        case 4:
            instruction = Instruction::PHA;
            break;
        case 5:
            instruction = Instruction::CLI;
            break;
        case 6:
            instruction = Instruction::PLA;
            break;
        case 7:
            instruction = Instruction::SEI;
            break;
        case 8:
            instruction = Instruction::DEY;
            break;
        case 9:
            instruction = Instruction::TYA;
            break;
        case 0xA:
            instruction = Instruction::TAY;
            break;
        case 0xB:
            instruction = Instruction::CLV;
            break;
        case 0xC:
            instruction = Instruction::INY;
            break;
        case 0xD:
            instruction = Instruction::CLD;
            break;
        case 0xE:
            instruction = Instruction::INX;
            break;
        case 0xF:
            instruction = Instruction::SED;
            break;
        }
        break;

    case 0x9:
        if (upper & 1)
            addressMode = AddressMode::ABSOLUTE_Y_INDEX;
        else
        {
            if (upper != 8)
                addressMode = AddressMode::IMMEDIATE;
        }
        instruction = Low159D(upper);
        break;

    case 0xA:
        if (upper > 7)
            addressMode = AddressMode::IMPLIED;
        else
            addressMode = AddressMode::ACCUMULATOR;
        switch (upper)
        {
        case 0:
            instruction = Instruction::ASL;
            break;
        case 2:
            instruction = Instruction::ROL;
            break;
        case 4:
            instruction = Instruction::LSR;
            break;
        case 6:
            instruction = Instruction::ROR;
            break;
        case 8:
            instruction = Instruction::TXA;
            break;
        case 9:
            instruction = Instruction::TXS;
            break;
        case 0xA:
            instruction = Instruction::TAX;
            break;
        case 0xB:
            instruction = Instruction::TSX;
            break;
        case 0xC:
            instruction = Instruction::DEX;
            break;
        case 0xE:
            instruction = Instruction::NOP;
            break;
        }
        break;

    case 0xC:
        switch (upper)
        {
        case 0x2:
            instruction = Instruction::BIT;
            addressMode = AddressMode::ABSOLUTE;
            break;
        case 0x4:
            instruction = Instruction::JMP;
            addressMode = AddressMode::J_ABSOLUTE;
            break;
        case 0x6:
            instruction = Instruction::JMP;
            addressMode = AddressMode::INDIRECT;
            break;
        case 0x8:
            instruction = Instruction::STY;
            addressMode = AddressMode::ABSOLUTE;
            break;
        case 0xA:
            instruction = Instruction::LDY;
            addressMode = AddressMode::ABSOLUTE;
            break;
        case 0xB:
            instruction = Instruction::LDY;
            addressMode = AddressMode::ABSOLUTE_X_INDEX;
            break;
        case 0xC:
            instruction = Instruction::CPY;
            addressMode = AddressMode::ABSOLUTE;
            break;
        case 0xE:
            instruction = Instruction::CPX;
            addressMode = AddressMode::ABSOLUTE;
            break;
        }

        break;

    case 0xD:
        if (upper & 1)
            addressMode = AddressMode::ABSOLUTE_X_INDEX;
        else
            addressMode = AddressMode::ABSOLUTE;
        instruction = Low159D(upper);
        break;

    case 0xE:
        if (upper & 1)
        {
            if (upper == 0xB)
                addressMode = AddressMode::ABSOLUTE_Y_INDEX;
            else if (upper != 0x9)
                addressMode = AddressMode::ABSOLUTE_X_INDEX;
        }
        else
            addressMode = AddressMode::ABSOLUTE;

        instruction = Low6E(upper);
        break;

    default:
        cerr << "Invalid lower nybble of opcode " << std::hex << opcode << "\n";
        exit(6);
    }
}

uint16_t NES::GetOperandValue(AddressMode addressMode, uint16_t &address)
{
    switch(addressMode)
    {
    case ACCUMULATOR: return registers.accumulator;
    case J_ABSOLUTE: return Read16Bit(registers.programCounter, true);
    case ABSOLUTE:
        address = Read16Bit(registers.programCounter, true);
        return Read8Bit(address, false);
    case ABSOLUTE_X_INDEX: 
        address = Read16Bit(registers.programCounter, true) + registers.Xregister;
        return Read8Bit(address, false);
    case ABSOLUTE_Y_INDEX:
        address = Read16Bit(registers.programCounter, true) + registers.Yregister;
        return Read8Bit(address, false);
    case IMMEDIATE: return Read8Bit(registers.programCounter, true);
    case IMPLIED: return 0;
    case INDIRECT:
        address = Read16Bit(registers.programCounter, true);
        return Read16Bit(address, false);
    case INDIRECT_X_INDEX:
        address = (Read8Bit(registers.programCounter, true) + registers.Xregister) % 256;
        address = Read16Bit(address, false);
        return Read8Bit(address,false);
    case INDIRECT_Y_INDEX:
        address = Read8Bit(registers.programCounter, true);
        address = Read16Bit(address, false) + registers.Yregister;
        return Read8Bit(address, false);
    case RELATIVE: return Read8Bit(registers.programCounter, true);
    case ZEROPAGE:
        address = Read8Bit(registers.programCounter, true);
        return Read8Bit(address, false);
    case ZEROPAGE_X_INDEX:
        address = (Read8Bit(registers.programCounter, true)+registers.Xregister)%256;
        return Read8Bit(address, false);
    case ZEROPAGE_Y_INDEX:
        address = (Read8Bit(registers.programCounter, true) + registers.Yregister) % 256;
        return Read8Bit(address, false);
    }
    return -1;
}

void NES::SetProcessorStatusBit(int bitNum, bool set)
{
    uint8_t setBit = 1 << bitNum;
    uint8_t mask = ~setBit;
    if(set)
        registers.processorStatus |= setBit;
    else
        registers.processorStatus &= mask;
}



void NES::UpdateProcessorStatus(uint16_t result, int32_t signedResult, Instruction instruction)
{
    switch(instruction)
    {
    case ADC:
        SetProcessorStatusBit(OVERFLOW, signedResult > 127 || signedResult < -128);
    case ASL:
    case ROL:
        SetProcessorStatusBit(CARRY, result > 255);
    case AND:
    case DEC:
    case DEX:
    case DEY:
    case EOR:
    case INC:
    case INX:
    case INY:
    case LDA:
    case LDX:
    case LDY:
    case LSR:
    case ORA:
    case PLA:
    case ROR:
    case TAX:
    case TAY:
    case TSX:
    case TXA:
    case TYA:
        SetProcessorStatusBit(ZERO, (result & 0xFF)==0);
        SetProcessorStatusBit(NEGATIVE, result & 0b10000000);
    break;
    case CMP:
    case CPX:
    case CPY:
        SetProcessorStatusBit(CARRY, (int16_t)result > 0);
        SetProcessorStatusBit(ZERO, (result & 0xFF) ==0);
        SetProcessorStatusBit(NEGATIVE, result & 0b10000000);
    break;
    case BIT:
        SetProcessorStatusBit(NEGATIVE, result & 0b10000000);
        SetProcessorStatusBit(OVERFLOW, result & 0b01000000);
        SetProcessorStatusBit(ZERO, (result & registers.accumulator)==0);
    break;
    case BRK:
        //break flag is weird and doesn't really exist? TODO read more documentation on this
        break;
    case CLC:
        SetProcessorStatusBit(CARRY, false);
    break;
    case CLD:
        SetProcessorStatusBit(DECIMAL, false);
    break;
    case CLI:
        SetProcessorStatusBit(INTERRUPT_DISABLE, false);
    break;
    case CLV:
        SetProcessorStatusBit(OVERFLOW, false);
    break;
    case SEC:
        SetProcessorStatusBit(CARRY, true);
    break;
    case SED:
        SetProcessorStatusBit(DECIMAL, true);
    break;
    case SEI:
        SetProcessorStatusBit(INTERRUPT_DISABLE, true);
    break;
    }
}

void NES::Execute(Instruction instruction, AddressMode addressMode)
{
    uint16_t address;
    uint16_t operandValue = GetOperandValue(addressMode, address);
    uint32_t result;
    int32_t signedResult;
    
    switch(instruction)
    {//TODO potential improvement. add to PPUcycles more accurately, taking into account address mode and whatever
    case ADC:
        result = (registers.processorStatus & 1) + registers.accumulator + operandValue;
        signedResult = (int8_t)registers.accumulator + (int8_t)operandValue + (registers.processorStatus & 1);
        registers.accumulator=result;
        //cout << "A=" << (int)registers.accumulator << '\n';
        PPUcycles+=6;
    break;
    case AND:
        registers.accumulator &= operandValue;
        result = registers.accumulator;
        //cout << "A=" << (int)registers.accumulator << '\n';
        PPUcycles+=6;
    break;
    case ASL:
        result = operandValue << 1;
        if(addressMode == ACCUMULATOR)
        {
            registers.accumulator=result;
            //cout << "A=" << (int)registers.accumulator << '\n';
        }
        else
        {
            Write8Bit(address, result);
            //cout << std::hex << address << "=" <<result << '\n';
        }
        PPUcycles+=6;
    break;
    case BCC:
        if(!(registers.processorStatus & 1))
        {
            registers.programCounter += (int8_t) operandValue;
            //cout << "PC=" << std::hex << registers.programCounter << "\n";
        }
        PPUcycles+=6;
    break;
    case BCS:
        if (registers.processorStatus & 1)
        {
            registers.programCounter += (int8_t)operandValue;
            //cout << "PC=" << std::hex << registers.programCounter << "\n";
        }
        PPUcycles += 6;
    break;
    case BEQ:
        if (registers.processorStatus & 0b00000010)
        {
            registers.programCounter += (int8_t)operandValue;
            //cout << "PC=" << std::hex << registers.programCounter << "\n";
        }
        PPUcycles += 6;
    break;
    case BIT:
        result = registers.accumulator & operandValue;
        PPUcycles+=9;
    break;
    case BMI:
        if (registers.processorStatus & 0b10000000)
        {
            registers.programCounter += (int8_t)operandValue;
            //cout << "PC=" << std::hex << registers.programCounter << "\n";
        }
        PPUcycles += 6;
    break;
    case BNE:
        if (!(registers.processorStatus & 0b00000010))
        {
            registers.programCounter += (int8_t)operandValue;
            //cout << "PC=" << std::hex << registers.programCounter << "\n";
        }
        PPUcycles += 6;
    break;
    case BPL:
        if (!(registers.processorStatus & 0b10000000))
        {
            registers.programCounter += (int8_t)operandValue;
            //cout << "PC=" << std::hex << registers.programCounter << "\n";
        }
        PPUcycles += 6;
    break;
    case BRK:
        if(!(registers.processorStatus & 0b00000100))
        {
            PushStack16Bit(registers.programCounter);
            PushStack8Bit(registers.processorStatus);
            registers.programCounter = Read16Bit(0xFFFE, false);
        }

        PPUcycles+=21;
    break;
    case BVC:
        if (!(registers.processorStatus & 0b01000000))
        {
            registers.programCounter += (int8_t)operandValue;
            //cout << "PC=" << std::hex << registers.programCounter << "\n";
        }
        PPUcycles += 6;
    break;
    case BVS:
        if (registers.processorStatus & 0b01000000)
        {
            registers.programCounter += (int8_t)operandValue;
            //cout << "PC=" << std::hex << registers.programCounter << "\n";
        }
        PPUcycles += 6;
    break;
    case CMP:
        result = registers.accumulator - operandValue;
        //cout << "compare result=" << (int)result << "\n";
        PPUcycles += 6;
    break;
    case CPX:
        result = registers.Xregister - operandValue;
        //cout << "compare result=" << result << "\n";
        PPUcycles += 6;
    break;
    case CPY:
        result = registers.Yregister - operandValue;
        //cout << "compare result=" << result << "\n";
        PPUcycles += 6;
    break;
    case DEC:
        result = (operandValue-1);
        Write8Bit(address, result);
        //cout << std::hex << address << "=" << result << '\n';
        PPUcycles+=15;
    break;
    case DEX:
        result = --registers.Xregister;
        //cout << "X=" << (int)registers.Xregister << '\n';
        PPUcycles+=6;
    break;
    case DEY:
        result = --registers.Yregister;
        //cout << "Y=" << (int)registers.Yregister << '\n';
        PPUcycles+=6;
    break;
    case EOR:
        registers.accumulator ^= operandValue;
        result = registers.accumulator;
        //cout << "A=" << (int)registers.accumulator << '\n';
        PPUcycles+=6;
    break;
    case INC:
        result = (operandValue + 1);
        Write8Bit(address, result);
        //cout << std::hex << address << "=" << result << '\n';
        PPUcycles += 15;
        break;
    case INX:
        result = ++registers.Xregister;
        //cout << "X=" << (int)registers.Xregister << '\n';
        PPUcycles += 6;
        break;
    case INY:
        result = ++registers.Yregister;
        //cout << "Y=" << (int)registers.Yregister << '\n';
        PPUcycles += 6;
        break;
    case JMP:
        registers.programCounter = operandValue;
        //cout << "PC=" << std::hex << registers.programCounter << "\n";
        PPUcycles+=9;
    break;
    case JSR:
        PushStack16Bit(registers.programCounter);
        registers.programCounter = operandValue;
        //cout << "PC=" << std::hex << registers.programCounter << "\n";
        PPUcycles +=18;
    break;
    case LDA:
        result = operandValue;
        registers.accumulator = operandValue;
        //cout << "A=" << (int)registers.accumulator << '\n';
        PPUcycles+=6;
    break;
    case LDX:
        result = operandValue;
        registers.Xregister = operandValue;
        //cout << "X=" << (int)registers.Xregister << '\n';
        PPUcycles += 6;
        break;
    case LDY:
        result = operandValue;
        registers.Yregister = operandValue;
        //cout << "Y=" << (int)registers.Yregister << '\n';
        PPUcycles += 6;
        break;
    case LSR: 
        //some information is lost before we make it to UpdateProcessorStatus. have to do it here
        SetProcessorStatusBit(CARRY, operandValue&1);

        result = operandValue >> 1;
        if (addressMode == ACCUMULATOR)
            registers.accumulator = result;
        else
            Write8Bit(address, result);
        PPUcycles += 6;
    break;
    case ORA:
        registers.accumulator |= operandValue;
        result = registers.accumulator;
        //cout << "A=" << (int)registers.accumulator << '\n';
        PPUcycles += 6;
        break;
    case PHA:
        PushStack8Bit(registers.accumulator);
        PPUcycles+=9;
    break;
    case PHP:
        PushStack8Bit(registers.processorStatus);
        PPUcycles+=9;
    break;
    case PLA:
        registers.accumulator = PullStack8Bit();
        result = registers.accumulator;
        //cout << "A=" << (int)registers.accumulator << '\n';
        PPUcycles+=12;
    break;
    case PLP:
        registers.processorStatus = PullStack8Bit();
        PPUcycles+=12;
    break;
    case ROL:
        result = (operandValue << 1) | (registers.processorStatus & 1);
        if (addressMode == ACCUMULATOR)
            registers.accumulator = result;
        else
            Write8Bit(address, result);
        PPUcycles += 6;
    break;
    case ROR:
        //some information is lost before we make it to UpdateProcessorStatus. have to do it here
        
        result = (operandValue >> 1) | ((registers.processorStatus & 1) << 7);
        if (addressMode == ACCUMULATOR)
            registers.accumulator = result;
        else
            Write8Bit(address, result);

        SetProcessorStatusBit(CARRY, operandValue & 1);

        PPUcycles += 6;
    break;
    case RTI:
        registers.processorStatus = PullStack8Bit();
        registers.programCounter = PullStack16Bit();
        PPUcycles+=18;
    break;
    case RTS:
        registers.programCounter = PullStack16Bit();
        PPUcycles += 18;
    break;
    case SBC:
        result = registers.accumulator - operandValue - (1-(registers.processorStatus & 1));
        registers.accumulator = result;
        signedResult = (int8_t)registers.accumulator - (int8_t)operandValue - (1-(registers.processorStatus & 1));
        //looks like a pain to do update in UpdateProcessorStatus so just do it here
        SetProcessorStatusBit(CARRY, operandValue + (1 - (registers.processorStatus & 1)) <= registers.accumulator);
        SetProcessorStatusBit(ZERO, registers.accumulator==0);
        SetProcessorStatusBit(OVERFLOW, signedResult > 127 || signedResult < -128);
        SetProcessorStatusBit(NEGATIVE, registers.accumulator & 0b10000000);
        PPUcycles += 6;
        //cout << "A=" << (int)registers.accumulator << '\n';
        break;
    case STA:
        Write8Bit(address, registers.accumulator);
        PPUcycles+=9;
    break;
    case STX:
        Write8Bit(address, registers.Xregister);
        PPUcycles += 9;
    break;
    case STY:
        Write8Bit(address, registers.Yregister);
        PPUcycles += 9;
    break;
    case TAX:
        result= registers.accumulator;
        registers.Xregister=registers.accumulator;
        PPUcycles+=6;
    break;
    case TAY:
        result = registers.accumulator;
        registers.Yregister = registers.accumulator;
        PPUcycles += 6;
    break;
    case TSX:
        result = registers.stackPointer;
        registers.Xregister = registers.stackPointer;
        PPUcycles += 6;
    break;
    case TXA:
        result = registers.Xregister;
        registers.accumulator = registers.Xregister;
        PPUcycles += 6;
    break;
    case TXS:
        registers.stackPointer = registers.Xregister;
        PPUcycles += 6;
    break;
    case TYA:
        result = registers.Yregister;
        registers.accumulator = registers.Yregister;
        PPUcycles += 6;
    break;
    case CLC:
    case CLD:
    case CLI:
    case CLV:
    case NOP:
    case SEC:
    case SED:
    case SEI:
        PPUcycles+=6;
    }
    UpdateProcessorStatus(result, signedResult, instruction);
}

void NES::Run()
{
    int frameCount=0;
    while(true)
    {
        uint8_t opcode = Read8Bit(registers.programCounter, true);
        Instruction instruction=Instruction::INVALID_INSTRUCTION;
        AddressMode addressMode=AddressMode::INVALID_ADDRESS_MODE;
        ParseOpcode(opcode, instruction, addressMode);
        //cout << debugInstructionToString[instruction] << " " << debugAddressModeToString[addressMode] << '\n';
        if(instruction==Instruction::INVALID_INSTRUCTION || addressMode == AddressMode::INVALID_ADDRESS_MODE)
        {
            cerr << "Invalid opcode " << std::hex << (int)opcode <<"\n";
            exit(7);
        }

        Execute(instruction, addressMode);
        if(PPUcycles > PPUcyclesPerLine)
        {
            PPUcycles -= PPUcyclesPerLine;
            if(scanline < numTotalLines - numVBlankLines)
                PPURenderLine();
            scanline++;
            if (scanline == numTotalLines - numVBlankLines)
            {
                if (PPUstatus.doNMI)
                {
                    PushStack16Bit(registers.programCounter);
                    PushStack8Bit(registers.processorStatus);
                    registers.programCounter = Read16Bit(0xFFFA, false);
                }

                PPUstatus.VBlanking = true;
            }
            else if (scanline >= numTotalLines)
            {
                scanline = header.isPAL ? 0 : 8;
                PPUstatus.VBlanking = false;
                PPUstatus.hitSprite0 = false;
                PPUstatus.spriteOverflow = false;
                // TODO show frame then sleep until we hit msPerFrame
                SDL_BlitScaled(nesSurface, NULL, windowSurface, &stretchRect);
                SDL_UpdateWindowSurface(win);
                
                
                frameCount++;
                std::cout << "\n";
                std::cout << "\n";

                std::cout << "\n";
                if(frameCount>3)
                {
                    for(int i=0; i<30; i++)
                    {
                        for(int j=0; j<32;j++)
                        {
                            //cout << setfill('0') << setw(2) << std::hex << ((int)PPUmemory[0x2000 + j + i * 32] & 0xFF) << " ";
                        }
                        //cout << "\n";
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(5000));
                    exit(0);
                }
                //std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            }
        }


    }
    SDL_DestroyWindow(win);
}