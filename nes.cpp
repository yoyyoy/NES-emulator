#include "nes.h"

using namespace std;

void NES::ParseHeader(ifstream &romFile)
{
    char magicBytes[4];
    romFile.read(magicBytes, 4);

    if (!(magicBytes[0] == 0x4E && magicBytes[0] == 0x45 && magicBytes[0] == 0x53 && magicBytes[0] == 0x1A))
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

void NES::InitMemory(ifstream &romFile)
{
    if (header.hasTrainer)
        romFile.read(nesMemory + 0x7000, 512);

    for (int i = 0; i < header.PRGROMsize; i++)
    {
        ROMbanks.emplace_back();
        romFile.read(ROMbanks.back(), 0x4000);
    }

    for (int i = 0; i < header.CHRROMsize; i++)
    {
        VROMbanks.emplace_back();
        romFile.read(VROMbanks.back(), 0x2000);
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
        numVBlankLines=70;
        numTotalLines=312;
        msPerFrame=20;
    }
    else
    {
        numVBlankLines=20;
        numTotalLines=262;
        msPerFrame=16.66666;
    }
    registers.programCounter = *(uint16_t *)(&nesMemory[0xFFFC]);
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
        cerr << "Invalid lower nybble of opcode\n";
        exit(6);
    }
}

uint16_t NES::GetOperandValue(AddressMode addressMode, uint16_t &address)
{
    uint16_t address;
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
        PPUcycles+=6;
    break;
    case AND:
        registers.accumulator &= operandValue;
        result = registers.accumulator;
        PPUcycles+=6;
    break;
    case ASL:
        result = operandValue << 1;
        if(addressMode == ACCUMULATOR)
            registers.accumulator=result;
        else
            Write8Bit(address, result);
        PPUcycles+=6;
    break;
    case BCC:
        if(!(registers.processorStatus & 1))
            registers.programCounter += (int8_t) operandValue;
        PPUcycles+=6;
    break;
    case BCS:
        if (registers.processorStatus & 1)
            registers.programCounter += (int8_t)operandValue;
        PPUcycles += 6;
    break;
    case BEQ:
        if (registers.processorStatus & 0b00000010)
            registers.programCounter += (int8_t)operandValue;
        PPUcycles += 6;
    break;
    case BIT:
        result = registers.accumulator & operandValue;
        PPUcycles+=9;
    break;
    case BMI:
        if (registers.processorStatus & 0b10000000)
            registers.programCounter += (int8_t)operandValue;
        PPUcycles += 6;
    break;
    case BNE:
        if (!(registers.processorStatus & 0b00000010))
            registers.programCounter += (int8_t)operandValue;
        PPUcycles += 6;
    break;
    case BPL:
        if (!(registers.processorStatus & 0b10000000))
            registers.programCounter += (int8_t)operandValue;
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
            registers.programCounter += (int8_t)operandValue;
        PPUcycles += 6;
    break;
    case BVS:
        if (registers.processorStatus & 0b01000000)
            registers.programCounter += (int8_t)operandValue;
        PPUcycles += 6;
    break;
    case CMP:
        result = registers.accumulator - operandValue;
        PPUcycles += 6;
    break;
    case CPX:
        result = registers.Xregister - operandValue;
        PPUcycles += 6;
    break;
    case CPY:
        result = registers.Yregister - operandValue;
        PPUcycles += 6;
    break;
    case DEC:
        result = (operandValue-1);
        Write8Bit(address, result);
        PPUcycles+=15;
    break;
    case DEX:
        result = --registers.Xregister;
        PPUcycles+=6;
    break;
    case DEY:
        result = --registers.Yregister;
        PPUcycles+=6;
    break;
    case EOR:
        registers.accumulator ^= operandValue;
        result = registers.accumulator;
        PPUcycles+=6;
    break;
    case INC:
        result = (operandValue + 1);
        Write8Bit(address, result);
        PPUcycles += 15;
        break;
    case INX:
        result = ++registers.Xregister;
        PPUcycles += 6;
        break;
    case INY:
        result = ++registers.Yregister;
        PPUcycles += 6;
        break;
    case JMP:
        registers.programCounter = operandValue;
        PPUcycles+=9;
    break;
    case JSR:
        PushStack16Bit(registers.programCounter);
        registers.programCounter = operandValue;
        PPUcycles +=18;
    break;
    case LDA:
        result = operandValue;
        registers.accumulator = operandValue;
        PPUcycles+=6;
    break;
    case LDX:
        result = operandValue;
        registers.Xregister = operandValue;
        PPUcycles += 6;
        break;
    case LDY:
        result = operandValue;
        registers.Yregister = operandValue;
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
    while(true)
    {
        uint8_t opcode = Read8Bit(registers.programCounter, true);
        Instruction instruction=Instruction::INVALID;
        AddressMode addressMode=AddressMode::INVALID;
        ParseOpcode(opcode, instruction, addressMode);

        if(instruction==Instruction::INVALID || addressMode == AddressMode::INVALID)
        {
            cerr << "Invalid opcode " << (int)opcode <<"\n";
            exit(7);
        }

        Execute(instruction, addressMode);
        if(PPUcycles > PPUcyclesPerLine)
        {
            PPUcycles -= PPUcyclesPerLine;
            scanline++;
            PPURenderLine();
        }

        if (scanline == numTotalLines - numVBlankLines )
        {
            if (PPUstatus.doNMI)
            {
                PushStack16Bit(registers.programCounter);
                PushStack8Bit(registers.processorStatus);
                registers.programCounter = Read16Bit(0xFFFA, false);
            }
            
            PPUstatus.VBlanking=true;
        }
        else if(scanline>=numTotalLines)
        {
            scanline=0;
            PPUstatus.VBlanking=false;
            PPUstatus.hitSprite0=false;
            PPUstatus.spriteOverflow=false;
            //TODO show frame then sleep until we hit msPerFrame
        }
    }
}