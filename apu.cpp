#include "nes.h"

void NES::APUHandleRegisterWrite(uint16_t address, uint8_t value)
{
    switch(address)
    {
    case 0x0:
        pulse1.duty = (value & 0b11000000) >> 6;
        pulse1.infinite = value & 0b100000;
        pulse1.constantVol = value & 0b10000;
        pulse1.volumeEnvelope = value & 0xF;
    break;
    case 0x1:
        pulse1.sweepEnabled = value & 0b10000000;
        pulse1.period = (value & 0b1110000) >> 4;
        pulse1.negate = value & 0b1000;
        pulse1.shift = value&0b111;
    break;
    case 0x2:
        pulse1.timer = (pulse1.timer & 0xFF00) | value;
    break;
    case 0x3:
        pulse1.lengthLoadCounter = (value &0b11111000) >> 3;
        pulse1.timer = (pulse1.timer & 0x00FF) | ((value & 0b111) <<8);
    break;
    case 0x4:
        pulse2.duty = (value & 0b11000000) >> 6;
        pulse2.infinite = value & 0b100000;
        pulse2.constantVol = value & 0b10000;
        pulse2.volumeEnvelope = value & 0xF;
    break;
    case 0x5:
        pulse2.sweepEnabled = value & 0b10000000;
        pulse2.period = (value & 0b1110000) >> 4;
        pulse2.negate = value & 0b1000;
        pulse2.shift = value & 0b111;
    break;
    case 0x6:
        pulse2.timer = (pulse1.timer & 0xFF00) | value;
    break;
    case 0x7:
        pulse2.lengthLoadCounter = (value & 0b11111000) >> 3;
        pulse2.timer = (pulse1.timer & 0x00FF) | ((value & 0b111) << 8);
    break;
    case 0x8:
        triangle.infinite = value & 0b10000000;
        triangle.linearCounterLoad = value & 0x7F;
    break;
    case 0x9:
    break;
    case 0xA:
        triangle.timer = (pulse1.timer & 0xFF00) | value;
    break;
    case 0xB:
        triangle.lengthLoadCounter = (value & 0b11111000) >> 3;
        triangle.timer = (pulse1.timer & 0x00FF) | ((value & 0b111) << 8);
    break;
    case 0xC:
        noise.infinite = value & 0b100000;
        noise.constantVolume = value & 0b10000;
        noise.volumeEnvelope = value & 0xF;
    break;
    case 0xD:
    break;
    case 0xE:
        noise.loop=value & 0b10000000;
        noise.period = value & 0xF;
    break;
    case 0xF:
        noise.lengthLoadCounter = value >> 3;
    break;
    case 0x10:
        DMC.IRQEnable = value & 0b10000000;
        if(!DMC.IRQEnable) DMCinterrupt=false;
        DMC.loop = value & 0b1000000;
        DMC.frequency = value & 0xF;
    break;
    case 0x11:
        DMC.loadCounter = value & 0x7F;
    break;
    case 0x12:
        DMC.sampleAddress = value;
    break;
    case 0x13:
        DMC.sampleLength = value;
    break;
    case 0x15:
        DMCinterrupt=false;
        APUstatus.enableDMC = value & 0b10000;
        APUstatus.enableNoise = value & 0b1000;
        APUstatus.enableTriangle = value & 0b100;
        APUstatus.enablePulse2 = value & 0b10;
        APUstatus.enablePulse1 = value & 0b1;
    break;
    case 0x17:
        APUstatus.inhibitIRQ = value & 0b1000000;
        APUstatus.mode5steps = value & 0b10000000;
    break;
    }
}

uint8_t NES::APUHandleRegisterRead(uint16_t address)
{
    if(address!=0x15)
        return 0;

    uint8_t result=0;
    if(pulse1.lengthLoadCounter>0 && APUstatus.enablePulse1) result |=1;
    if(pulse2.lengthLoadCounter>0 && APUstatus.enablePulse2) result |=0b10;
    if(triangle.lengthLoadCounter>0 && APUstatus.enableTriangle) result |=0b100;
    if(noise.lengthLoadCounter>0 && APUstatus.enableNoise) result |=0b1000;
    //TODO implement DMC before handling this flag
    if(frameInterrupt) result |=0b1000000;
    if(DMCinterrupt) result |=0b10000000;

    return result;
}

void NES::UpdateAudio()
{
    if (!(APUscanlineTiming >= (header.isPAL ? 156 : 131)))
        return;

    APUscanlineTiming -= header.isPAL ? 156 : 131;

    switch(APUstage)
    {
    case 0:
        APUQuaterClock();
    break;
    case 1:
        APUQuaterClock();
        APUHalfClock();
    break;
    case 2:
        APUQuaterClock();
    break;
    case 3: 
        if(!APUstatus.mode5steps) 
        {
            APUQuaterClock();
            APUHalfClock();
            APUFrameClock();
        }
    break;
    case 4:
        APUQuaterClock();
        APUHalfClock();
        APUFrameClock();
    }

    APUstage++;
    if (APUstage >= (APUstatus.mode5steps ? 5 : 4))
        APUstage=0;

    MixAudio();
}

void NES::APUQuaterClock()
{
}

void NES::APUHalfClock()
{
}

void NES::APUFrameClock()
{
}

void NES::MixAudio()
{

}