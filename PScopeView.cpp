/*
*    File:        PScopeView.cpp
**    
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

#include "PScope.h"
#include "PScopeView.h"
#include "AUCarbonViewBase.h"
#include "AUControlGroup.h"
#include <AGL/agl.h>
#include <OpenGL/gl.h>
#include <AGL/glu.h>

COMPONENT_ENTRY(PScopeView)

PScopeView::PScopeView(AudioUnitCarbonView auv) : AUCarbonViewBase(auv),
    mPScope(NULL),
    mGLContext(NULL),
    mThreadDidDie(false)
{
    fprintf(stderr, "PScopeView ctor\n");
}

bool PScopeView::HandleEvent(EventRef event)
{
    UInt32 eclass = GetEventClass(event);
    UInt32 ekind = GetEventKind(event);
//    ControlRef control;
    
    switch (eclass) {
    case kEventClassWindow:
        switch (ekind) {
        case kEventWindowClosed:
            printf("caught window-closed\n");
            CleanupGLThread();
            break;
        }
        break;
    }

    return false;
    
}

void PScopeView::CleanupGLThread()
{
    if (mThreadDidDie)
        return;
        
    mThreadShouldDie = true;
    
    fprintf(stderr, "PScopeView dying. signalling thread to die...\n");
    MPSignalSemaphore(mPScope->ringDataAvailableSemaphore);
    fprintf(stderr, "PScopeView dying: signal sent. waiting on result queue\n");
    MPWaitOnQueue(mScopeThreadResultQueue,NULL,NULL,NULL,kDurationForever);
    fprintf(stderr, "PScopeView dying. thread died. setting view closed\n");    
    mPScope->viewIsOpen = false;

    if (mGLContext != NULL)
    {
        fprintf(stderr, "Destroying AGL context\n");
        aglDestroyContext(mGLContext);
        mGLContext = NULL;
    }

}

PScopeView::~PScopeView()
{    
    fprintf(stderr, "PScopeView dtor\n");

    fprintf(stderr, "Deleting scope thread result queue\n");
    CleanupGLThread();
    
    MPDeleteQueue(mScopeThreadResultQueue);
}

// ____________________________________________________________________________
//
OSStatus    PScopeView::CreateUI(Float32 xoffset, Float32 yoffset)
{
    fprintf(stderr, "PScopeView CreateUI\n");

    {
        UInt32 size = sizeof(PScope *);
        
        OSStatus err = AudioUnitGetProperty(mEditAudioUnit,
                                            kPScopePointerProperty,
                                            kAudioUnitScope_Global,
                                            0,
                                            &mPScope,
                                            &size);
        verify_noerr(err);

                
        verify(mPScope != NULL);
        verify(mPScope->GetMagicNumber() == kPScopeMagicNumber);

        mPScope->viewIsOpen = true;

    }
    
    // need offsets as int's:
    int xoff = (int)xoffset;
    int yoff = (int)yoffset;
    
    // for each parameter, create controls
    // inside mCarbonWindow, embedded in mCarbonPane
    
#define kLabelWidth 80
#define kLabelHeight 16
#define kEditTextWidth 40
#define kMinMaxWidth 32
#define kScopeDimension 350
#define kSpacing    10

    ControlRef newControl;
    ControlFontStyleRec fontStyle;
    fontStyle.flags = kControlUseFontMask | kControlUseJustMask;
    fontStyle.font = kControlFontSmallSystemFont;
    fontStyle.just = teFlushRight;
    
    Rect r;
    Point labelSize, textSize;
    labelSize.v = textSize.v = kLabelHeight;
    labelSize.h = kMinMaxWidth;
    textSize.h = kEditTextWidth;
    
    // decay time at top
    {
        CAAUParameter auvp(mEditAudioUnit, kParam_DecayTime, kAudioUnitScope_Global, 0);
        
        // text label
        r.top = 10 + yoff;
        r.bottom = r.top + kLabelHeight;
        r.left = 10 +xoff;
        r.right = r.left + kLabelWidth;
        
        verify_noerr(CreateStaticTextControl(mCarbonWindow, &r, auvp.GetName(), &fontStyle, &newControl));
        verify_noerr(EmbedControl(newControl));

        r.left = r.right + 4;
        r.right = r.left + 240;
        AUControlGroup::CreateLabelledSliderAndEditText(this, auvp, r, labelSize, textSize, fontStyle);
    }
    
    // and the main scope pane:
    {
        r.top += kLabelHeight + kSpacing;
        r.bottom = r.top + kScopeDimension;
        r.left = 10 + xoff;
        r.right = r.left + kScopeDimension;
        
        
        verify_noerr(CreateUserPaneControl(mCarbonWindow,
                                            &r,
                                            kControlWantsIdle,
                                            &mScopeControl));

        verify_noerr(EmbedControl(mScopeControl));
    }
    
    verify(mPScope->viewIsOpen == true);
    SetUpScopeThread();
    
    // set size of overall pane
    SizeControl(mCarbonPane, mBottomRight.h + 8, mBottomRight.v + 8);
    return noErr;
}

void PScopeView::SetUpScopeThread(void)
{
    fprintf(stderr, "PScopeView SetUpScopeThread\n");

    OSStatus err;
    
    mThreadShouldDie = false;
    mThreadDidDie = false;
    
    err = MPCreateQueue(&mScopeThreadResultQueue);
    verify_noerr(err);
    
    err = MPCreateTask(ScopeThreadEntry,this,0,mScopeThreadResultQueue,NULL,NULL,0,&mScopeThreadTaskID);
    verify_noerr(err);
                
}

OSStatus PScopeView::ScopeThreadEntry(void *param)
{
    PScopeView *psv = (PScopeView *)param;
    
    OSStatus err = psv->ScopeThread();
    
    return err;

}

OSStatus PScopeView::ScopeThread()
{    
    mThreadStartTicks = TickCount();
    mThreadIts = 0;
    
    verify(mPScope->viewIsOpen == true);

// cause it to create context whether or not we ever actually get any data
// so it goes black    
    GetGLContext();
    
    while (1)
    {
        OSStatus err = MPWaitOnSemaphore(mPScope->ringDataAvailableSemaphore, kDurationForever);
        verify_noerr(err);

        if (!mThreadShouldDie)
            RespondToNewData();
        else
            break;
    }

    mThreadDidDie = true;
    fprintf(stderr, "Thread should die. Ran %ld times in %ld ticks\n", mThreadIts, TickCount()-mThreadStartTicks);
    
    return noErr;
}


void PScopeView::RespondToNewData()
{
    mThreadIts++;

    PScopeView *ps = this;// (PScopeView *)userData;
    
    AGLContext context = ps->GetGLContext();
    
    aglSetCurrentContext(context);
    aglUpdateContext(context);

    glEnable(GL_SMOOTH);
    glEnable(GL_LINE_SMOOTH);
    glEnable(GL_ALPHA_TEST);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);    
    glEnable(GL_BLEND);
    
    DrawScope();        
    
    glFinish();
    
    QDFlushPortBuffer(GetWindowPort(mCarbonWindow), NULL);
    
}

float PScopeView::GetDecayAlpha()
{
    float decayTime = mPScope->GetParameter(kParam_DecayTime);
    float secs_per_frame = mPScope->lastFramesToProcess / mPScope->GetSampleRate();

    float decayFrames = decayTime / secs_per_frame;

    return 1.0/decayFrames;    
}

void PScopeView::DrawScope()
{

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(-1, 1, -1, 1);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    float alpha = GetDecayAlpha();

// Decay
    glBegin(GL_QUADS);
        glColor4f(0,0,0,alpha);
        glVertex2f(-1, -1);
        glColor4f(0,0,0,alpha);
        glVertex2f(1, -1);
        glColor4f(0,0,0,alpha);
        glVertex2f(1,1);
        glColor4f(0,0,0,alpha);
        glVertex2f(-1,1);
    glEnd();


    OSStatus err = MPEnterCriticalRegion(mPScope->ringLock, kDurationMillisecond);
    if (err != noErr)
    {
        fprintf(stderr, "Couldn't enter crit region for drawing: %ld\n", err);
        return;
    }

    glLineWidth(kLineWidth);
    glBegin(GL_LINE_STRIP);
        glColor4f(0.0, 1.0, 0.0, kLineAlpha);

        while (mPScope->ringReadIndex != mPScope->ringWriteIndex)
        {
            glVertex3f(mPScope->ringBuffer[mPScope->ringReadIndex], mPScope->ringBuffer[mPScope->ringReadIndex+1], 0.0);
            mPScope->ringReadIndex += 2;
            
            if (mPScope->ringReadIndex >= kRingBufferSize)
                mPScope->ringReadIndex -= kRingBufferSize;
        }
    glEnd();

    MPExitCriticalRegion(mPScope->ringLock);

}

AGLContext PScopeView::GetGLContext()
{
    if (mGLContext == NULL)
    {
        fprintf(stderr, "Creating context!\n");
        GLint attrib[] = { AGL_RGBA,  AGL_NONE };
        AGLPixelFormat fmt = aglChoosePixelFormat(nil, 0, attrib);
        mGLContext = aglCreateContext(fmt, nil);
        aglDestroyPixelFormat(fmt);
        assert(mGLContext != nil);

        HIRect ctrlBounds;
        Rect rWinBounds;
        HIViewGetBounds(mScopeControl, &ctrlBounds);

        WindowRef window = GetControlOwner(mScopeControl);
        ControlRef root = NULL;
        GetRootControl(window, &root);
        HIViewConvertRect(&ctrlBounds, mScopeControl, root);

        GetWindowBounds(mCarbonWindow,kWindowContentRgn, &rWinBounds);
        
        float winHeight = rWinBounds.bottom - rWinBounds.top;
            
        GLint bufferRect[4] = { (GLint)ctrlBounds.origin.x,
                                (GLint)(winHeight - (ctrlBounds.origin.y + ctrlBounds.size.height)),
                                (GLint)(ctrlBounds.size.width),
                                (GLint)ctrlBounds.size.height };

        aglSetInteger(mGLContext, AGL_BUFFER_RECT, bufferRect);
        aglEnable(mGLContext, AGL_BUFFER_RECT);

        aglSetDrawable(mGLContext, GetWindowPort(mCarbonWindow));
        aglSetCurrentContext(mGLContext);
        glClearColor(0,0,0,1);
        glClear(GL_COLOR_BUFFER_BIT);
    }

    return mGLContext;
}


