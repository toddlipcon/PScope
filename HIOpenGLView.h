#ifndef __HIOpenGLView__
#define __HIOpenGLView__

#include <Carbon/Carbon.h>

#ifdef __cplusplus
extern "C" {
#endif



OSStatus HIOpenGLViewCreate (WindowRef inWindow, const Rect* inBounds, ControlRef* outControl);
OSStatus HIOpenGLViewRegister ();



#ifdef __cplusplus
}
#endif

#endif
