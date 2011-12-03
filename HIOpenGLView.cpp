#include <Carbon/Carbon.h>
#include <AGL/agl.h>
#include <OpenGL/gl.h>
#include <AGL/glu.h>
#include "HIOpenGLView.h"



struct HIOpenGLViewData
{
    ControlRef     mControl;
    AGLContext mContext;
};



OSStatus HIOpenGLViewCreate (WindowRef inWindow, const Rect* inBounds, ControlRef* outControl);
OSStatus HIOpenGLViewRegister ();



static pascal OSStatus HIOpenGLViewEventProc (EventHandlerCallRef inCall, EventRef inEvent, void* inUserData);
static OSStatus HIOpenGLViewEventHIObject (EventHandlerCallRef inCall, EventRef inEvent, HIOpenGLViewData* inData);
static OSStatus HIOpenGLViewEventHIObjectConstruct (EventHandlerCallRef inCall, EventRef inEvent, HIOpenGLViewData* inData);
static OSStatus HIOpenGLViewEventHIObjectInitialize (EventHandlerCallRef inCall, EventRef inEvent, HIOpenGLViewData* inData);
static OSStatus HIOpenGLViewEventHIObjectDestruct (EventHandlerCallRef inCall, EventRef inEvent, HIOpenGLViewData* inData);
static OSStatus HIOpenGLViewEventControl (EventHandlerCallRef inCall, EventRef inEvent, HIOpenGLViewData* inData);
static OSStatus HIOpenGLViewEventControlInitialize (EventHandlerCallRef inCall, EventRef inEvent, HIOpenGLViewData* inData);
static OSStatus HIOpenGLViewEventControlDraw (EventHandlerCallRef inCall, EventRef inEvent, HIOpenGLViewData* inData);
static OSStatus HIOpenGLViewEventControlHitTest (EventHandlerCallRef inCall, EventRef inEvent, HIOpenGLViewData* inData);
static OSStatus HIOpenGLViewEventControlHiliteChanged (EventHandlerCallRef inCall, EventRef inEvent, HIOpenGLViewData* inData);
static OSStatus HIOpenGLViewEventControlBoundsChanged (EventHandlerCallRef inCall, EventRef inEvent, HIOpenGLViewData* inData);
static OSStatus HIOpenGLViewEventControlOwningWindowChanged (EventHandlerCallRef inCall, EventRef inEvent, HIOpenGLViewData* inData);



#define kHIOpenGLViewClassID CFSTR("org.mesonet.sample.HIOpenGLView")



OSStatus HIOpenGLViewCreate (WindowRef inWindow, const Rect* inBounds, ControlRef* outControl)
{
    // Register this class
    OSStatus err = HIOpenGLViewRegister();
    require_noerr(err, CantRegister);

    // Make an initialization event
    EventRef event;
    err = CreateEvent(nil, kEventClassHIObject, kEventHIObjectInitialize, GetCurrentEventTime(), 0, &event);
    require_noerr(err, CantCreateEvent);
    
    // If bounds were specified, push the them into the initialization
    // event so that they can be used in the initialization handler.
    if (inBounds != nil)
    {
        err = SetEventParameter(event, 'Boun', typeQDRectangle, sizeof(Rect), inBounds);
        require_noerr(err, CantSetParameter);
    }

    // Make a new instantiation of this class
    err = HIObjectCreate(kHIOpenGLViewClassID, event, (HIObjectRef*)outControl);
    require_noerr(err, CantCreate);
    
    // If a parent window was specified, place the new view into the parent window.
    if (inWindow != nil)
    {
        ControlRef root = nil;
        err = GetRootControl(inWindow, &root);
        require_noerr(err, CantGetRootControl);
        err = HIViewAddSubview(root, *outControl);
    }

    CantCreate:
    CantGetRootControl:
    CantSetParameter:
    CantCreateEvent:
        ReleaseEvent(event);
    
    CantRegister:
        return err;
}



