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
        memcpy(nesMemory + 0x8000, ROMbanks[0], 0x4000);
        memcpy(nesMemory + 0xC000, ROMbanks[0], 0x4000);
    }
    else if (ROMbanks.size() == 2)
    {
        memcpy(nesMemory + 0x8000, ROMbanks[0], 0x4000);
        memcpy(nesMemory + 0xC000, ROMbanks[1], 0x4000);
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

uint16_t NES::GetOperandValue(AddressMode addressMode)
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
        address = Read8Bit(registers.programCounter, true);
        address = Read16Bit((address+registers.Xregister)%256, false);
        return Read8Bit(address,false);
    case INDIRECT_Y_INDEX:
        address = Read8Bit(registers.programCounter, true);
        address = Read16Bit(address, false);
        return Read8Bit(address, false) + registers.Yregister;
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

void NES::Execute(Instruction instruction, AddressMode addressMode)
{
    uint16_t operandValue = GetOperandValue(addressMode);
    uint32_t result;
    switch(instruction)
    {
    case ADC:
        result = (registers.processorStatus & 1) + registers.accumulator + operandValue;
        UpdateProcessorStatus(result, 0b11000011);
        registers.accumulator=result;
    break;
    case AND:
        
    }
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

    }
}