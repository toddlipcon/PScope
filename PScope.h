/*
*    File:        PScope.h
*    
*    Version:    1.0
* 
*    Created:    1/15/05
*    
*    Copyright:  Copyright © 2005 Todd Lipcon, All Rights Reserved
* 
*    Disclaimer:    IMPORTANT:  This Apple software is supplied to you by Apple Computer, Inc. ("Apple") in 
*                consideration of your agreement to the following terms, and your use, installation, modification 
*                or redistribution of this Apple software constitutes acceptance of these terms.  If you do 
*                not agree with these terms, please do not use, install, modify or redistribute this Apple 
*                software.
*
*                In consideration of your agreement to abide by the following terms, and subject to these terms, 
*                Apple grants you a personal, non-exclusive license, under Apple's copyrights in this 
*                original Apple software (the "Apple Software"), to use, reproduce, modify and redistribute the 
*                Apple Software, with or without modifications, in source and/or binary forms; provided that if you 
*                redistribute the Apple Software in its entirety and without modifications, you must retain this 
*                notice and the following text and disclaimers in all such redistributions of the Apple Software. 
*                Neither the name, trademarks, service marks or logos of Apple Computer, Inc. may be used to 
*                endorse or promote products derived from the Apple Software without specific prior written 
*                permission from Apple.  Except as expressly stated in this notice, no other rights or 
*                licenses, express or implied, are granted by Apple herein, including but not limited to any 
*                patent rights that may be infringed by your derivative works or by other works in which the 
*                Apple Software may be incorporated.
*
*                The Apple Software is provided by Apple on an "AS IS" basis.  APPLE MAKES NO WARRANTIES, EXPRESS OR 
*                IMPLIED, INCLUDING WITHOUT LIMITATION THE IMPLIED WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY 
*                AND FITNESS FOR A PARTICULAR PURPOSE, REGARDING THE APPLE SOFTWARE OR ITS USE AND OPERATION ALONE 
*                OR IN COMBINATION WITH YOUR PRODUCTS.
*
*                IN NO EVENT SHALL APPLE BE LIABLE FOR ANY SPECIAL, INDIRECT, INCIDENTAL OR CONSEQUENTIAL 
*                DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS 
*                OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) ARISING IN ANY WAY OUT OF THE USE, 
*                REPRODUCTION, MODIFICATION AND/OR DISTRIBUTION OF THE APPLE SOFTWARE, HOWEVER CAUSED AND WHETHER 
*                UNDER THEORY OF CONTRACT, TORT (INCLUDING NEGLIGENCE), STRICT LIABILITY OR OTHERWISE, EVEN 
*                IF APPLE HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*
*/
#include "AUEffectBase.h"
#include "PScopeVersion.h"
#if AU_DEBUG_DISPATCHER
#include "AUDebugDispatcher.h"
#endif


#ifndef __PScope_h__
#define __PScope_h__

#define kPScopeMagicNumber  0xC0DE70AD
#define kRingBufferSize        96000

#pragma mark ____PScope Parameters


static const float kMinValue_DecayTime = 0.01;
static const float kDefaultValue_DecayTime = 0.3;
static const float kMaxValue_DecayTime = 1.5;

static CFStringRef kDecayTimeParamName = CFSTR("Decay Time");

enum {
    kParam_DecayTime =0,
    //Add more parameters here
    kNumberOfParameters=1
};

static const AudioUnitPropertyID    kPScopePointerProperty=10001;

#pragma mark ____PScope 
class PScope : public AUEffectBase
{
public:
    PScope(AudioUnit component);
    virtual ~PScope();

    virtual UInt32                SupportedNumChannels (    const AUChannelInfo**            outInfo)
    {
        static AUChannelInfo chans[] = { {2, 2} };
        if (outInfo != NULL)
            *outInfo = chans;
        
        return sizeof(chans) / sizeof(AUChannelInfo);
    }

        
    virtual    ComponentResult        GetParameterValueStrings(AudioUnitScope            inScope,
                                                         AudioUnitParameterID        inParameterID,
                                                         CFArrayRef *            outStrings);
    
    virtual    ComponentResult        GetParameterInfo(AudioUnitScope            inScope,
                                                 AudioUnitParameterID    inParameterID,
                                                 AudioUnitParameterInfo    &outParameterInfo);
    
    virtual ComponentResult        GetPropertyInfo(AudioUnitPropertyID        inID,
                                                AudioUnitScope            inScope,
                                                AudioUnitElement        inElement,
                                                UInt32 &            outDataSize,
                                                Boolean    &            outWritable );
    
    virtual ComponentResult        GetProperty(AudioUnitPropertyID inID,
                                            AudioUnitScope        inScope,
                                            AudioUnitElement        inElement,
                                            void *            outData);
    

    virtual OSStatus            ProcessBufferLists(
                                            AudioUnitRenderActionFlags &    ioActionFlags,
                                            const AudioBufferList &            inBuffer,
                                            AudioBufferList &                outBuffer,
                                            UInt32                            inFramesToProcess );


       virtual    bool                SupportsTail () { return false; }
    
    /*! @method Version */
    virtual ComponentResult    Version() { return kPScopeVersion; }
    
    int        GetNumCustomUIComponents () { return 1; }
    
    void    GetUIComponentDescs (ComponentDescription* inDescArray) {
        inDescArray[0].componentType = kAudioUnitCarbonViewComponentType;
        inDescArray[0].componentSubType = PScope_COMP_SUBTYPE;
        inDescArray[0].componentManufacturer = PScope_COMP_MANF;
        inDescArray[0].componentFlags = 0;
        inDescArray[0].componentFlagsMask = 0;
    }
    
    UInt32 GetMagicNumber() { return mMagicNumber; }
    
    
private:
    Float32            ringBuffer[kRingBufferSize];
    int                ringReadIndex;
    int                ringWriteIndex;
    MPCriticalRegionID    ringLock;

    MPSemaphoreID    ringDataAvailableSemaphore;

    long            mMagicNumber;

    friend class    PScopeView;
    
    bool            viewIsOpen;
    
    UInt32            lastFramesToProcess;
};
#endif