//    Register this class with the HIObject registry, notifying it of which
//    events we will be interested.
//    This API can be called multiple times, but will only register once.

OSStatus HIOpenGLViewRegister ()
{
    OSStatus err = noErr;
    static HIObjectClassRef sHIOpenGLViewClassRef = nil;

    if (sHIOpenGLViewClassRef == nil)
    {
        EventTypeSpec eventList[] =
        {
            { kEventClassHIObject, kEventHIObjectConstruct },
            { kEventClassHIObject, kEventHIObjectInitialize },
            { kEventClassHIObject, kEventHIObjectDestruct },
            { kEventClassControl, kEventControlInitialize },
            { kEventClassControl, kEventControlDraw },
            { kEventClassControl, kEventControlHitTest },
            { kEventClassControl, kEventControlHiliteChanged },
            { kEventClassControl, kEventControlBoundsChanged },
            { kEventClassControl, kEventControlOwningWindowChanged }
        };

        err = HIObjectRegisterSubclass(kHIOpenGLViewClassID, kHIViewClassID, nil, HIOpenGLViewEventProc, GetEventTypeCount(eventList), eventList, nil, &sHIOpenGLViewClassRef);
    }
    
    return err;
}



static void HIOpenGLViewSetContextWindowAndBounds (HIOpenGLViewData* inData)
{
    if (inData == nil) return;
    if (inData->mControl == nil) return;
    if (inData->mContext == nil) return;

    // Determine the AGL_BUFFER_RECT for the control. The coordinate
    // system for this rectangle is relative to the owning window, with
    // the origin at the bottom left corner and the y-axis inverted.
    HIRect ctrlBounds, winBounds;
    HIViewGetBounds(inData->mControl, &ctrlBounds);
    WindowRef window = GetControlOwner(inData->mControl);
    ControlRef root = nil;
    GetRootControl(window, &root);
    HIViewGetBounds(root, &winBounds);
    HIViewConvertRect(&ctrlBounds, inData->mControl, root);
    GLint bufferRect[4] = { (int)ctrlBounds.origin.x, (int)((winBounds.size.height) - (ctrlBounds.origin.y + ctrlBounds.size.height)),
                            (int)ctrlBounds.size.width, (int)ctrlBounds.size.height };
    
    // Associate the OpenGL context with the control's window, and establish the buffer rect.
    aglSetDrawable(inData->mContext, GetWindowPort(window));
    aglSetInteger(inData->mContext, AGL_BUFFER_RECT, bufferRect);
    aglEnable(inData->mContext, AGL_BUFFER_RECT);
    
    // Establish the clipping region for the OpenGL context. To properly handle clipping
    // within the view hierarchy, I'm walking the hierarchy to determine the intersection
    // of this view's bounds with its parents. Is there an easier way to do this?
    CGRect clipBounds = ctrlBounds;
    HIViewRef parent = HIViewGetSuperview(inData->mControl);
    while (parent != root)
    {
        CGRect parentBounds;
        HIViewGetBounds(parent, &parentBounds);
        HIViewConvertRect(&parentBounds, parent, root);
        clipBounds = CGRectIntersection(clipBounds, parentBounds);
        parent = HIViewGetSuperview(parent);
    }
    Rect rgnBounds = { (int)clipBounds.origin.y, (int)clipBounds.origin.x, (int)(clipBounds.origin.y + clipBounds.size.height), (int)(clipBounds.origin.x + clipBounds.size.width) };
    RgnHandle rgn = NewRgn();
    RectRgn(rgn, &rgnBounds);
    
    aglSetInteger(inData->mContext, AGL_CLIP_REGION, (const GLint*)rgn);
    aglEnable(inData->mContext, AGL_CLIP_REGION);
}



