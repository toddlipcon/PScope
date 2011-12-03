/*
*    File:        PScope.cpp
*    
*    Version:    1.0
* 
*    Created:    1/15/05
*    
*    Copyright:  Copyright © 2005 Todd Lipcon, All Rights Reserved
* 
*/
#include "PScope.h"


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

COMPONENT_ENTRY(PScope)


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//    PScope::PScope
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
PScope::PScope(AudioUnit component)
    : AUEffectBase(component),
    ringReadIndex(0),
    ringWriteIndex(0),
    mMagicNumber(kPScopeMagicNumber),
    viewIsOpen(false)
{
    CreateElements();
    Globals()->UseIndexedParameters(kNumberOfParameters);
    SetParameter(kParam_DecayTime, kDefaultValue_DecayTime );
    
    OSStatus err = MPCreateCriticalRegion(&ringLock);
    verify_noerr(err);

    err = MPCreateBinarySemaphore(&ringDataAvailableSemaphore);
    verify_noerr(err);
    
    // ring buffer size has to be even. otherwise we'll die.
    assert(kRingBufferSize % 2 == 0);
    
#if AU_DEBUG_DISPATCHER
    mDebugDispatcher = new AUDebugDispatcher (this);
#endif
    
}

PScope::~PScope()
{
    MPDeleteCriticalRegion(ringLock);
    MPDeleteSemaphore(ringDataAvailableSemaphore);
#if AU_DEBUG_DISPATCHER
     delete mDebugDispatcher;
#endif
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//    PScope::GetParameterValueStrings
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ComponentResult        PScope::GetParameterValueStrings(AudioUnitScope        inScope,
                                                                AudioUnitParameterID    inParameterID,
                                                                CFArrayRef *        outStrings)
{
    return kAudioUnitErr_InvalidProperty;
}



//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//    PScope::GetParameterInfo
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ComponentResult        PScope::GetParameterInfo(AudioUnitScope        inScope,
                                                        AudioUnitParameterID    inParameterID,
                                                        AudioUnitParameterInfo    &outParameterInfo )
{
    ComponentResult result = noErr;

    outParameterInfo.flags =     kAudioUnitParameterFlag_IsWritable
                        |        kAudioUnitParameterFlag_IsReadable;
    
    if (inScope == kAudioUnitScope_Global) {
        switch(inParameterID)
        {
            case kParam_DecayTime:
                AUBase::FillInParameterName (outParameterInfo, kDecayTimeParamName, false);
                outParameterInfo.unit = kAudioUnitParameterUnit_Seconds;
                outParameterInfo.minValue = kMinValue_DecayTime;
                outParameterInfo.maxValue = kMaxValue_DecayTime;
                outParameterInfo.defaultValue = kDefaultValue_DecayTime;
                break;
            default:
                result = kAudioUnitErr_InvalidParameter;
                break;
        }
    } else {
        result = kAudioUnitErr_InvalidParameter;
    }
    


    return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//    PScope::GetPropertyInfo
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ComponentResult        PScope::GetPropertyInfo (AudioUnitPropertyID    inID,
                                                        AudioUnitScope        inScope,
                                                        AudioUnitElement    inElement,
                                                        UInt32 &        outDataSize,
                                                        Boolean &        outWritable)
{
    if (inScope == kAudioUnitScope_Global)
    {
        switch (inID)
        {
            case kPScopePointerProperty:
                outWritable = true;
                outDataSize = sizeof(PScope *);
                return noErr;
        }
    }
    
    return AUEffectBase::GetPropertyInfo (inID, inScope, inElement, outDataSize, outWritable);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//    PScope::GetProperty
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ComponentResult        PScope::GetProperty(    AudioUnitPropertyID inID,
                                                        AudioUnitScope         inScope,
                                                        AudioUnitElement     inElement,
                                                        void *            outData )
{
    if (inScope == kAudioUnitScope_Global)
    {
        switch (inID)
        {
            case kPScopePointerProperty:
                *((PScope **)outData) = this;
                return noErr;
                break;
        }
    }
    
    return AUEffectBase::GetProperty (inID, inScope, inElement, outData);
}


OSStatus            PScope::ProcessBufferLists(    AudioUnitRenderActionFlags &    ioActionFlags,
                                                const AudioBufferList &            inBuffer,
                                                AudioBufferList &                outBuffer,
                                                UInt32                            inFramesToProcess )
{    
    lastFramesToProcess = inFramesToProcess;
    
    if (!viewIsOpen)
        return noErr;
    
    OSStatus err = MPEnterCriticalRegion(ringLock, kDurationForever);
    if (err != noErr)
        return err;
    
    bool overRun = false;
    
    if (inBuffer.mNumberBuffers == 1)
    {
    // interleaved
        for (UInt32 i=0; i < inFramesToProcess; i++)
        {
            ringBuffer[ringWriteIndex++] = ((Float32 *)inBuffer.mBuffers[0].mData)[i*2];
            ringBuffer[ringWriteIndex++] = ((Float32 *)inBuffer.mBuffers[0].mData)[i*2 + 1];

            verify(ringWriteIndex <= kRingBufferSize);

            if (ringWriteIndex == kRingBufferSize)
                ringWriteIndex -= kRingBufferSize;
            
            if (ringReadIndex == ringWriteIndex)
                overRun = true;
        }
    } else {
        for (UInt32 i=0; i < inFramesToProcess; i++)
        {
            ringBuffer[ringWriteIndex++] = ((Float32 *)inBuffer.mBuffers[0].mData)[i];
            ringBuffer[ringWriteIndex++] = ((Float32 *)inBuffer.mBuffers[1].mData)[i];

            verify(ringWriteIndex <= kRingBufferSize);

            if (ringWriteIndex >= kRingBufferSize)
                ringWriteIndex -= kRingBufferSize;
            
            if (ringReadIndex == ringWriteIndex)
                overRun = true;            
        }
    }
    
    if (overRun) {
        fprintf(stderr, "PScope: ring buffer overrun. skipping ahead reader\n");
        ringReadIndex = ringWriteIndex;
    }

    MPExitCriticalRegion(ringLock);

    MPSignalSemaphore(ringDataAvailableSemaphore);
    
    
    return noErr;
}    

