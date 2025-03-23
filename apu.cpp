#include "nes.h"



void NES::APUHandleRegisterWrite(uint16_t address, uint8_t value)
{
    constexpr uint8_t lengthLUT[32]={10, 254, 20, 2, 40, 4, 80, 6, 160, 8, 60, 10, 14, 12, 26, 14, 12, 16, 24, 18, 48, 20, 96, 22, 192, 24, 72, 26, 16, 28, 32, 30};
    switch(address)
    {
    case 0x0:
        pulse1.duty = (value & 0b11000000) >> 6;
        switch(pulse1.duty)
        {
        case 0: pulse1.decodedWaveform=0.125;
        break;
        case 1: pulse1.decodedWaveform=0.25;
        break;
        case 2: pulse1.decodedWaveform=0.5;
        break;
        case 3: pulse1.decodedWaveform=0.75;
        }
        pulse1.infinite = value & 0b100000;
        pulse1.constantVol = value & 0b10000;
        pulse1.volumeEnvelope = value & 0xF;
        pulse1.volumeEnvelopeLoad=pulse1.volumeEnvelope;
    break;
    case 0x1:
        pulse1.sweepEnabled = value & 0b10000000;
        pulse1.period = (value & 0b1110000) >> 4;
        pulse1.periodLoad=pulse1.period;
        pulse1.negate = value & 0b1000;
        pulse1.shift = value&0b111;
        pulse1.sweepReload=true;
    break;
    case 0x2:
        pulse1.timer = (pulse1.timer & 0xFF00) | value;
        pulse1.targetTimer=pulse1.timer;
    break;
    case 0x3:
        pulse1.length = lengthLUT[(value & 0b11111000) >> 3];
        pulse1.timer = (pulse1.timer & 0x00FF) | ((value & 0b111) <<8);
        pulse1.targetTimer = pulse1.timer;
        pulse1.envelopeStart=true;
    break;
    case 0x4:
        pulse2.duty = (value & 0b11000000) >> 6;
        switch(pulse2.duty)
        {
        case 0: pulse2.decodedWaveform=0.125;
        break;
        case 1: pulse2.decodedWaveform=0.25;
        break;
        case 2: pulse2.decodedWaveform=0.5;
        break;
        case 3: pulse2.decodedWaveform=0.75;
        }
        pulse2.infinite = value & 0b100000;
        pulse2.constantVol = value & 0b10000;
        pulse2.volumeEnvelope = value & 0xF;
        pulse2.volumeEnvelopeLoad = pulse2.volumeEnvelope;
    break;
    case 0x5:
        pulse2.sweepEnabled = value & 0b10000000;
        pulse2.period = (value & 0b1110000) >> 4;
        pulse2.periodLoad = pulse2.period;
        pulse2.negate = value & 0b1000;
        pulse2.shift = value & 0b111;
        pulse2.sweepReload = true;
    break;
    case 0x6:
        pulse2.timer = (pulse2.timer & 0xFF00) | value;
        pulse2.targetTimer = pulse2.timer;
    break;
    case 0x7:
        pulse2.length = lengthLUT[(value & 0b11111000) >> 3];
        pulse2.timer = (pulse2.timer & 0x00FF) | ((value & 0b111) << 8);
        pulse2.targetTimer = pulse2.timer;
        pulse2.envelopeStart = true;
    break;
    case 0x8:
        triangle.infinite = value & 0b10000000;
        triangle.linearCounterLoad = value & 0x7F;
        triangle.currentLinearCounter = triangle.linearCounterLoad;
    break;
    case 0x9:
    break;
    case 0xA:
        triangle.timer = (triangle.timer & 0xFF00) | value;
        //triangle.waveformPos = 0;
    break;
    case 0xB:
        triangle.length = lengthLUT[(value & 0b11111000) >> 3];
        triangle.timer = (triangle.timer & 0x00FF) | ((value & 0b111) << 8);
        //triangle.waveformPos=0;
        triangle.reloadLinear=true;
    break;
    case 0xC:
        noise.infinite = value & 0b100000;
        noise.constantVolume = value & 0b10000;
        noise.volumeEnvelope = value & 0xF;
        noise.volumeEnvelopeLoad=noise.volumeEnvelope;
    break;
    case 0xD:
    break;
    case 0xE:
        noise.loop=value & 0b10000000;
        noise.period = value & 0xF;
        switch(noise.period)
        {
        case 0x0:
            noise.periodClockCycles = header.isPAL ? 4 : 4;
            break;
        case 0x1:
            noise.periodClockCycles = header.isPAL ? 8 : 8;
            break;
        case 0x2:
            noise.periodClockCycles = header.isPAL ? 14 : 16;
            break;
        case 0x3:
            noise.periodClockCycles = header.isPAL ? 30 : 32;
            break;
        case 0x4:
            noise.periodClockCycles = header.isPAL ? 60 : 64;
            break;
        case 0x5:
            noise.periodClockCycles = header.isPAL ? 88 : 96;
            break;
        case 0x6:
            noise.periodClockCycles = header.isPAL ? 118 : 128;
            break;
        case 0x7:
            noise.periodClockCycles = header.isPAL ? 148 : 160;
            break;
        case 0x8:
            noise.periodClockCycles = header.isPAL ? 188 : 202;
            break;
        case 0x9:
            noise.periodClockCycles = header.isPAL ? 236 : 254;
            break;
        case 0xA:
            noise.periodClockCycles = header.isPAL ? 354 : 380;
            break;
        case 0xB:
            noise.periodClockCycles = header.isPAL ? 472 : 508;
            break;
        case 0xC:
            noise.periodClockCycles = header.isPAL ? 708 : 762;
            break;
        case 0xD:
            noise.periodClockCycles = header.isPAL ? 944 : 1016;
            break;
        case 0xE:
            noise.periodClockCycles = header.isPAL ? 1890 : 2034;
            break;
        case 0xF:
            noise.periodClockCycles = header.isPAL ? 1890 : 4068;
            break;
        }
    break;
    case 0xF:
        noise.length = lengthLUT[value >> 3];
        noise.envelopeStart=true;
    break;
    case 0x10:
        DMC.IRQEnable = value & 0b10000000;
        if(!DMC.IRQEnable) DMCinterrupt=false;
        DMC.loop = value & 0b1000000;
        DMC.frequency = value & 0xF;
        DMC.frequencyDecoded=0;
        switch(DMC.frequency)
        {//breaks are ugly so i do like this, probably gets compiler optimized anyways
        case 0x0: DMC.frequencyDecoded += (header.isPAL ? 44 : 48);
        case 0x1: DMC.frequencyDecoded += (header.isPAL ? 38 : 40);
        case 0x2: DMC.frequencyDecoded += (header.isPAL ? 18 : 20);
        case 0x3: DMC.frequencyDecoded += (header.isPAL ? 22 : 34);
        case 0x4: DMC.frequencyDecoded += (header.isPAL ? 40 : 32);
        case 0x5: DMC.frequencyDecoded += (header.isPAL ? 26 : 28);
        case 0x6: DMC.frequencyDecoded += (header.isPAL ? 12 : 12);
        case 0x7: DMC.frequencyDecoded += (header.isPAL ? 22 : 24);
        case 0x8: DMC.frequencyDecoded += (header.isPAL ? 28 : 30);
        case 0x9: DMC.frequencyDecoded += (header.isPAL ? 16 : 18);
        case 0xA: DMC.frequencyDecoded += (header.isPAL ? 14 : 14);
        case 0xB: DMC.frequencyDecoded += (header.isPAL ? 20 : 22);
        case 0xC: DMC.frequencyDecoded += (header.isPAL ? 20 : 22);
        case 0xD: DMC.frequencyDecoded += (header.isPAL ? 12 : 12);
        case 0xE: DMC.frequencyDecoded += (header.isPAL ? 16 : 18);
        case 0xF: DMC.frequencyDecoded += (header.isPAL ? 50 : 54);
        }
    break;
    case 0x11:
        DMC.currentOutput = value & 0x7F;
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
        if(!APUstatus.enablePulse1) pulse1.length=0;
        if(!APUstatus.enablePulse2) pulse2.length=0;
        if(!APUstatus.enableNoise) noise.length=0;
        if(!APUstatus.enableTriangle) triangle.length=0;
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
    if(pulse1.length>0 && APUstatus.enablePulse1) result |=1;
    if(pulse2.length>0 && APUstatus.enablePulse2) result |=0b10;
    if(triangle.length>0 && APUstatus.enableTriangle) result |=0b100;
    if(noise.length>0 && APUstatus.enableNoise) result |=0b1000;
    //TODO implement DMC before handling this flag
    if(frameInterrupt) result |=0b1000000;
    if(DMCinterrupt) result |=0b10000000;

    return result;
}

void NES::UpdateAudio()
{
    UpdateDMC();
    if (!(APUscanlineTiming >= (header.isPAL ? 156 : 131)))
        return;

    APUscanlineTiming -= header.isPAL ? 156 : 131;

    FillBuffers();
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
}

void NES::UpdateDMC()
{
    DMCClockCycles+=(header.isPAL ? 106 : 113);
    while(DMCClockCycles >= DMC.frequencyDecoded)
    {//TODO implement
        DMC.inProgressData.push_back(0);
        DMCClockCycles-=DMC.frequencyDecoded;
    }
}

void NES::FillPulseData(PulseAudio& pulseChannel, uint8_t* data, bool enabled)
{
    int numSamples = 12000 / (header.isPAL ? 50 : 60);
    if ((pulseChannel.timer < 8 || pulseChannel.length == 0 || !enabled || pulseChannel.targetTimer > 0x7FF))
    {
        for (int i = 0; i < numSamples; i++)
        {
            data[i] = 0;
        }
    }
    else
    {
        uint8_t volume = (pulseChannel.constantVol ? pulseChannel.volumeEnvelope : pulseChannel.decayCounter);
        double numWaveforms = (header.isPAL ? 1662607.0 / 200.0 : 1789773.0 / 240.0) / (16.0 * (pulseChannel.timer + 1));
        double numSamplesPerWaveform = (double)numSamples / numWaveforms;
        for(int i=0; i<numSamples; i++)
        {
            data[i] = (pulseChannel.waveformPos/numSamplesPerWaveform > pulseChannel.decodedWaveform) ? 0 : (pulseChannel.constantVol ? pulseChannel.volumeEnvelopeLoad : pulseChannel.decayCounter);
            pulseChannel.waveformPos++;
            while (pulseChannel.waveformPos >= numSamplesPerWaveform)
                pulseChannel.waveformPos -= numSamplesPerWaveform;
        }
    }
}

void NES::FillBuffers()
{
    //debug
    //APUstatus.enableNoise=false;
    //APUstatus.enableTriangle=false;
    //APUstatus.enableDMC=false;

    //these flags constantly overwrites length
    if(!APUstatus.enablePulse1) pulse1.length=0;
    if(!APUstatus.enablePulse2) pulse2.length=0;
    if(!APUstatus.enableNoise) noise.length=0;
    if(!APUstatus.enableTriangle) triangle.length=0;

    int numSamples = 12000 / (header.isPAL ? 50 : 60);
    //pulse channels
    uint8_t pulse1Data[numSamples];
    uint8_t pulse2Data[numSamples];
    FillPulseData(pulse1, pulse1Data, APUstatus.enablePulse1);
    FillPulseData(pulse2, pulse2Data, APUstatus.enablePulse2);

    //triangle
    uint8_t triangleData[numSamples];
    if(triangle.timer<2 || triangle.currentLinearCounter ==0 || triangle.length == 0 || !APUstatus.enableTriangle)
    {
        for(int i=0; i< numSamples; i++)
            triangleData[i]=0;
    }
    else
    {
        double numWaveforms = (header.isPAL ? 1662607.0 / 200.0 : 1789773.0 / 240.0) / (32 * (triangle.timer + 1));
        double numSamplesPerWaveform = (double)numSamples / numWaveforms;
        int samplesWritten =0;
        constexpr int triangleWaveformLUT[32] = {15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
        for(; samplesWritten < numSamples; samplesWritten++)
        {
            triangleData[samplesWritten] = triangleWaveformLUT[(int)floor((triangle.waveformPos/numSamplesPerWaveform) * 32)];
            triangle.waveformPos++;
            while (triangle.waveformPos >= numSamplesPerWaveform)
                triangle.waveformPos -= numSamplesPerWaveform;
        }
    }

    //noise
    uint8_t noiseData[numSamples];
    if(noise.length == 0 || !APUstatus.enableNoise)
    {
        for (int i = 0; i < numSamples; i++)
            noiseData[i] = 0;
    }
    else
    {
        double numWaveforms = (header.isPAL ? 1662607.0 / 200.0 : 1789773.0 / 240.0) / noise.periodClockCycles;
        double numSamplesPerWaveform = (double)numSamples / numWaveforms;
        int samplesWritten = 0;
        bool randResult = rand() & 1;
        for(int i=0; i<numSamples; i++)
        {
            if (randResult)
                noiseData[i]= (noise.constantVolume ? noise.volumeEnvelopeLoad : noise.decayCounter);
            else
                noiseData[i]=0;
            noise.waveformPos++;
            while (noise.waveformPos >= numSamplesPerWaveform)
            {
                noise.waveformPos -= numSamplesPerWaveform;
                randResult=rand() & 1;
            }
        }

    }
    MixAudio(pulse1Data, pulse2Data, triangleData, noiseData);
}

void NES::ClockEnvelope(bool &envelopeStart, uint8_t &decayCounter, uint8_t &volumeEnvelope, uint8_t volumeEnvelopeLoad, bool infinite, bool constantVolume)
{
    if (envelopeStart)
    {
        envelopeStart = false;
        decayCounter = 15;
        volumeEnvelope = volumeEnvelopeLoad;
    }
    else
    {
        if (volumeEnvelope == 0)
        {
            volumeEnvelope = volumeEnvelopeLoad;
            if (decayCounter == 0 && infinite)
                decayCounter = 15;
            else if (decayCounter != 0)
                decayCounter--;
        }
        else
            volumeEnvelope--;
    }
}

void NES::APUQuaterClock()
{
    ClockEnvelope(pulse1.envelopeStart, pulse1.decayCounter, pulse1.volumeEnvelope, pulse1.volumeEnvelopeLoad, pulse1.infinite, pulse1.constantVol);
    ClockEnvelope(pulse2.envelopeStart, pulse2.decayCounter, pulse2.volumeEnvelope, pulse2.volumeEnvelopeLoad, pulse2.infinite, pulse2.constantVol);
    ClockEnvelope(noise.envelopeStart, noise.decayCounter, noise.volumeEnvelope, noise.volumeEnvelopeLoad, noise.infinite, noise.constantVolume);
    //TODO triangle counter here
    if(triangle.reloadLinear)
    {
        triangle.currentLinearCounter=triangle.linearCounterLoad;
    }
    else if(triangle.currentLinearCounter>0)
        triangle.currentLinearCounter--;
    if (!triangle.infinite)
        triangle.reloadLinear = false;
}

void NES::APUHalfClock()
{
    if(!pulse1.infinite && pulse1.length>0) pulse1.length--;
    if(!pulse2.infinite && pulse2.length>0) pulse2.length--;
    if(!triangle.infinite && triangle.length>0) triangle.length--;
    if(!noise.infinite && noise.length>0) noise.length--;

    int16_t changeAmount = (pulse1.timer >> pulse1.shift) & 0x7FF;
    if (pulse1.negate)
        changeAmount = -changeAmount - 1;
    pulse1.targetTimer = pulse1.timer + changeAmount;
    if (pulse1.targetTimer < 0)
        pulse1.targetTimer = 0;

    changeAmount = (pulse2.timer >> pulse2.shift) & 0x7FF;
    if (pulse2.negate)
        changeAmount = -changeAmount;
    pulse2.targetTimer = pulse2.timer + changeAmount;
    if (pulse2.targetTimer < 0)
        pulse2.targetTimer = 0;

    if (pulse1.sweepEnabled && pulse1.shift != 0)
    {
        if (pulse1.period == 0 && pulse1.targetTimer <=0x7FF)
                pulse1.timer = pulse1.targetTimer;

        if (pulse1.period == 0 || pulse1.sweepReload)
        {
            pulse1.period = pulse1.periodLoad;
            pulse1.sweepReload = false;
        }
        else if (pulse1.period > 0)
            pulse1.period--;
    }

    if (pulse2.sweepEnabled && pulse2.shift != 0)
    {
        if (pulse2.period == 0 && pulse2.targetTimer <= 0x7FF)
                pulse2.timer = pulse2.targetTimer;

        if (pulse2.period == 0 || pulse2.sweepReload)
        {
            pulse2.period = pulse2.periodLoad;
            pulse2.sweepReload = false;
        }
        else if (pulse2.period > 0)
            pulse2.period--;
    }
}

void NES::APUFrameClock()
{
    
}

constexpr double makePulseLUT(int i)
{
    return (95.52 / ((8128.0 / i) + 100));
}

struct PulseLUT
{
    double vals[31];
    constexpr PulseLUT() : vals() 
    {
        vals[0]=0;
        for(int i=1; i<31; i++)
            vals[i]=makePulseLUT(i);
    }
};

constexpr double makeTNDLUT(int i)
{
    return (163.67 / ((24329.0 / i) + 100));
}

struct TNDLUT
{
    double vals[203];
    constexpr TNDLUT() : vals()
    {
        vals[0] = 0;
        for (int i = 1; i < 203; i++)
            vals[i] = makeTNDLUT(i);
    }
};

void NES::MixAudio(uint8_t *pulse1Data, uint8_t *pulse2Data, uint8_t *triangleData, uint8_t *noiseData)
{
    constexpr PulseLUT pulseOut;
    constexpr TNDLUT tndOut;
    int numSamples = 12000 / (header.isPAL ? 50 : 60);
    //fill out DMC with most recent value if not exactly quarter frame # samples yet
    //TODO
    while(DMC.inProgressData.size() < numSamples)
        DMC.inProgressData.push_back(0);

    for (int i = 0; i < numSamples; i++)
    {
        partialData[partialCounter++] = (uint16_t)(20000*(pulseOut.vals[pulse1Data[i]+pulse2Data[i]] + tndOut.vals[3*triangleData[i] + 2*noiseData[i]+DMC.inProgressData[i]]));
        
        if(partialCounter>=512)
        {
            dataQueueLock.lock();
            audioDataQueue.push(partialData);
            dataQueueLock.unlock();
            partialCounter=0;
        }
    }
    DMC.inProgressData.clear();
    
}