static AGLContext HIOpenGLViewGetContext (HIOpenGLViewData* inData)
{
    // If the OpenGL context hasn't been set up yet, then do so now.
    if (inData->mContext == nil)
    {
        GLint attrib[] = { AGL_RGBA , AGL_DOUBLEBUFFER, AGL_NONE };
        AGLPixelFormat fmt = aglChoosePixelFormat(nil, 0, attrib);
        inData->mContext = aglCreateContext(fmt, nil);
        aglDestroyPixelFormat(fmt);
        assert(inData->mContext != nil);
        HIOpenGLViewSetContextWindowAndBounds(inData);
        aglSetCurrentContext(inData->mContext);
    }

    return inData->mContext;
}



//    This is the event bottleneck through which all of the incoming events are dispatched.

pascal OSStatus HIOpenGLViewEventProc (EventHandlerCallRef inCall, EventRef inEvent, void* inUserData)
{
    HIOpenGLViewData* data = (HIOpenGLViewData*)inUserData;
    switch (GetEventClass(inEvent))
    {
        case kEventClassHIObject: return HIOpenGLViewEventHIObject(inCall, inEvent, data); break;
        case kEventClassControl: return HIOpenGLViewEventControl(inCall, inEvent, data); break;
        default: return eventNotHandledErr; break;
    }
}



OSStatus HIOpenGLViewEventHIObject (EventHandlerCallRef inCall, EventRef inEvent, HIOpenGLViewData* inData)
{
    fprintf(stderr, "HIOpenGLViewEventHIObject\n");
    switch (GetEventKind(inEvent))
    {
        case kEventHIObjectConstruct: return HIOpenGLViewEventHIObjectConstruct(inCall, inEvent, inData); break;
        case kEventHIObjectInitialize: return HIOpenGLViewEventHIObjectInitialize(inCall, inEvent, inData); break;
        case kEventHIObjectDestruct: return HIOpenGLViewEventHIObjectDestruct(inCall, inEvent, inData); break;
        default: return eventNotHandledErr; break;
    }
}



//    Do any instatiation-time preparation for the view.

OSStatus HIOpenGLViewEventHIObjectConstruct (EventHandlerCallRef, EventRef inEvent, HIOpenGLViewData*)
{
    OSStatus err;
    HIOpenGLViewData* data;

    // don't CallNextEventHandler!
    data = (HIOpenGLViewData*)malloc(sizeof(HIOpenGLViewData));
    require_action(data != nil, CantMalloc, err = memFullErr);
    data->mControl = nil;
    data->mContext = nil;

    err = GetEventParameter(inEvent, kEventParamHIObjectInstance, typeHIObjectRef, nil, sizeof(HIObjectRef), nil, (HIObjectRef*) &data->mControl);
    require_noerr(err, ParameterMissing);
    
    // Set the userData that will be used with all subsequent eventHandler calls
    err = SetEventParameter(inEvent, kEventParamHIObjectInstance, typeVoidPtr, sizeof(HIOpenGLViewData*), &data); 

    ParameterMissing:
        if (err != noErr) free(data);
    
    CantMalloc:
        return err;
}



OSStatus HIOpenGLViewEventHIObjectInitialize (EventHandlerCallRef inCall, EventRef inEvent, HIOpenGLViewData*)
{
    return CallNextEventHandler(inCall, inEvent);
}



OSStatus HIOpenGLViewEventHIObjectDestruct (EventHandlerCallRef, EventRef, HIOpenGLViewData*)
{
    // Don't CallNextEventHandler!
    return noErr;
}



OSStatus HIOpenGLViewEventControl (EventHandlerCallRef inCall, EventRef inEvent, HIOpenGLViewData* inData)
{
    fprintf(stderr, "HIOpenGLViewEventControl\n");
    switch (GetEventKind(inEvent))
    {
        case kEventControlInitialize: return HIOpenGLViewEventControlInitialize(inCall, inEvent, inData); break;
        case kEventControlDraw: return HIOpenGLViewEventControlDraw(inCall, inEvent, inData); break;
        case kEventControlHitTest: return HIOpenGLViewEventControlHitTest(inCall, inEvent, inData); break;
        case kEventControlHiliteChanged: return HIOpenGLViewEventControlHiliteChanged(inCall, inEvent, inData); break;
        case kEventControlBoundsChanged: return HIOpenGLViewEventControlBoundsChanged(inCall, inEvent, inData); break;
        case kEventControlOwningWindowChanged: return HIOpenGLViewEventControlOwningWindowChanged(inCall, inEvent, inData); break;
        default: return eventNotHandledErr; break;
    }
}



