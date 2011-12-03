/*
*    File:        PScopeView.h
*    
*    Version:    1.0
* 
*    Created:    1/15/05
*    
*    Copyright:  Copyright � 2005 Todd Lipcon, All Rights Reserved
*/

#ifndef __PScopeView__H_
#define __PScopeView__H_

#include "PScopeVersion.h"

#include "AUCarbonViewBase.h"
#include "AUControlGroup.h"
#include <map>
#include <vector>
#include "AGL/agl.h"

static const UInt32  kNumDisplayRows = 1;

#define kLineWidth    2
#define kLineAlpha    1.0

class PScopeView : public AUCarbonViewBase {

public:
    PScopeView(AudioUnitCarbonView auv);
    virtual ~PScopeView();
    
    virtual OSStatus CreateUI (Float32    inXOffset, Float32     inYOffset);
    virtual bool     HandleEvent(EventRef event);


    static OSStatus ScopeThreadEntry(void *param);
    OSStatus ScopeThread();


    void RespondToNewData();

private:
    AGLContext    GetGLContext();
    void        SetUpScopeThread(void);
    void        DrawScope();
    void        CleanupGLThread();
    float        GetDecayAlpha();

    PScope *mPScope;
    
    ControlRef mScopeControl;
    AGLContext mGLContext;

    MPTaskID        mScopeThreadTaskID;

    MPQueueID        mScopeThreadResultQueue;

    bool mThreadShouldDie;
    bool mThreadDidDie;
    long mThreadStartTicks;
    long mThreadIts;    

};

#endif