OSStatus HIOpenGLViewEventControlInitialize (EventHandlerCallRef, EventRef, HIOpenGLViewData*)
{
    return noErr;
}



OSStatus HIOpenGLViewEventControlDraw (EventHandlerCallRef, EventRef, HIOpenGLViewData* inData)
{
    // We don't always know the control bounds at construction time, so
    // we wait until the first time we draw to set up the OpenGL context.
    AGLContext context = HIOpenGLViewGetContext(inData);
    
    HIRect bounds;
    OSStatus err = HIViewGetBounds(inData->mControl, &bounds);
    if (err != noErr) return err;
    
    double w = bounds.size.width;
    double h = bounds.size.height;
    double alpha = (GetControlHilite(inData->mControl) == kControlNoPart) ? 1.0 : 0.5;

    aglSetCurrentContext(context);
    aglUpdateContext(context);
    
    glClearColor(1.0, 1.0, 1.0, 1.0);
    glClear (GL_COLOR_BUFFER_BIT);
    glEnable(GL_SMOOTH);
    glEnable(GL_ALPHA_TEST);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);    
    glEnable(GL_BLEND);
        
    GLint r[4];
    glGetIntegerv(GL_VIEWPORT, r);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    gluOrtho2D(0, r[2] - r[0], 0, r[3] - r[1]);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    
    glBegin(GL_QUADS);
        glColor4f(1.0, 0.0, 0.0, alpha);
        glVertex3f(0.0, h, 0.0);
        glColor4f(1.0, 1.0, 0.0, alpha);
        glVertex3f(w, h, 0.0);
        glColor4f(0.0, 0.0, 1.0, alpha);
        glVertex3f(w, 0.0, 0.0);    
        glColor4f(0.0, 1.0, 0.0, alpha);
        glVertex3f(0.0, 0.0, 0.0);
    glEnd();
    
    aglSwapBuffers(context);
    return noErr;
}



//    Check to see if a point hits the view

OSStatus HIOpenGLViewEventControlHitTest (EventHandlerCallRef, EventRef inEvent, HIOpenGLViewData* inData)
{
    // Extract the mouse location
    HIPoint where;
    ControlPartCode part;
    OSStatus err = GetEventParameter(inEvent, kEventParamMouseLocation, typeHIPoint, nil, sizeof(HIPoint), nil, &where);
    require_noerr(err, ParameterMissing);

    // Is the mouse in the view?
    HIRect bounds;
    err = HIViewGetBounds(inData->mControl, &bounds);
    part = CGRectContainsPoint(bounds, where) ? 1 : kControlNoPart;

    // Send back the value of the hit part
    err = SetEventParameter(inEvent, kEventParamControlPart, typeControlPartCode, sizeof(ControlPartCode), &part); 

    ParameterMissing:
        return err;
}



OSStatus HIOpenGLViewEventControlHiliteChanged (EventHandlerCallRef, EventRef, HIOpenGLViewData* inData)
{
    HIViewSetNeedsDisplay(inData->mControl, true);
    return noErr;
}



OSStatus HIOpenGLViewEventControlBoundsChanged (EventHandlerCallRef, EventRef, HIOpenGLViewData* inData)
{
    HIOpenGLViewSetContextWindowAndBounds(inData);
    return noErr;
}



OSStatus HIOpenGLViewEventControlOwningWindowChanged (EventHandlerCallRef, EventRef, HIOpenGLViewData* inData)
{
    HIOpenGLViewSetContextWindowAndBounds(inData);
    return noErr;
}
