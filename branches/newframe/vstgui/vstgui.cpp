//-----------------------------------------------------------------------------
// VST Plug-Ins SDK
// VSTGUI: Graphical User Interface Framework for VST plugins : 
//
// Version 2.2       Date : 14/05/03
//
// First version            : Wolfgang Kundrus         06.97
// Added Motif/Windows vers.: Yvan Grabit              01.98
// Added Mac version        : Charlie Steinberg        02.98
// Added BeOS version       : Georges-Edouard Berenger 05.99
// Added new functions      : Matthias Juwan           12.01
// Added MacOSX version     : Arne Scheffler           02.03
// Added Quartz stuff		: Arne Scheffler           08.03
// Added Win Alpha Blending : Arne Scheffler           04.04
//
//-----------------------------------------------------------------------------
// VSTGUI LICENSE
// � 2003, Steinberg Media Technologies, All Rights Reserved
//-----------------------------------------------------------------------------
// Redistribution and use in source and binary forms, with or without modification,
// are permitted provided that the following conditions are met:
// 
//   * Redistributions of source code must retain the above copyright notice, 
//     this list of conditions and the following disclaimer.
//   * Redistributions in binary form must reproduce the above copyright notice,
//     this list of conditions and the following disclaimer in the documentation 
//     and/or other materials provided with the distribution.
//   * Neither the name of the Steinberg Media Technologies nor the names of its
//     contributors may be used to endorse or promote products derived from this 
//     software without specific prior written permission.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED 
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A  PARTICULAR PURPOSE ARE DISCLAIMED. 
// IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, 
// INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, 
// BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, 
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF 
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE 
// OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE  OF THIS SOFTWARE, EVEN IF ADVISED
// OF THE POSSIBILITY OF SUCH DAMAGE.
//-----------------------------------------------------------------------------

#ifndef __vstgui__
#include "vstgui.h"
#endif

#if !PLUGGUI
#ifndef __audioeffectx__
#include "audioeffectx.h"
#endif
#endif

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

//---Some defines-------------------------------------
#define USE_ALPHA_BLEND			QUARTZ || USE_LIBPNG
#define USE_CLIPPING_DRAWRECT	1
#define MAC_OLD_DRAG			1
#define NEW_UPDATE_MECHANISM	1

#if !WINDOWS
// For OS which allows a lot of Drawing contexts
#define USE_GLOBAL_CONTEXT 1
#endif

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
#if USE_NAMESPACE
#define VSTGUI_CFrame     VSTGUI::CFrame
#define VSTGUI_CPoint     VSTGUI::CPoint
#define VSTGUI_kDropFiles VSTGUI::kDropFiles
#define VSTGUI_kDropText  VSTGUI::kDropText
#define VSTGUI_CTextEdit  VSTGUI::CTextEdit
#define VSTGUI_CColor     VSTGUI::CColor
#define VSTGUI_CDrawContext VSTGUI::CDrawContext
#define VSTGUI_COptionMenu VSTGUI::COptionMenu
#define VSTGUI_COptionMenuScheme VSTGUI::COptionMenuScheme
#else
#define VSTGUI_CFrame     CFrame
#define VSTGUI_CPoint     CPoint
#define VSTGUI_kDropFiles kDropFiles
#define VSTGUI_kDropText  kDropText
#define VSTGUI_CTextEdit  CTextEdit
#define VSTGUI_CColor     CColor
#define VSTGUI_CDrawContext CDrawContext
#define VSTGUI_COptionMenu COptionMenu
#define VSTGUI_COptionMenuScheme COptionMenuScheme
#endif

//---For Debugging------------------------
#if DEBUG

long gNbCBitmap = 0;
long gNbCView = 0;
long gNbCDrawContext = 0;
long gNbCOffscreenContext = 0;
long gBitmapAllocation = 0;
long gNbDC = 0;

#include <stdarg.h>

void DebugPrint (char *format, ...);
void DebugPrint (char *format, ...)
{
	char string[300];
	va_list marker;
	va_start (marker, format);
	vsprintf (string, format, marker);
	if (!string)
		strcpy (string, "Empty string\n");
	#if WINDOWS
	OutputDebugString (string);
	#elif MAC && !MACX
	Str255 pStr;
	c2pstrcpy (pStr, string);
	DebugStr (pStr);
	#else
	fprintf (stderr, string);
	#endif
}
#endif
//---End For Debugging------------------------

#if WINDOWS
static bool bSwapped_mouse_buttons = false; 

// Alpha blending for Windows using library : msimg32.dll
#define DYNAMICALPHABLEND   1

#define WIN32_LEAN_AND_MEAN 1
#include <windows.h>
#include <shlobj.h>
#include <shellapi.h>
#include <zmouse.h>

#if DYNAMICALPHABLEND
typedef  BOOL (WINAPI *PFNALPHABLEND)(
  HDC hdcDest,                 // handle to destination DC
  int nXOriginDest,            // x-coord of upper-left corner
  int nYOriginDest,            // y-coord of upper-left corner
  int nWidthDest,              // destination width
  int nHeightDest,             // destination height
  HDC hdcSrc,                  // handle to source DC
  int nXOriginSrc,             // x-coord of upper-left corner
  int nYOriginSrc,             // y-coord of upper-left corner
  int nWidthSrc,               // source width
  int nHeightSrc,              // source height
  BLENDFUNCTION blendFunction  // alpha-blending function
);

PFNALPHABLEND pfnAlphaBlend = NULL;

typedef	BOOL (WINAPI *PFNTRANSPARENTBLT)(
  HDC hdcDest,        // handle to destination DC
  int nXOriginDest,   // x-coord of destination upper-left corner
  int nYOriginDest,   // y-coord of destination upper-left corner
  int nWidthDest,     // width of destination rectangle
  int hHeightDest,    // height of destination rectangle
  HDC hdcSrc,         // handle to source DC
  int nXOriginSrc,    // x-coord of source upper-left corner
  int nYOriginSrc,    // y-coord of source upper-left corner
  int nWidthSrc,      // width of source rectangle
  int nHeightSrc,     // height of source rectangle
  UINT crTransparent  // color to make transparent
);

PFNTRANSPARENTBLT	pfnTransparentBlt = NULL;
#endif

#if PLUGGUI
	extern HINSTANCE ghInst;
	inline HINSTANCE GetInstance () { return ghInst; }
#else
	extern void* hInstance;
	inline HINSTANCE GetInstance () { return (HINSTANCE)hInstance; }
#endif

static long   gUseCount = 0;
static char   gClassName[20];
static bool   InitWindowClass ();
static void   ExitWindowClass ();
LONG_PTR WINAPI WindowProc (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

static HANDLE CreateMaskBitmap (CDrawContext* pContext, CRect& rect, CColor transparentColor);
static void   DrawTransparent (CDrawContext* pContext, CRect& rect, const CPoint& offset, HDC hdcBitmap, POINT ptSize, HBITMAP pMask, COLORREF color);
static bool   checkResolveLink (const char* nativePath, char* resolved);
static void   *createDropTarget (VSTGUI_CFrame* pFrame);

BEGIN_NAMESPACE_VSTGUI
long        gStandardFontSize[] = { 12, 18, 14, 12, 11, 10, 9, 13 };
const char* gStandardFontName[] = {
	"Arial", "Arial", "Arial", 
	"Arial", "Arial", "Arial", 
	"Arial", "Symbol" };
END_NAMESPACE_VSTGUI

#if USE_LIBPNG
#include "png.h"
#endif

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
#elif MOTIF

 #define USE_XPM     0
 #define TEST_REGION 0

 #if USE_XPM
  #include <X11/xpm.h>
 #endif

 #include <X11/Xlib.h>
 #include <Xm/DrawingA.h>
 #include <assert.h>
 #include <Xm/MwmUtil.h>
 #include <Xm/DialogS.h>
 #include <time.h>

 #if SGI
   #include <sys/syssgi.h>
 #elif SUN
 #elif LINUX
 #endif

 #define XDRAWPARAM pDisplay, (Window)pWindow, (GC)pSystemContext
 #define XWINPARAM  pDisplay, (Window)pWindow
 #define XGCPARAM   pDisplay, (GC)pSystemContext

// init the static variable about font
bool gFontInit = false;
XFontStruct *gFontStructs[] = {0, 0, 0, 0, 0, 0, 0};

struct SFontTable {char* name; char* string;};

static SFontTable gFontTable[] = {
  {"SystemFont",        "-adobe-helvetica-bold-r-*-*-12-*-*-*-*-*-*-*"},   // kSystemFont
  {"NormalFontVeryBig", "-adobe-helvetica-medium-r-*-*-18-*-*-*-*-*-*-*"}, // kNormalFontVeryBig
  {"NormalFontBig",     "-adobe-helvetica-medium-r-normal-*-14-*-*-*-*-*-*-*"}, // kNormalFontBig
  {"NormalFont",        "-adobe-helvetica-medium-r-*-*-12-*-*-*-*-*-*-*"}, // kNormalFont
  {"NormalFontSmall",   "-adobe-helvetica-medium-r-*-*-10-*-*-*-*-*-*-*"}, // kNormalFontSmall
  {"NormalFontSmaller",   "-adobe-helvetica-medium-r-*-*-9-*-*-*-*-*-*-*"}, // kNormalFontSmaller
  {"NormalFontVerySmall", "-adobe-helvetica-medium-r-*-*-8-*-*-*-*-*-*-*"},  // kNormalFontVerySmall
  {"SymbolFont",        "-adobe-symbol-medium-r-*-*-12-*-*-*-*-*-*-*"}     // kSymbolFont
};

long gStandardFontSize[] = { 12, 16, 14, 12, 10, 9, 8, 10 };

//-----------------------------------------------------------------------------
// declaration of different local functions
long convertPoint2Angle (CPoint &pm, CPoint &pt);

// callback for the frame
void _drawingAreaCallback (Widget widget, XtPointer clientData, XtPointer callData);
void _eventHandler (Widget w, XtPointer clientData, XEvent *event, char *p);
void _destroyCallback (Widget widget, XtPointer clientData, XtPointer callData);

// stuff for color
long getIndexColor8Bit (CColor color, Display *pDisplay, Colormap colormap);
long CDrawContext::nbNewColor = 0;
static CColor paletteNewColor[256];

//------ our user-defined XPM functions
bool xpmGetValues (char **ppDataXpm, long *pWidth, long *pHeight, long *pNcolor, long *pCpp);

 #if !USE_XPM
  #include "xpmloader.cpp"
 #endif

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
#elif MAC

long pSystemVersion;

#if MACX
//-----------------------------------------------------------------------------
#include <QuickTime/ImageCompression.h>

#if QUARTZ
const char* gMacXfontNames[] = {
	"Helvetica",
	"Helvetica",
	"Helvetica",
	"Helvetica",
	"Helvetica",
	"Helvetica",
	"Helvetica",
	"Symbol"
};

#ifndef M_PI
#define	M_PI		3.14159265358979323846	/* pi */
#endif

static inline double radians (double degrees) { return degrees * M_PI / 180; }

#else
const unsigned char* gMacXfontNames[] = {
	"\pArial",
	"\pArial",
	"\pArial",
	"\pArial",
	"\pArial",
	"\pArial",
	"\pArial",
	"\pSymbol"
};
#endif

//-----------------------------------------------------------------------------
#else
#include <QDOffscreen.h>
#include <StandardFile.h>
#include <Navigation.h>
#include <PictUtils.h>
#endif

long gStandardFontSize[] = { 12, 18, 14, 12, 10, 9, 9, 12 };

long convertPoint2Angle (CPoint &pm, CPoint &pt);
void RectNormalize (Rect& rect);
void CRect2Rect (const CRect &cr, Rect &rr);
void Rect2CRect (Rect &rr, CRect &cr);
void CColor2RGBColor (const CColor &cc, RGBColor &rgb);
void RGBColor2CColor (const RGBColor &rgb, CColor &cc);

#if MAC_OLD_DRAG
static void install_drop (CFrame *frame);
static void remove_drop (CFrame *frame);
#endif

//-----------------------------------------------------------------------------
void RectNormalize (Rect& rect)
{
	if (rect.left > rect.right)
	{
		long temp = rect.right;
		rect.right = rect.left;
		rect.left = temp;
	}
	if (rect.top > rect.bottom)
	{
		long temp = rect.bottom;
		rect.bottom = rect.top;
		rect.top = temp;
	}
}

//-----------------------------------------------------------------------------
void CRect2Rect (const CRect &cr, Rect &rr)
{
	rr.left   = cr.left;
	rr.right  = cr.right;
	rr.top    = cr.top;
	rr.bottom = cr.bottom;
	RectNormalize (rr);
}

//-----------------------------------------------------------------------------
void Rect2CRect (Rect &rr, CRect &cr)
{
	cr.left   = rr.left;
	cr.right  = rr.right;
	cr.top    = rr.top;
	cr.bottom = rr.bottom;
}

//-----------------------------------------------------------------------------
void CColor2RGBColor (const CColor &cc, RGBColor &rgb)
{
	rgb.red   = cc.red   * 257;
	rgb.green = cc.green * 257;
	rgb.blue  = cc.blue  * 257;
}

//-----------------------------------------------------------------------------
void RGBColor2CColor (const RGBColor &rgb, CColor &cc)
{
	cc.red   = rgb.red   / 257;
	cc.green = rgb.green / 257;
	cc.blue  = rgb.blue  / 257;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
#elif BEOS

#include <TranslationUtils.h>
#include <Resources.h>
#include <Bitmap.h>
#include <Region.h>
#include <View.h>
#include <Window.h>
#include <Message.h>
#include <Entry.h>
#include <Path.h>

//--------------------------
class PlugView: public BView
{
public:
	PlugView (BRect frame, CFrame* cframe);
	void Draw (BRect updateRect);
	void MouseDown (BPoint where);
	void MessageReceived (BMessage *msg);
private:
	CFrame*	cframe;
};

long convertPoint2Angle (CPoint &pm, CPoint &pt);

drawing_mode modeToPlatform [] = {
//  kCopyMode  kOrMode    kXorMode
	B_OP_COPY, B_OP_OVER, B_OP_INVERT
};

long gStandardFontSize[] = { 12, 18, 14, 12, 11, 10, 9, 12 };
const char*	standardFont  = "Swis721 BT";
const char* standardFontS = "Roman";
const char* systemFont    = "Swis721 BT";
const char* systemFontS   = "Bold";
const char* gStandardFontName[] = { systemFont,
	standardFont, standardFont, standardFont, standardFont, standardFont,
	standardFont };
const char* gStandardFontStyle[] = { systemFontS,
	standardFontS, standardFontS, standardFontS, standardFontS, standardFontS,
	standardFontS };
#endif

//-----------------------------------------------------------------------------
bool CRect::pointInside (const CPoint& where) const
{
	return where.h >= left && where.h < right && where.v >= top && where.v < bottom;
}

//-----------------------------------------------------------------------------
bool CRect::isEmpty () const 
{
	if (right <= left)
		return true;
	if (bottom <= top)
		return true;
	return false;
}

//-----------------------------------------------------------------------------
void CRect::bound (const CRect& rect)
{
	if (left < rect.left)
		left = rect.left;
	if (top < rect.top)
		top = rect.top;
	if (right > rect.right)
		right = rect.right;
	if (bottom > rect.bottom)
		bottom = rect.bottom;
	if (bottom < top)
		bottom = top;
	if (right < left)
		right = left;
}

BEGIN_NAMESPACE_VSTGUI

CColor kTransparentCColor = {255, 255, 255, 0};
CColor kBlackCColor  = {0,     0,   0, 255};
CColor kWhiteCColor  = {255, 255, 255, 255};
CColor kGreyCColor   = {127, 127, 127, 255};
CColor kRedCColor    = {255,   0,   0, 255};
CColor kGreenCColor  = {0  , 255,   0, 255};
CColor kBlueCColor   = {0  ,   0, 255, 255};
CColor kYellowCColor = {255, 255,   0, 255};
CColor kMagentaCColor= {255,   0, 255, 255};
CColor kCyanCColor   = {0  , 255, 255, 255};

#define kDragDelay 0

//-----------------------------------------------------------------------------
// CDrawContext Implementation
//-----------------------------------------------------------------------------

CDrawContext::CDrawContext (CFrame *inFrame, void *inSystemContext, void *inWindow)
:	pSystemContext (inSystemContext), pWindow (inWindow), pFrame (inFrame), 
	frameWidth (1), lineStyle (kLineSolid), drawMode (kCopyMode)
	#if WINDOWS
	,pBrush (0), pFont (0), pPen (0), pOldBrush (0), pOldPen (0), pOldFont (0)
	#elif MAC && !QUARTZ
	,bInitialized (false)
	#endif
{
	#if DEBUG
	gNbCDrawContext++;
	#endif

	// initialize values
	if (pFrame)
		pFrame->getViewSize (clipRect);
	else
		clipRect (0, 0, 1000, 1000);
	frameColor = kWhiteCColor;
	fillColor  = kBlackCColor;
	fontColor  = kWhiteCColor;

	// offsets use by offscreen
	offset (0, 0);
	offsetScreen (0, 0);

#if WINDOWS
	pHDC = 0;
	if (!pSystemContext && pWindow)
		pSystemContext = pHDC = GetDC ((HWND)pWindow);

	if (pSystemContext)
	{
		pOldBrush  = GetCurrentObject ((HDC)pSystemContext, OBJ_BRUSH);
		pOldPen    = GetCurrentObject ((HDC)pSystemContext, OBJ_PEN);
		pOldFont   = GetCurrentObject ((HDC)pSystemContext, OBJ_FONT);
		SetBkMode ((HDC)pSystemContext, TRANSPARENT);
	}
	iPenStyle = PS_SOLID;

	// get position 
	if (pWindow)
	{
		RECT  rctTempWnd;
		GetWindowRect ((HWND)pWindow, &rctTempWnd);
		offsetScreen.h = rctTempWnd.left;
		offsetScreen.v = rctTempWnd.top;
	}

#elif MAC
	#if QUARTZ
	if (pFrame && (pSystemContext || pWindow))
	{
		HIRect bounds;
		HIViewGetFrame ((HIViewRef)pFrame->getPlatformControl (), &bounds);
		if (pWindow)
		{
			HIViewRef contentView;
			HIViewFindByID (HIViewGetRoot ((WindowRef)pWindow), kHIViewWindowContentID, &contentView);
			HIViewConvertRect (&bounds, (HIViewRef)pFrame->getPlatformControl (), contentView);
		}
		offsetScreen.x = bounds.origin.x;
		offsetScreen.y = bounds.origin.y;
		clipRect (0, 0, bounds.size.width, bounds.size.height);
	}
	gCGContext = 0;
	if (pSystemContext)
	{
		CGContextSaveGState (gCGContext); // save the original state
		gCGContext = (CGContextRef) pSystemContext;
		CGContextScaleCTM (gCGContext, 1, -1);
		CGContextSetShouldAntialias (gCGContext, false);
		CGContextSaveGState (gCGContext);
		setClipRect (clipRect);
		if (pFrame)
			pFrame->setDrawContext (this);
	}
	else if (pWindow)
	{
		GrafPtr port = GetWindowPort ((WindowRef)pWindow);
		OSStatus err = QDBeginCGContext (port, &gCGContext);
		if (err == noErr)
		{
			CGContextSaveGState (gCGContext); // save the original state
			SyncCGContextOriginWithPort (gCGContext, port);
			Rect rect;
			GetPortBounds (port, &rect);
			CGContextTranslateCTM (gCGContext, 0, rect.bottom - rect.top);
			CGContextTranslateCTM (gCGContext, offsetScreen.x, -offsetScreen.y);
			CGContextSetShouldAntialias (gCGContext, false);
			CGContextSaveGState (gCGContext);
			setClipRect (clipRect);
			if (pFrame)
				pFrame->setDrawContext (this);
		}
	}

	#else
	pSystemContext = pWindow;
	#endif
	
#elif MOTIF 
	if (pFrame)
		pDisplay = pFrame->getDisplay ();
	
	// set the current font
	if (pSystemContext)
		setFont (kNormalFont);
	else
		fprintf (stderr, "Error in CDrawContext::CDrawContext : pSystemContext must not be Null!!!\n");

#elif BEOS
	pView = (BView*) pSystemContext;
	if (pView)
		pView->LockLooper ();

#endif

	if (pSystemContext)
	{
		// set the default values
		setFrameColor (frameColor);
		setLineStyle (lineStyle);
		
#if !MOTIF
		setFillColor (fillColor);
		setFontColor (fontColor);
#endif
	}
}

//-----------------------------------------------------------------------------
CDrawContext::~CDrawContext ()
{
	#if DEBUG
	gNbCDrawContext--;
	#endif

#if WINDOWS
	if (pOldBrush)
		SelectObject ((HDC)pSystemContext, pOldBrush);
	if (pOldPen)
		SelectObject ((HDC)pSystemContext, pOldPen);
	if (pOldFont)
		SelectObject ((HDC)pSystemContext, pOldFont);
	
	if (pBrush)
		DeleteObject (pBrush);
	if (pPen)
		DeleteObject (pPen);
	if (pFont)
		DeleteObject (pFont);
  
	if (pHDC)
	{
		ReleaseDC ((HWND)pWindow, pHDC);
		#if DEBUG
		gNbDC--;
		#endif
	}

#elif (MAC && QUARTZ)
	if (gCGContext)
	{
		CGContextRestoreGState (gCGContext); // restore the original state
		CGContextSynchronize (gCGContext);
		if (!pSystemContext && pWindow)
			QDEndCGContext (GetWindowPort ((WindowRef)pWindow), &gCGContext);
		if (pFrame)
			pFrame->setDrawContext (0);
	}
#elif MOTIF
#elif BEOS
	if (pView)
	{
		pView->Flush ();
		pView->UnlockLooper ();
	}
#endif
}

//-----------------------------------------------------------------------------
void CDrawContext::moveTo (const CPoint &_point)
{
	CPoint point (_point);
	point.offset (offset.h, offset.v);

#if WINDOWS
	MoveToEx ((HDC)pSystemContext, point.h, point.v, NULL);
  
#elif MAC
	#if QUARTZ
	#else
	CGrafPtr OrigPort;
	GDHandle OrigDevice;
	GetGWorld (&OrigPort, &OrigDevice); // get current GrafPort
	SetGWorld (getPort (), NULL);       // activate our GWorld
	MoveTo (point.h, point.v);
	SetGWorld (OrigPort, OrigDevice);
	#endif
  	penLoc = point;
  	
#elif MOTIF || BEOS
	penLoc = point;
#endif
}

//-----------------------------------------------------------------------------
void CDrawContext::lineTo (const CPoint& _point)
{
	CPoint point (_point);
	point.offset (offset.h, offset.v);

#if WINDOWS
	LineTo ((HDC)pSystemContext, point.h, point.v);
	
#elif MAC
	#if QUARTZ
	CGContextRef context = beginCGContext ();
	{
		if (drawMode == kAntialias)
			CGContextSetLineWidth (context, 2 * frameWidth);
		CGContextScaleCTM (context, 1, -1);

		CGRect cgClipRect = CGRectMake (clipRect.left, clipRect.top, clipRect.width (), clipRect.height ());
		CGContextClipToRect (gCGContext, cgClipRect);

		CGContextTranslateCTM (context, 0.5f, 0.5f);

		CGContextBeginPath (context);
		CGContextMoveToPoint (context, penLoc.h, penLoc.v);
		CGContextAddLineToPoint (context, point.h, point.v);
		CGContextClosePath (context);
		CGContextDrawPath (context, kCGPathStroke);
		releaseCGContext (context);
	}
	penLoc = point;
	#else
	CGrafPtr OrigPort;
	GDHandle OrigDevice;
	GetGWorld (&OrigPort, &OrigDevice); // get current GrafPort
	SetGWorld (getPort (), NULL);       // activate our GWorld
	RGBColor col;
	CColor2RGBColor (frameColor, col);
	RGBForeColor (&col);
	#if 1
	if (point.v == penLoc.v)
	{
		CPoint old = point;
		if (point.h > penLoc.h)
			point.h--;
		else
			point.h++;
		penLoc = old;
		LineTo (point.h, point.v);
		MoveTo (penLoc.h, penLoc.v);
	}
	else if (point.h == penLoc.h)
	{
		CPoint old = point;
		if (point.v > penLoc.v)
			point.v--;
		else
			point.v++;
		penLoc = old;
		LineTo (point.h, point.v);
		MoveTo (penLoc.h, penLoc.v);
	}
	else
	{
		penLoc = point;	
		LineTo (point.h, point.v);
	}
	#else
	if (point.v > penLoc.v)
		point.v--;
	else if (point.v < penLoc.v)
		point.v++;
	if (point.h > penLoc.h)
		point.h--;
	else if (point.h < penLoc.h)
		point.h++;
	penLoc = point;
	LineTo (point.h, point.v);
	#endif
	SetGWorld (OrigPort, OrigDevice);
	#endif

#elif MOTIF
	CPoint start (penLoc);
	CPoint end (point);
	if (start.h == end.h)
	{
		if (start.v < -5)
			start.v = -5;
		else if (start.v > 10000)
			start.v = 10000;
		
		if (end.v < -5)
			end.v = -5;
		else if (end.v > 10000)
			end.v = 10000;
	}
	if (start.v == end.v)
	{
		if (start.h < -5)
			start.h = -5;
		else if (start.h > 10000)
			start.h = 10000;
		
		if (end.h < -5)
			end.h = -5;
		else if (end.h > 10000)
			end.h = 10000;
	}
	XDrawLine (XDRAWPARAM, start.h, start.v, end.h, end.v);
	
	// keep trace of the new position
	penLoc = point;

#elif BEOS
	rgb_color c = { frameColor.red, frameColor.green, frameColor.blue, 255 };
	pView->SetHighColor (c);
	pView->SetDrawingMode (modeToPlatform [drawMode]);
	pView->SetPenSize (frameWidth);
	lineFromTo (penLoc, point);
	penLoc = point;
#endif
}

//-----------------------------------------------------------------------------
void CDrawContext::polyLine (const CPoint *pPoints, long numberOfPoints)
{
#if WINDOWS
	POINT points[30];
	POINT *polyPoints;
	bool allocated = false;

	if (numberOfPoints > 30)
	{
		polyPoints = (POINT*)new char [numberOfPoints * sizeof (POINT)];
		if (!polyPoints)
			return;
		allocated = true;
	}
	else
		polyPoints = points;

	for (long i = 0; i < numberOfPoints; i++)
	{
		polyPoints[i].x = pPoints[i].h + offset.h;
		polyPoints[i].y = pPoints[i].v + offset.v;
	}

	Polyline ((HDC)pSystemContext, polyPoints, numberOfPoints);

	if (allocated)
		delete[] polyPoints;

#elif MAC
	#if QUARTZ
	CGContextRef context = beginCGContext ();
	{
		CGContextScaleCTM (context, 1, -1);

		CGRect cgClipRect = CGRectMake (clipRect.left, clipRect.top, clipRect.width (), clipRect.height ());
		CGContextClipToRect (gCGContext, cgClipRect);

		CGContextTranslateCTM (context, 0.5f, 0.5f);

		CGContextBeginPath (context);
		CGContextMoveToPoint (context, pPoints[0].h + offset.h, pPoints[0].v + offset.v);
		for (long i = 1; i < numberOfPoints; i++)
			CGContextAddLineToPoint (context, pPoints[i].h + offset.h, pPoints[i].v + offset.v);
		CGContextClosePath (context);
		CGContextDrawPath (context, kCGPathStroke);
		releaseCGContext (context);
	}
	#else
	CGrafPtr OrigPort;
	GDHandle OrigDevice;
	GetGWorld (&OrigPort, &OrigDevice);
	SetGWorld (getPort (), NULL);
	RGBColor col;
	CColor2RGBColor (frameColor, col);
	RGBForeColor (&col);
	MoveTo (pPoints[0].h, pPoints[0].v);
	for (long i = 1; i < numberOfPoints; i++)
		LineTo (pPoints[i].h + offset.h, pPoints[i].v + offset.v);
	SetGWorld (OrigPort, OrigDevice);
	#endif

#elif MOTIF
	XPoint* pt = (XPoint*)malloc (numberOfPoints * sizeof (XPoint));
	if (!pt)
		return;
	for (long i = 0; i < numberOfPoints; i++)
	{
		pt[i].x = (short)pPoints[i].h + offset.h;
		pt[i].y = (short)pPoints[i].v + offset.v;
	}
	
	XDrawLines (XDRAWPARAM, pt, numberOfPoints, CoordModeOrigin);

	free (pt);

#elif BEOS
	rgb_color c = { frameColor.red, frameColor.green, frameColor.blue, 255 };
	pView->SetHighColor (c);
	pView->SetDrawingMode (modeToPlatform [drawMode]);
	pView->SetPenSize (frameWidth);

	CPoint begin (pPoints[0]);
	begin.offset (offset.h, offset.v);
	CPoint end;
	for (long i = 1; i < numberOfPoints; i++)
	{
		end = pPoints[i];
		end.offset (offset.h, offset.v);
		lineFromTo (begin, end);
		begin = end;
	}
#endif
}

//-----------------------------------------------------------------------------
void CDrawContext::setLineStyle (CLineStyle style)
{
	lineStyle = style;

#if WINDOWS
	switch (lineStyle) 
	{
	case kLineOnOffDash: 
		iPenStyle = PS_DOT;
		break;
	default:
		iPenStyle = PS_SOLID;
		break;
	}
	
	LOGPEN logPen = {iPenStyle, {frameWidth, frameWidth}, 
					 RGB (frameColor.red, frameColor.green, frameColor.blue)};
	
	HANDLE newPen = CreatePenIndirect (&logPen);
	SelectObject ((HDC)pSystemContext, newPen);
	if (pPen)
		DeleteObject (pPen);
	pPen = newPen;

#elif MAC
	#if QUARTZ
	if (gCGContext)
	{
		float offset = 0;
		float dotf[2] = { 0.5, 0.5 };
		if (lineStyle == kLineOnOffDash)
		{
			dotf[0] = .5f;
			dotf[1] = 1.5f;
		}
		CGContextSetLineDash (gCGContext, offset, dotf, 2);
	}
	#else
	CGrafPtr OrigPort;
	GDHandle OrigDevice;
	if (pWindow)
	{
		GetGWorld (&OrigPort, &OrigDevice);
		SetGWorld (getPort (), NULL);
		PenState penState;
		GetPenState (&penState);
		switch (lineStyle) 
		{
		case kLineOnOffDash:
			StuffHex (&penState.pnPat, "\pF0F0F0F00F0F0F0F"); // dashed line 4 pixel
			break;
		default:
			StuffHex (&penState.pnPat, "\pFFFFFFFFFFFFFFFF");
			break;
		}
		SetPenState (&penState);
		SetGWorld (OrigPort, OrigDevice);
	}
	#endif

#elif MOTIF
	long line_width;
	long line_style;
	if (frameWidth == 1)
		line_width = 0;
	else 
		line_width = frameWidth;

	switch (lineStyle)
	{
	case kLineOnOffDash:
		line_style = LineOnOffDash;
		break;
	default:
		line_style = LineSolid;
		break;
	}
	
	XSetLineAttributes (XGCPARAM, line_width, line_style, CapNotLast, JoinRound);
#endif
}

//-----------------------------------------------------------------------------
void CDrawContext::setLineWidth (long width)
{
	frameWidth = width;
	
#if WINDOWS
	LOGPEN logPen = {iPenStyle, {frameWidth, frameWidth},
					 RGB (frameColor.red, frameColor.green, frameColor.blue)};
	
	HANDLE newPen = CreatePenIndirect (&logPen);
	SelectObject ((HDC)pSystemContext, newPen);
	if (pPen)
		DeleteObject (pPen);
	pPen = newPen;
	
#elif MAC
	#if QUARTZ
	if (gCGContext)
		CGContextSetLineWidth (gCGContext, width);
	#else
	CGrafPtr OrigPort;
	GDHandle OrigDevice;
	if (pWindow)
	{
		GetGWorld (&OrigPort, &OrigDevice);
		SetGWorld (getPort (), NULL);
		PenState penState;
		GetPenState (&penState);
		penState.pnSize.h = width;
		penState.pnSize.v = width;
		SetPenState (&penState);
		SetGWorld (OrigPort, OrigDevice);
	}
	#endif
#elif MOTIF
	setLineStyle (lineStyle);
#endif
}

//-----------------------------------------------------------------------------
void CDrawContext::setDrawMode (CDrawMode mode)
{
	if (drawMode == mode)
		return;

	drawMode = mode;

#if WINDOWS
	long iMode = 0;
	switch (drawMode) 
	{
	case kXorMode :
		iMode = R2_NOTXORPEN; // Pixel is the inverse of the R2_XORPEN color (final pixel = ~ (pen ^ screen pixel)).
		break;
	case kOrMode :
		iMode = R2_MERGEPEN; // Pixel is a combination of the pen color and the screen color (final pixel = pen | screen pixel).
		break;
	default:
		iMode = R2_COPYPEN;
		break;
	}
	SetROP2 ((HDC)pSystemContext, iMode);

#elif MAC
	#if QUARTZ
	// quartz only support antialias
	if (gCGContext)
			CGContextSetShouldAntialias (gCGContext, drawMode == kAntialias ? true : false);

	#else
	if (pWindow)
	{
		CGrafPtr OrigPort;
		GDHandle OrigDevice;
		GetGWorld (&OrigPort, &OrigDevice);
		SetGWorld (getPort (), NULL);
		long iMode = 0;

		switch (drawMode) 
		{
		case kXorMode :
			iMode = patXor;
			break;
		case kOrMode :
			iMode = patOr;
			break;
		default:
			iMode = patCopy;
		}
		PenMode (mode);
		
		SetGWorld (OrigPort, OrigDevice);
	}
	#endif

#elif MOTIF
	long iMode = 0;
	switch (drawMode) 
	{
	case kXorMode :
		iMode = GXinvert;
		break;
	case kOrMode :
		iMode = GXor;
		break;
	default:
		iMode = GXcopy;
	}
	((XGCValues*)pSystemContext)->function = iMode;
	XChangeGC (XGCPARAM, GCFunction, (XGCValues*)pSystemContext);
#endif
}

//------------------------------------------------------------------------------
void CDrawContext::setClipRect (const CRect &clip)
{
	CRect _clip (clip);
	_clip.offset (offset.h, offset.v);

	if (clipRect == _clip)
		return;

	clipRect = _clip;

#if MAC
	#if QUARTZ
	if (0 && gCGContext)
	{
		CGContextRestoreGState (gCGContext);
		CGContextSaveGState (gCGContext);
		CGContextScaleCTM (gCGContext, 1, -1);
		CGRect cgClipRect = CGRectMake (clipRect.left, clipRect.top, clipRect.width (), clipRect.height ());
		CGContextClipToRect (gCGContext, cgClipRect);
		CGContextScaleCTM (gCGContext, 1, -1);
		setLineWidth (frameWidth);
		setLineStyle (lineStyle);
		setFrameColor (frameColor);
		setFillColor (fillColor);
		setFont (fontId, fontSize);
		setDrawMode (drawMode);
	}
	
	#else
	Rect r;
	CRect2Rect (clip, r);

	CGrafPtr OrigPort;
	GDHandle OrigDevice;
	GetGWorld (&OrigPort, &OrigDevice);
	SetGWorld (getPort (), NULL);
	ClipRect (&r);
	SetGWorld (OrigPort, OrigDevice);
	#endif

#elif WINDOWS
	RECT r = {clipRect.left, clipRect.top, clipRect.right, clipRect.bottom};
	HRGN hRgn  = CreateRectRgn (r.left, r.top, r.right, r.bottom);
	SelectClipRgn ((HDC)pSystemContext, hRgn);
	DeleteObject (hRgn);

#elif MOTIF
	XRectangle r;
	r.x = 0;
	r.y = 0;
	r.width  = clipRect.right - clipRect.left;
	r.height = clipRect.bottom - clipRect.top;
	XSetClipRectangles (XGCPARAM, clipRect.left, clipRect.top, &r, 1, Unsorted); 

#elif BEOS
	clipping_rect r = {clipRect.left, clipRect.top, clipRect.right - 1, clipRect.bottom - 1};	
	BRegion region;
	region.Set (r);
	pView->ConstrainClippingRegion (&region);
#endif
}

//------------------------------------------------------------------------------
void CDrawContext::resetClipRect ()
{
	CRect newClip;
	if (pFrame)
		pFrame->getViewSize (newClip);
	else
		newClip (0, 0, 1000, 1000);

#if (MAC && QUARTZ)
	if (0 && gCGContext)
	{
		CGContextRestoreGState (gCGContext);
		CGContextScaleCTM (gCGContext, 1, -1);
		CGRect cgClipRect = CGRectMake (newClip.left, newClip.top, newClip.width (), newClip.height ());
		CGContextClipToRect (gCGContext, cgClipRect);
		CGContextScaleCTM (gCGContext, 1, -1);
		CGContextSaveGState (gCGContext);
		setLineWidth (frameWidth);
		setLineStyle (lineStyle);
		setFrameColor (frameColor);
		setFillColor (fillColor);
		setFont (fontId, fontSize);
		setDrawMode (drawMode);
	}

#elif MAC || WINDOWS || MOTIF
	setClipRect (newClip);

#elif BEOS
	pView->ConstrainClippingRegion (NULL);
#endif

	clipRect = newClip;
}

//-----------------------------------------------------------------------------
void CDrawContext::fillPolygon (const CPoint *pPoints, long numberOfPoints)
{
	// Don't draw boundary
#if WINDOWS
	POINT points[30];
	POINT *polyPoints;
	bool allocated = false;

	if (numberOfPoints > 30)
	{
		polyPoints = (POINT*)new char [numberOfPoints * sizeof (POINT)];
		if (!polyPoints)
			return;
		allocated = true;
	}
	else
		polyPoints = points;

	for (long i = 0; i < numberOfPoints; i++)
	{
		polyPoints[i].x = pPoints[i].h + offset.h;
		polyPoints[i].y = pPoints[i].v + offset.v;
	}

	HANDLE nullPen = GetStockObject (NULL_PEN);
	HANDLE oldPen  = SelectObject ((HDC)pSystemContext, nullPen);
	Polygon ((HDC)pSystemContext, polyPoints, numberOfPoints);
	SelectObject ((HDC)pSystemContext, oldPen);

	if (allocated)
		delete[] polyPoints;

#elif MAC
	#if QUARTZ
	CGContextRef context = beginCGContext ();
	{
		CGContextScaleCTM (context, 1, -1);

		CGRect cgClipRect = CGRectMake (clipRect.left, clipRect.top, clipRect.width (), clipRect.height ());
		CGContextClipToRect (gCGContext, cgClipRect);

		CGContextTranslateCTM (context, 0.5f, 0.5f);

		CGContextBeginPath (context);
		CGContextMoveToPoint (context, pPoints[0].h + offset.h, pPoints[0].v + offset.v);
		for (long i = 1; i < numberOfPoints; i++)
			CGContextAddLineToPoint (context, pPoints[i].h + offset.h, pPoints[i].v + offset.v);
		CGContextClosePath (context);
		CGContextDrawPath (context, kCGPathFill);
		releaseCGContext (context);
	}
	#else
	CGrafPtr   OrigPort;
	GDHandle   OrigDevice;
	PolyHandle thePoly;
	RGBColor	col;
	
	GetGWorld (&OrigPort, &OrigDevice);
	SetGWorld (getPort (), NULL);
	CColor2RGBColor (fillColor, col);
	RGBForeColor (&col);
	thePoly = OpenPoly ();              // start recording 
	polyLine (pPoints, numberOfPoints); // draw polygon
	LineTo (pPoints[0].h + offset.h, pPoints[0].v + offset.v);  // close the boundary
	ClosePoly ();                       // stop recording 
	
	PixPatHandle pixPatHandle = NewPixPat ();
	CColor2RGBColor (fillColor, col);
	MakeRGBPat (pixPatHandle, &col);    // create pixel pattern with fill color
	
	FillCPoly (thePoly, pixPatHandle);  // fill inside 
	KillPoly (thePoly);                 // deallocate all memory used here 
	DisposePixPat (pixPatHandle);
	SetGWorld (OrigPort, OrigDevice);
	#endif
	
#elif MOTIF
	// convert the points
	XPoint* pt = (XPoint*)malloc (numberOfPoints * sizeof (XPoint));
	for (long i = 0; i < numberOfPoints; i++)
	{
		pt[i].x = (short)pPoints[i].h + offset.h;
		pt[i].y = (short)pPoints[i].v + offset.v;
	}
	
	XFillPolygon (XDRAWPARAM, pt, numberOfPoints, Convex, CoordModeOrigin);

	free (pt);

#elif BEOS
	BPoint bpoints[30];
	BPoint* polyPoints;
	bool allocated = false;
	
	if (numberOfPoints > 30)
	{
		polyPoints = new BPoint [numberOfPoints];
		if (!polyPoints)
			return;
		allocated = true;
	}	
	else
		polyPoints = bpoints;
			
	for (long i = 0; i < numberOfPoints; i++)
		polyPoints[i].Set (pPoints[i].h + offset.h, pPoints[i].v + offset.v);

	rgb_color c = { fillColor.red, fillColor.green, fillColor.blue, 255 };
	pView->SetHighColor (c);
	pView->SetDrawingMode (modeToPlatform [drawMode]);
	pView->FillPolygon (polyPoints, numberOfPoints);

	if (allocated)
		delete[] polyPoints;
#endif
}

//-----------------------------------------------------------------------------
void CDrawContext::drawRect (const CRect &_rect)
{
	CRect rect (_rect);
	rect.offset (offset.h, offset.v);

#if WINDOWS
	MoveToEx ((HDC)pSystemContext, rect.left, rect.top, NULL);
	LineTo ((HDC)pSystemContext, rect.right, rect.top);
	LineTo ((HDC)pSystemContext, rect.right, rect.bottom);
	LineTo ((HDC)pSystemContext, rect.left, rect.bottom);
	LineTo ((HDC)pSystemContext, rect.left, rect.top);
	
#elif MAC
	#if QUARTZ
	CGContextRef context = beginCGContext ();
	{
		if (drawMode == kAntialias)
			CGContextSetLineWidth (context, 2 * frameWidth);
		CGRect r = CGRectMake (rect.left, rect.top, rect.width (), rect.height ());
		CGContextScaleCTM (context, 1, -1);

		CGRect cgClipRect = CGRectMake (clipRect.left, clipRect.top, clipRect.width (), clipRect.height ());
		CGContextClipToRect (gCGContext, cgClipRect);

		CGContextTranslateCTM (context, 0.5f, 0.5f);

		CGContextStrokeRect (context, r);
		releaseCGContext (context);
	}
	#else
	CGrafPtr OrigPort;
	GDHandle OrigDevice;
	GetGWorld (&OrigPort, &OrigDevice);	// get current GrafPort
	SetGWorld (getPort (), NULL);       // activate our GWorld
	RGBColor col;
	CColor2RGBColor (frameColor, col);
	RGBForeColor (&col);
	MoveTo (rect.left, rect.top);
	LineTo (rect.right, rect.top);
	LineTo (rect.right, rect.bottom);
	LineTo (rect.left, rect.bottom);
	LineTo (rect.left, rect.top);
	SetGWorld (OrigPort, OrigDevice);
	#endif

#elif MOTIF
	XDrawRectangle (XDRAWPARAM, rect.left, rect.top, rect.width (), rect.height ());

#elif BEOS
	rgb_color c = { frameColor.red, frameColor.green, frameColor.blue, 255 };
	pView->SetHighColor (c);
	pView->SetDrawingMode (modeToPlatform [drawMode]);
	BRect r (rect.left, rect.top, rect.right, rect.bottom);
	pView->SetPenSize (frameWidth);
	pView->StrokeRect (r);

#endif
}

//-----------------------------------------------------------------------------
void CDrawContext::fillRect (const CRect &_rect)
{
	CRect rect (_rect);
	rect.offset (offset.h, offset.v);

	// Don't draw boundary
#if WINDOWS
	RECT wr = {rect.left + 1, rect.top + 1, rect.right, rect.bottom};
	HANDLE nullPen = GetStockObject (NULL_PEN);
	HANDLE oldPen  = SelectObject ((HDC)pSystemContext, nullPen);
	FillRect ((HDC)pSystemContext, &wr, (HBRUSH)pBrush);
	SelectObject ((HDC)pSystemContext, oldPen);

#elif MAC
	#if QUARTZ
	CGContextRef context = beginCGContext ();
	{
		CGRect r = CGRectMake (rect.left, rect.top, rect.width (), rect.height ());
		CGContextScaleCTM (context, 1, -1);

		CGRect cgClipRect = CGRectMake (clipRect.left, clipRect.top, clipRect.width (), clipRect.height ());
		CGContextClipToRect (gCGContext, cgClipRect);

		CGContextTranslateCTM (context, 0.5f, 0.5f);

		CGContextFillRect (context, r);
		releaseCGContext (context);
	}
	#else
	Rect     rr;
	CGrafPtr OrigPort;
	GDHandle OrigDevice;
	GetGWorld (&OrigPort, &OrigDevice);
	SetGWorld (getPort (), NULL);
	RGBColor col;
	CColor2RGBColor (fillColor, col);
	RGBForeColor (&col);
	CRect2Rect (rect, rr);
	rr.left++;
	rr.top++;
	FillRect (&rr, &fillPattern);
	SetGWorld (OrigPort, OrigDevice);
	#endif

#elif MOTIF
	XFillRectangle (XDRAWPARAM, rect.left + 1, rect.top + 1, rect.width () - 1, rect.height () - 1);

#elif BEOS
	rgb_color c = { fillColor.red, fillColor.green, fillColor.blue, 255 };
	pView->SetHighColor (c);
	pView->SetDrawingMode (modeToPlatform [drawMode]);
	BRect r (rect.left + 1, rect.top + 1, rect.right - 1, rect.bottom - 1);
	pView->FillRect (r);
#endif
}

//-----------------------------------------------------------------------------
void CDrawContext::drawEllipse (const CRect &_rect)
{
	#if QUARTZ
	CRect rect (_rect);
	rect.offset (offset.h, offset.v);

	CGContextRef context = beginCGContext ();
	{
		CGContextScaleCTM (context, 1, -1);

		CGRect cgClipRect = CGRectMake (clipRect.left, clipRect.top, clipRect.width (), clipRect.height ());
		CGContextClipToRect (gCGContext, cgClipRect);

		CGContextTranslateCTM (context, 0.5f, 0.5f);

		CGContextSaveGState (context);
		CGContextBeginPath (context);

		CGRect cgRect = CGRectMake (rect.left, rect.top, rect.width (), rect.height ());
		CGPoint center = CGPointMake (CGRectGetMidX (cgRect), CGRectGetMidY (cgRect));
		float a = CGRectGetWidth (cgRect) / 2;
		float b = CGRectGetHeight (cgRect) / 2;

	    CGContextTranslateCTM (context, center.x, center.y);
	    CGContextScaleCTM (context, a, b);
	    CGContextMoveToPoint (context, 1, 0);
	    CGContextAddArc (context, 0, 0, 1, radians (0), radians (360), 0);

		CGContextClosePath (context);
		CGContextRestoreGState (context);
		CGContextDrawPath (context, kCGPathStroke);
		releaseCGContext (context);
	}

	#else
	CPoint point (_rect.left + (_rect.right - _rect.left) / 2, _rect.top);
	drawArc (_rect, point, point);
	#endif
}

//-----------------------------------------------------------------------------
void CDrawContext::fillEllipse (const CRect &_rect)
{
	CRect rect (_rect);
	rect.offset (offset.h, offset.v);

	// Don't draw boundary
#if WINDOWS
	HANDLE nullPen = GetStockObject (NULL_PEN);
	HANDLE oldPen  = SelectObject ((HDC)pSystemContext, nullPen);
	Ellipse ((HDC)pSystemContext, rect.left + 1, rect.top + 1, rect.right + 1, rect.bottom + 1);
	SelectObject ((HDC)pSystemContext, oldPen);

#elif QUARTZ
	CGContextRef context = beginCGContext ();
	{
		CGContextScaleCTM (context, 1, -1);

		CGRect cgClipRect = CGRectMake (clipRect.left, clipRect.top, clipRect.width (), clipRect.height ());
		CGContextClipToRect (gCGContext, cgClipRect);

		CGContextTranslateCTM (context, 0.5f, 0.5f);

		CGContextSaveGState (context);
		CGContextBeginPath (context);

		CGRect cgRect = CGRectMake (rect.left, rect.top, rect.width (), rect.height ());
		CGPoint center = CGPointMake (CGRectGetMidX (cgRect), CGRectGetMidY (cgRect));
		float a = CGRectGetWidth (cgRect) / 2;
		float b = CGRectGetHeight (cgRect) / 2;

	    CGContextTranslateCTM (context, center.x, center.y);
	    CGContextScaleCTM (context, a, b);
	    CGContextMoveToPoint (context, 1, 0);
	    CGContextAddArc (context, 0, 0, 1, radians (0), radians (360), 0);

		CGContextClosePath (context);
		CGContextRestoreGState (context);
		CGContextDrawPath (context, kCGPathFill);
		releaseCGContext (context);
	}

#else
	CPoint point (_rect.left + ((_rect.right - _rect.left) / 2), _rect.top);
	fillArc (_rect, point, point);
#endif
}

//-----------------------------------------------------------------------------
void CDrawContext::drawPoint (const CPoint &_point, CColor color)
{
	CPoint point (_point);
	point.offset (offset.h, offset.v);

#if WINDOWS
	SetPixel ((HDC)pSystemContext, point.h, point.v, RGB(color.red, color.green, color.blue));

#elif MOTIF
	CColor oldframecolor = frameColor;
	setFrameColor (color);
	XDrawPoint (XDRAWPARAM, point.h, point.v);
	setFrameColor (oldframecolor);

#elif MAC
	int oldframeWidth = frameWidth;
	CColor oldframecolor = frameColor;
	setLineWidth (1);
	setFrameColor (color);
	CPoint point2 (point);
	point2.h++;
	moveTo (point);
	lineTo (point2);
	
	setFrameColor (oldframecolor);
	setLineWidth (oldframeWidth);

#else
	int oldframeWidth = frameWidth;
	CColor oldframecolor = frameColor;
	setLineWidth (1);
	setFrameColor (color);
	moveTo (point);
	lineTo (point);
	
	setFrameColor (oldframecolor);
	setLineWidth (oldframeWidth);
#endif
}

//-----------------------------------------------------------------------------
CColor CDrawContext::getPoint (const CPoint& _point)
{
	CPoint point (_point);
	point.offset (offset.h, offset.v);
	CColor color = kBlackCColor;

	#if WINDOWS
	COLORREF c  = GetPixel ((HDC)pSystemContext, point.h, point.v);
	color.red   = GetRValue (c);
	color.green = GetGValue (c);
	color.blue  = GetBValue (c);

	#elif MAC
	#if QUARTZ
	// no quartz equivalent
	
	#else
	CGrafPtr OrigPort;
	GDHandle OrigDevice;
	GetGWorld (&OrigPort, &OrigDevice);
	SetGWorld (getPort (), NULL);
	RGBColor cPix;
	GetCPixel (point.h, point.v, &cPix);
	RGBColor2CColor (cPix, color);
	SetGWorld (OrigPort, OrigDevice);
	#endif
	#endif

	return color;
}

//-----------------------------------------------------------------------------
void CDrawContext::floodFill (const CPoint& _start)
{
	CPoint start (_start);
	start.offset (offset.h, offset.v);

	#if WINDOWS
	COLORREF c = GetPixel ((HDC)pSystemContext, start.h, start.v);
	ExtFloodFill ((HDC)pSystemContext, start.h, start.v, c, FLOODFILLSURFACE);
	
	#elif MAC
	#if QUARTZ
	// no quartz equivalent
	
	#else
	CGrafPtr oldPort;
	GDHandle oldDevice;
	GetGWorld (&oldPort, &oldDevice);
	SetGWorld (getPort (), 0);
	
	Rect r;
	GetPortBounds (getPort (), &r);
	GWorldPtr pMask;
	OSErr err = NewGWorld ((GWorldPtr*)&pMask, 1, &r, 0, 0, 0); // create monochrome GWorld
	if (!err)
	{
		// generate fill mask
		PixMapHandle srcBits = GetGWorldPixMap (getPort ());
		PixMapHandle dstBits = GetGWorldPixMap (pMask);
		if (srcBits && dstBits)
		{
			LockPixels (srcBits);
			LockPixels (dstBits);
		
			SeedCFill ((BitMapPtr)*srcBits, (BitMapPtr)*dstBits, &r, &r, start.h, start.v, 0, 0);

			// fill destination
			RGBColor oldForeColor, oldBackColor;
			GetForeColor (&oldForeColor);
			GetBackColor (&oldBackColor);
		
			::BackColor (whiteColor);
		
			RGBColor col;
			CColor2RGBColor (fillColor, col);
			RGBForeColor (&col);		
		
			CopyMask ((BitMapPtr)*dstBits, (BitMapPtr)*dstBits, (BitMapPtr)*srcBits, &r, &r, &r);
		
			RGBForeColor (&oldForeColor);
			RGBBackColor (&oldBackColor);

			// cleanup
			UnlockPixels (srcBits);
			UnlockPixels (dstBits);
		}
		
		DisposeGWorld (pMask);
	}

	SetGWorld (oldPort, oldDevice);
	#endif
	#endif
}

#if QUARTZ
void addOvalToPath(CGContextRef c, CPoint center, float a, float b, float start_angle, float arc_angle, int pie)
{
	float CGstart_angle = start_angle;
	CGContextSaveGState (c);
	CGContextTranslateCTM (c, center.x, center.y);
	CGContextScaleCTM (c, a, b);
	if (pie) 
		CGContextMoveToPoint (c, 0, 0);
	else 
		CGContextMoveToPoint (c, cos (radians (CGstart_angle)), sin (radians (CGstart_angle)));
	long dir = arc_angle > 0 ? 1 : 0;
	CGContextAddArc(c, 0, 0, 1, radians (CGstart_angle), radians (CGstart_angle - arc_angle), dir);
	CGContextAddArc(c, 0, 0, 1, radians (CGstart_angle - arc_angle), radians (CGstart_angle), dir ? 0 : 1);
	if (pie)
	{
		CGContextClosePath(c);
	}
	CGContextRestoreGState(c);
}
#endif

//-----------------------------------------------------------------------------
void CDrawContext::drawArc (const CRect &_rect, const CPoint &_point1, const CPoint &_point2)
{
	CRect rect (_rect);
	rect.offset (offset.h, offset.v);
	CPoint point1 (_point1);
	point1.offset (offset.h, offset.v);
	CPoint point2 (_point2);
	point2.offset (offset.h, offset.v);

	// draws from point1 to point2 counterclockwise
#if WINDOWS

	Arc ((HDC)pSystemContext, rect.left, rect.top, rect.right + 1, rect.bottom + 1, 
			 point1.h, point1.v, point2.h, point2.v);

#elif MAC || MOTIF || BEOS
	
	int	angle1, angle2;
	if ((point1.v == point2.v) && (point1.h == point2.h))
	{
		angle1 = 0;
		angle2 = 23040; // 360 * 64
	}
	else
	{
		CPoint pm ((rect.left + rect.right) / 2, (rect.top + rect.bottom) / 2);
		angle1 = convertPoint2Angle (pm, point1);
		angle2 = convertPoint2Angle (pm, point2) - angle1;
		if (angle2 < 0)
			angle2 += 23040; // 360 * 64
	}

#if MAC

	#if QUARTZ
	angle1 /= 64;
	angle2 /= 64;
	CGContextRef context = beginCGContext ();
	{	// someone who uses this shoud check if this is correct
		CGContextScaleCTM (context, 1, -1);

		CGRect cgClipRect = CGRectMake (clipRect.left, clipRect.top, clipRect.width (), clipRect.height ());
		CGContextClipToRect (gCGContext, cgClipRect);

		CGContextTranslateCTM (context, 0.5f, 0.5f);

		CGContextBeginPath (context);
		addOvalToPath (context, CPoint (rect.left + rect.width () / 2, rect.top + rect.height () / 2), rect.width () / 2, rect.height () / 2, angle1, angle2, 0);
		CGContextClosePath (context);
		CGContextDrawPath (context, kCGPathStroke);
		releaseCGContext (context);
	}
	#else
	Rect     rr;
	CGrafPtr OrigPort;
	GDHandle OrigDevice;
	GetGWorld (&OrigPort, &OrigDevice);
	SetGWorld (getPort (), NULL);
	RGBColor col;
	CColor2RGBColor (frameColor, col);
	RGBForeColor (&col);
	CRect2Rect (rect, rr);
	FrameArc (&rr, 90 - (angle1 / 64), -angle2 / 64);
	SetGWorld (OrigPort, OrigDevice);
	#endif
	        
#elif MOTIF
	XDrawArc (XDRAWPARAM, rect.left, rect.top, rect.width (), rect.height (),
						angle1, angle2);

#elif BEOS
	rgb_color c = { frameColor.red, frameColor.green, frameColor.blue, 255 };
	pView->SetHighColor (c);
	pView->SetDrawingMode (modeToPlatform [drawMode]);
	BRect r (rect.left, rect.top, rect.right, rect.bottom);
	pView->SetPenSize (frameWidth);
	pView->StrokeArc (r, angle1 / 64, angle2 / 64);
#endif	

#endif
}

//-----------------------------------------------------------------------------
void CDrawContext::fillArc (const CRect &_rect, const CPoint &_point1, const CPoint &_point2)
{
	CRect rect (_rect);
	rect.offset (offset.h, offset.v);
	CPoint point1 (_point1);
	point1.offset (offset.h, offset.v);
	CPoint point2 (_point2);
	point2.offset (offset.h, offset.v);

	// Don't draw boundary
#if WINDOWS
	HANDLE nullPen = GetStockObject (NULL_PEN);
	HANDLE oldPen  = SelectObject ((HDC)pSystemContext, nullPen);
	Pie ((HDC)pSystemContext, offset.h + rect.left + 1, offset.v + rect.top + 1, offset.h + rect.right, offset.v + rect.bottom, 
			 point1.h, point1.v, point2.h, point2.v);
	SelectObject ((HDC)pSystemContext, oldPen);

#elif MAC || MOTIF || BEOS
	
	int	angle1, angle2;
	if ((point1.v == point2.v) && (point1.h == point2.h))
	{
		angle1 = 0;
		angle2 = 23040; // 360 * 64
	}
	else
	{
		CPoint pm ((rect.left + rect.right) / 2, (rect.top + rect.bottom) / 2);
		angle1 = convertPoint2Angle (pm, point1);
		angle2 = convertPoint2Angle (pm, point2);
	}

#if MAC
	#if QUARTZ
	// check drawArc and implement it here
	
	#else
	Rect     rr;
	CGrafPtr OrigPort;
	GDHandle OrigDevice;
	GetGWorld (&OrigPort, &OrigDevice);
	SetGWorld (getPort (), NULL);
	RGBColor col;
	CColor2RGBColor (fillColor, col);
	RGBForeColor (&col);
	CRect2Rect (rect, rr);
	
	angle2 = angle2 - angle1;
	if (angle2 < 0)
		angle2 = -angle2;
	FillArc (&rr, 90 - (angle1 / 64), -angle2 / 64, &fillPattern);

	SetGWorld (OrigPort, OrigDevice);
	#endif
        
#elif MOTIF
	XFillArc (XDRAWPARAM, rect.left, rect.top, rect.width (), rect.height (),
				angle1, angle2);

#elif BEOS
	rgb_color c = { fillColor.red, fillColor.green, fillColor.blue, 255 };
	pView->SetHighColor (c);
	pView->SetDrawingMode (modeToPlatform [drawMode]);
	BRect r (rect.left + 1, rect.top + 1, rect.right - 1, rect.bottom - 1);
	pView->FillArc (r, angle1 / 64, angle2 / 64);

#endif
#endif
}

//-----------------------------------------------------------------------------
void CDrawContext::setFontColor (const CColor color)
{
	fontColor = color;
	
#if WINDOWS
	SetTextColor ((HDC)pSystemContext, RGB (fontColor.red, fontColor.green, fontColor.blue));
	
#elif MAC
	#if QUARTZ
	// on quartz the fill color is the font color

	#else
	RGBColor col;
	CGrafPtr OrigPort;
	GDHandle OrigDevice;
	if (pWindow)
	{
		GetGWorld (&OrigPort, &OrigDevice);
		SetGWorld (getPort (), NULL);
		CColor2RGBColor (fontColor, col);
		RGBForeColor (&col);
		SetGWorld (OrigPort, OrigDevice);
	}
	#endif
        
#elif MOTIF
	setFrameColor (fontColor);

#endif
}

//-----------------------------------------------------------------------------
void CDrawContext::setFrameColor (const CColor color)
{
	frameColor = color;

#if WINDOWS
	LOGPEN logPen = {iPenStyle, {frameWidth, frameWidth}, 
					 RGB (frameColor.red, frameColor.green, frameColor.blue)};
	
	HANDLE newPen = CreatePenIndirect (&logPen);
	SelectObject ((HDC)pSystemContext, newPen);
	if (pPen)
		DeleteObject (pPen);
	pPen = newPen;

#elif MAC
	#if QUARTZ
	if (gCGContext)
		CGContextSetRGBStrokeColor (gCGContext, color.red/255.f, color.green/255.f, color.blue/255.f, color.alpha/255.f);
	#else
	RGBColor col;
	CGrafPtr OrigPort;
	GDHandle OrigDevice;
	if (pWindow)
	{
		GetGWorld (&OrigPort, &OrigDevice);
		SetGWorld (getPort (), NULL);
		CColor2RGBColor (frameColor, col);
		RGBForeColor (&col);
		SetGWorld (OrigPort, OrigDevice);
	}
	#endif
        
#elif MOTIF
	XSetForeground (XGCPARAM, getIndexColor (frameColor));
#endif
}

//-----------------------------------------------------------------------------
void CDrawContext::setFillColor (const CColor color)
{
	fillColor = color;

#if WINDOWS
 	SetBkColor ((HDC)pSystemContext, RGB (color.red, color.green, color.blue));
	LOGBRUSH logBrush = {BS_SOLID, RGB (color.red, color.green, color.blue), 0 };
	HANDLE newBrush = CreateBrushIndirect (&logBrush);
	if (newBrush == 0)
	{
		DWORD err = GetLastError ();
		return;
	}
	SelectObject ((HDC)pSystemContext, newBrush);
	if (pBrush)
		DeleteObject (pBrush);
	pBrush = newBrush;
	
#elif MAC
	#if QUARTZ
	if (gCGContext)
		CGContextSetRGBFillColor (gCGContext, color.red/255.f, color.green/255.f, color.blue/255.f, color.alpha/255.f);
	#else
	RGBColor col;
	CGrafPtr OrigPort;
	GDHandle OrigDevice;
	if (pWindow)
	{
		GetGWorld (&OrigPort, &OrigDevice);
		SetGWorld (getPort (), NULL);
		CColor2RGBColor (fillColor, col);
		RGBForeColor (&col);
		SetGWorld (OrigPort, OrigDevice);
	}
	#endif
        
#elif MOTIF
	// set the background for the text
	XSetBackground (XGCPARAM, getIndexColor (fillColor));
	
	// set the foreground for the fill
	setFrameColor (fillColor);
#endif
}

//-----------------------------------------------------------------------------
void CDrawContext::setFont (CFont fontID, const long size, long style)
{
	if (fontID < 0 || fontID >= kNumStandardFonts)
		fontID = kSystemFont;

	fontId = fontID;
	if (size != 0)
		fontSize = size;
	else
		fontSize = gStandardFontSize[fontID];

#if WINDOWS
	LOGFONT logfont = {0};

	if (style & kBoldFace)
		logfont.lfWeight = FW_BOLD;
	else
		logfont.lfWeight = FW_NORMAL;
	if (style & kItalicFace)
		logfont.lfItalic = true;
	if (style & kUnderlineFace)
		logfont.lfUnderline = true;
	
	logfont.lfHeight         = -fontSize;
	logfont.lfPitchAndFamily = VARIABLE_PITCH | FF_SWISS;
	strcpy (logfont.lfFaceName, gStandardFontName[fontID]);

	if (fontID == kSymbolFont)
		logfont.lfPitchAndFamily = DEFAULT_PITCH | FF_DECORATIVE;
	else if (fontID == kSystemFont)
		logfont.lfWeight     = FW_BOLD;
  
	logfont.lfClipPrecision = CLIP_STROKE_PRECIS;
	logfont.lfOutPrecision  = OUT_STRING_PRECIS;
	logfont.lfQuality 	    = DEFAULT_QUALITY;
	logfont.lfCharSet       = ANSI_CHARSET;

	HANDLE newFont = CreateFontIndirect (&logfont);
	SelectObject ((HDC)pSystemContext, newFont);
	if (pFont)
		DeleteObject (pFont);
	pFont = newFont;
  
#elif MAC
	#if QUARTZ
	if (gCGContext)
		CGContextSelectFont (gCGContext, (const char*)gMacXfontNames[fontId], fontSize, kCGEncodingMacRoman);
	#else
	CGrafPtr OrigPort;
	GDHandle OrigDevice;
	if (pWindow)
	{
		GetGWorld (&OrigPort, &OrigDevice);
		SetGWorld (getPort (), NULL);
		
		TextFace (style); // normal, bold, italic, underline...
   		TextMode (0);
		TextSize (fontSize);
		
		#if MACX
		short familyID;
		
		GetFNum (gMacXfontNames[fontID], &familyID);

		TextFont (familyID);
		
		#else
		if (fontID == kSymbolFont)
			TextFont (kFontIDSymbol);
		else if (fontID == kSystemFont)
			TextFont (0);	// system
		else if (fontID == kNormalFontSmaller)
			TextFont (kFontIDGeneva);	// Geneva
		else
			TextFont (kFontIDHelvetica);
		#endif
		
		GetFontInfo (&fontInfoStruct);
		SetGWorld (OrigPort, OrigDevice);
	}
	#endif
        
#elif MOTIF
	XSetFont (XGCPARAM, gFontStructs[fontID]->fid);
	
	// keep trace of the current font
	pFontInfoStruct = gFontStructs[fontID];

#elif BEOS
	font.SetFamilyAndStyle (gStandardFontName[fontID], gStandardFontStyle[fontID]);
	font.SetSize (fontSize);
	pView->SetFont (&font, B_FONT_FAMILY_AND_STYLE | B_FONT_SIZE);
#endif
}

//------------------------------------------------------------------------------
long CDrawContext::getStringWidth (const char *pStr)
{
	long result = 0;

	#if MAC
	#if QUARTZ
	CGContextRef context = beginCGContext ();
	if (context)
	{
		CGContextScaleCTM (context, 1, 1);
		CGContextSetTextDrawingMode (context, kCGTextInvisible);
		CGContextSetTextPosition (context, 0.f, 0.f);
		CGContextShowText (context, pStr, strlen (pStr));
		CGPoint p = CGContextGetTextPosition (context);
		result = p.x;
		releaseCGContext (context);
	}
	#else
	CGrafPtr OrigPort;
	GDHandle OrigDevice;
	GetGWorld (&OrigPort, &OrigDevice);
	SetGWorld (getPort (), NULL);

	result = (long)TextWidth (pStr, 0, strlen (pStr));

	SetGWorld (OrigPort, OrigDevice);
	#endif
        
	#elif WINDOWS
	SIZE size;
	GetTextExtentPoint32 ((HDC)pSystemContext, pStr, (int)strlen (pStr), &size);
	result = (long)size.cx;

	#elif MOTIF
	result = (long)XTextWidth (pFontInfoStruct, pStr, strlen (pStr));
	
	#elif BEOS
	result = (long)(ceil (pView->StringWidth (pStr)));
	#endif

	return result;
}

//-----------------------------------------------------------------------------
void CDrawContext::drawString (const char *string, const CRect &_rect,
							 const short opaque, const CHoriTxtAlign hAlign)
{
	if (!string)
		return;
	
	CRect rect (_rect);
	rect.offset (offset.h, offset.v);

#if WINDOWS
	// set the visibility mask
	SetBkMode ((HDC)pSystemContext, opaque ? OPAQUE : TRANSPARENT);

	RECT Rect = {rect.left, rect.top, rect.right, rect.bottom};
	UINT flag = DT_VCENTER + DT_SINGLELINE + DT_NOPREFIX;
	switch (hAlign)
	{
	case kCenterText:
		// without DT_SINGLELINE no vertical center alignment here
		DrawText ((HDC)pSystemContext, string, (int)strlen (string), &Rect, flag + DT_CENTER);
		break;
		
	case kRightText:
		DrawText ((HDC)pSystemContext, string, (int)strlen (string), &Rect, flag + DT_RIGHT);
		break;
		
	default : // left adjust
		Rect.left++;
		DrawText ((HDC)pSystemContext, string, (int)strlen (string), &Rect, flag + DT_LEFT);
	}

	SetBkMode ((HDC)pSystemContext, TRANSPARENT);

#elif MAC
	#if QUARTZ
	CGContextRef context = beginCGContext ();
	if (context)
	{
		long strWidth = getStringWidth (string);
		rect.bottom -= rect.height ()/2 - fontSize / 2 + 1;
		switch (hAlign)
		{
			case kCenterText:
			{
				rect.left += rect.width () / 2 - strWidth/2;
				break;
			}
			case kRightText:
				rect.left = rect.right - strWidth;
				break;
			default : // left adjust
				rect.left++;
		}

		CGContextScaleCTM (context, 1, 1);

		CGRect cgClipRect = CGRectMake (clipRect.left, clipRect.bottom * -1, clipRect.width (), clipRect.height ());
		CGContextClipToRect (gCGContext, cgClipRect);

		CGContextSetShouldAntialias (context, true);
		CGContextSetTextDrawingMode (context, kCGTextFill);
		CGContextSetRGBFillColor (context, fontColor.red/255.f, fontColor.green/255.f, fontColor.blue/255.f, fontColor.alpha/255.f);
		CGContextSetTextPosition (context, rect.left - 0.5f, rect.bottom * -1 + 0.5f);
		CGContextShowText (context, string, strlen (string));
		releaseCGContext (context);
	}

	#else
	CGrafPtr OrigPort;
	GDHandle OrigDevice;
	int width;
	int xPos, yPos;
	int fontHeight;
	int rectHeight;
	int stringLength;

	Rect stringsRect;
	Rect contextsClip;
	Rect compositeClip;
	
	CRect2Rect (rect, stringsRect);
	CRect2Rect (clipRect, contextsClip);
	
	if (SectRect (&stringsRect, &contextsClip, &compositeClip))
	{	
		GetGWorld (&OrigPort, &OrigDevice);
		SetGWorld (getPort (), NULL);
		
		if (opaque)
			TextMode (srcCopy);
		else
			TextMode (srcOr);
		
		RGBColor col;
		CColor2RGBColor (fontColor, col);
		RGBForeColor (&col);

		CColor2RGBColor (fillColor, col);
		RGBBackColor (&col);
		
		rectHeight = rect.height ();
		fontHeight = fontInfoStruct.ascent + fontInfoStruct.descent;
		yPos = rect.bottom - fontInfoStruct.descent;
		if (rectHeight >= fontHeight)
			yPos -= (rectHeight - fontHeight) / 2;
			
		stringLength = strlen (string);
		width = TextWidth ((Ptr)string, 0, stringLength);
	
		switch (hAlign)
		{
		case kCenterText:
			xPos = (rect.right + rect.left - width) / 2;
			break;
			
		case kRightText:
			xPos = rect.right - width;
			break;
			
		default: // left adjust
			xPos = rect.left;
		}
		
		RgnHandle saveRgn = NewRgn ();
		GetClip (saveRgn);
		
		ClipRect (&compositeClip);

		#if CARBON
		CFStringRef str;

		// Create a unicode string
		str = CFStringCreateWithCString(NULL, string, kCFStringEncodingMacRoman);
	
		// Initialize proper text box options
		TXNTextBoxOptionsData myOptions;
		myOptions.optionTags = kTXNSetJustificationMask;
		myOptions.justification = kTXNFlushLeft;

		// Determine the vertical alignment of the text box.
		// It is centered vertically.
		// Somehow, the yPos calculation above doesn't work here
		// or I am too stupid to understand it. Therefore I calculate
		// the text position in the surrounding control rect myself.
		long myHeight = (rect.height () - fontHeight) / 2;
		if (myHeight>0)
		{
			stringsRect.top += myHeight;
			stringsRect.bottom += myHeight;
		}
		stringsRect.left = xPos;
		stringsRect.right = xPos + width;//rect.width ();
	
		// Draw the unicode string
		TXNDrawCFStringTextBox (str, &stringsRect, NULL, &myOptions);

		// Release the unicode string
		CFRelease (str);
		#else
		MoveTo (xPos, yPos);
		DrawText ((Ptr)string, 0, stringLength);
		#endif
		
		SetClip (saveRgn);
		DisposeRgn (saveRgn);
		TextMode (srcOr);
		SetGWorld (OrigPort, OrigDevice);
	}
        #endif
        
#elif MOTIF
	int width;
	int fontHeight = pFontInfoStruct->ascent + pFontInfoStruct->descent;
	int xPos;
	int yPos;
	int rectHeight = rect.height ();

	if (rectHeight >= fontHeight)
		yPos = rect.bottom - (rectHeight - fontHeight) / 2;
	else 
		yPos = rect.bottom;
	yPos -=	pFontInfoStruct->descent;

	switch (hAlign)
	{
	case kCenterText:
		width = XTextWidth (pFontInfoStruct, string, strlen (string));
		xPos = (rect.right + rect.left - width) / 2;
		break;
		
	case kRightText:
		width = XTextWidth (pFontInfoStruct, string, strlen (string));
		xPos = rect.right - width;
		break;
		
	default: // left adjust
		xPos = rect.left + 1;
	}
	
	if (opaque)
		XDrawImageString (XDRAWPARAM, xPos, yPos, string, strlen (string));
	else
		XDrawString (XDRAWPARAM, xPos, yPos, string, strlen (string));

#elif BEOS
	BRect r (rect.left, rect.top, rect.right - 1, rect.bottom - 1);
	BRegion LocalRegion (r);
	pView->ConstrainClippingRegion (&LocalRegion);
	pView->SetFontSize (fontSize);
	float width = -1;
	if (opaque)
	{
		width = ceil (pView->StringWidth (string));
		CRect cr (rect.left, rect.top, rect.left + width, rect.bottom);
		fillRect (cr);
	}
	rgb_color c = { fontColor.red, fontColor.green, fontColor.blue, 255 };
	pView->SetHighColor (c);
	if (drawMode == kXorMode)
		pView->SetDrawingMode (B_OP_INVERT);
	else
		pView->SetDrawingMode (B_OP_OVER);
	BPoint		p;
	font_height	height;
	pView->GetFontHeight (&height);
	p.y = r.bottom - (rect.height () - height.ascent) / 2;
	if (hAlign == kCenterText || hAlign == kRightText)
	{
		if (width < 0)
			width = ceil (pView->StringWidth (string));
		if (hAlign == kCenterText)
			p.x = rect.left + (rect.right - rect.left - width) / 2;
		else
			p.x = rect.right - width - 1;
	}
	else
		p.x = rect.left + 1;
	pView->DrawString (string, p);
	pView->ConstrainClippingRegion (NULL);
#endif
}

//-----------------------------------------------------------------------------
long CDrawContext::getMouseButtons ()
{
	long buttons = 0;
	
#if WINDOWS
	if (GetAsyncKeyState (VK_LBUTTON) < 0)
		buttons |= (bSwapped_mouse_buttons ? kRButton : kLButton);
	if (GetAsyncKeyState (VK_MBUTTON) < 0)
		buttons |= kMButton;
	if (GetAsyncKeyState (VK_RBUTTON) < 0)
		buttons |= (bSwapped_mouse_buttons ? kLButton : kRButton);
	
	if (GetAsyncKeyState (VK_SHIFT)   < 0)
		buttons |= kShift;
	if (GetAsyncKeyState (VK_CONTROL) < 0)
		buttons |= kControl;
	if (GetAsyncKeyState (VK_MENU)    < 0)
		buttons |= kAlt;

#elif MAC
	#if MACX // this works for MacOSX 10.2 and later
	UInt32 state = GetCurrentButtonState ();
	if (state & kEventMouseButtonPrimary)
		buttons |= kLButton;
	if (state & kEventMouseButtonSecondary)
		buttons |= kRButton;
	if (state & 4)//kEventMouseButtonTertiary) this define is false...Apple ?
		buttons |= kMButton;

	state = GetCurrentKeyModifiers ();
	if (state & cmdKey)
		buttons |= kControl;
	if (state & shiftKey)
		buttons |= kShift;
	if (state & optionKey)
		buttons |= kAlt;
	if (state & controlKey)
		buttons |= kApple;
	// for the one buttons
	if (buttons & kApple && buttons & kLButton)
	{
		buttons &= ~(kApple | kLButton);
		buttons |= kRButton;
	}
	#else
	if (Button ())
		buttons |= kLButton;
	
	KeyMap	Keys;
	unsigned char *BytePtr = (unsigned char*)Keys;
	GetKeys (Keys);
	
	if (BytePtr[7] & 1)		// Shift   		0x38 == 56 = (7 * 8) + 0
		buttons |= kShift;
	if (BytePtr[7] & 8)		// Control (extra Mac) 0x3B == 59 = (7 * 8) + 3
		buttons |= kApple;			
	if (BytePtr[7] & 4)		// Alt   		0x3A == 58 = (7 * 8) + 2
		buttons |= kAlt;			
	if (BytePtr[6] & 128)	// Apple => ctrl (PC)  0x37 == 55 = (6 * 8) + 7
		buttons |= kControl;
	#endif

#elif MOTIF
	Window root, child;
	long rootX, rootY, childX, childY;
	unsigned int mask;
	int result = XQueryPointer (XWINPARAM, &root, &child, &rootX, &rootY,
								&childX, &childY, &mask);
	if (mask & Button1Mask)
		buttons |= kLButton;
	if (mask & Button2Mask)
		buttons |= kMButton;
	if (mask & Button3Mask)
		buttons |= kRButton;

	if (mask & ShiftMask)
		buttons |= kShift;
	if (mask & ControlMask)
		buttons |= kControl;
	if (mask & Mod1Mask)
		buttons |= kAlt;

#elif BEOS
	BPoint	where;
	uint32	b;
	pView->GetMouse (&where, &b);
	if (b & B_PRIMARY_MOUSE_BUTTON)
		buttons |= kLButton;
	if (b & B_SECONDARY_MOUSE_BUTTON)
		buttons |= kRButton;
	if (b & B_TERTIARY_MOUSE_BUTTON)
		buttons |= kMButton;
	int32 m = modifiers ();
	if (m & B_SHIFT_KEY)
		buttons |= kShift;
	if (m & B_COMMAND_KEY)
		buttons |= kControl;
	if (m & B_OPTION_KEY)
		buttons |= kApple;
	if (m & B_CONTROL_KEY)
		buttons |= kAlt;
#endif
	
	return buttons;
}

//-----------------------------------------------------------------------------
void CDrawContext::getMouseLocation (CPoint &point)
{
#if WINDOWS
	POINT where;
	GetCursorPos (&where);
	point (where.x, where.y);

#elif MACX
	#if 0 // QUARTZ // does not work sic!
	Point where;
	UInt32 mod;
	MouseTrackingResult result;
	if (TrackMouseLocationWithOptions ((CGrafPtr)-1, 0, kEventDurationNoWait, &where, &mod, &result) == noErr)
	{
		QDGlobalToLocalPoint (getPort (), &where);
		point (where.h, where.v);
	}
	#else
	Point where;
	CGrafPtr savedPort;
	Boolean portChanged = QDSwapPort (getPort (), &savedPort);
	GetMouse (&where);
	if (portChanged)
		QDSwapPort (savedPort, NULL);
	point (where.h, where.v);
	#endif
#elif MAC
	Point where;
	GetMouse (&where);
	point (where.h, where.v);
	
#elif MOTIF
	Window root, child;
	int rootX, rootY, childX, childY;
	unsigned int mask;
	int result = XQueryPointer (XWINPARAM, &root, &child, &rootX, &rootY, 
								&childX, &childY, &mask);
	point (childX, childY);

#elif BEOS
	BPoint	where;
	uint32	b;
	pView->GetMouse (&where, &b);
	point (where.x, where.y);
#endif

	point.offset (-offsetScreen.h, -offsetScreen.v);
}

//-----------------------------------------------------------------------------
bool CDrawContext::waitDoubleClick ()
{
	bool doubleClick = false;

#if WINDOWS
	CPoint mouseLoc;
	getMouseLocation (mouseLoc);
	CRect observe (mouseLoc.h - 2, mouseLoc.v - 2, mouseLoc.h + 2, mouseLoc.v + 2);

	DWORD currentTime = GetTickCount ();
	DWORD clickTime = GetMessageTime () + (DWORD)GetDoubleClickTime ();

	MSG message;
	while (currentTime < clickTime)
	{
		getMouseLocation (mouseLoc);
		if (!observe.pointInside (mouseLoc))
			break;

		if (PeekMessage (&message, 0, WM_LBUTTONDOWN, WM_LBUTTONDOWN, PM_REMOVE | PM_NOYIELD)) 
		{
			doubleClick = true;
			break;
		}
		
		currentTime = GetTickCount ();
	}

#elif MAC
	#if MACX
	#if QUARTZ
	EventTimeout timeout = GetDblTime () * kEventDurationSecond / 60;
	const EventTypeSpec eventTypes[] = { { kEventClassMouse, kEventMouseDown }, { kEventClassMouse, kEventMouseDragged } };
	EventRef event;
	if (ReceiveNextEvent (GetEventTypeCount (eventTypes), eventTypes, timeout, true, &event) == noErr)
	{
		if (GetEventKind (event) == kEventMouseDown)
		{
			doubleClick = true;
		}
		ReleaseEvent (event);
	}
	
	#else
	unsigned long clickTime, doubletime;
	EventRecord downEvent;

	doubletime = GetDblTime ();
	clickTime = TickCount () + doubletime;
	while (TickCount () < clickTime)
	{
		if (GetNextEvent (mDownMask, &downEvent))
		{
			doubleClick = true;
			break;
		}
	}
	#endif // !QUARTZ

	#else
	long clickTime, doubleTime;
	EventRecord downEvent;

	#define MOUSE_IS_DOWN   ((* (char*)0x172) >= 0)

	doubleTime = GetDblTime () / 2;
	clickTime  = TickCount () + doubleTime;
	
	while (TickCount () < clickTime)
		if (!MOUSE_IS_DOWN) break;        /* look for mouse up! */
	
	if (GetNextEvent (mUpMask, &downEvent))
	{
		clickTime += doubleTime;
		while (TickCount () < clickTime)
			if (MOUSE_IS_DOWN) break; /* look for mouse down! */
		if (GetNextEvent (mDownMask, &downEvent))
			doubleClick = true;
	}
	#endif
#elif MOTIF	
	long currentTime = _getTicks ();
	long clickTime = currentTime + XtGetMultiClickTime (pDisplay);
	
	XEvent e;
	while (currentTime < clickTime)
	{
		if (XCheckTypedEvent (pDisplay, ButtonPress, &e))
		{
			doubleClick = true;
			break;
		}
	
		currentTime = _getTicks ();
	}

#elif BEOS
	const bigtime_t snoozeTime = 5000;
	bigtime_t	latest = system_time ();
	bigtime_t	doubleclicktime;
	get_click_speed (&doubleclicktime);
	latest += doubleclicktime;
	BPoint	location;
	uint32	buttons;
	pView->GetMouse (&location, &buttons);
	while (buttons)	// user should release the mouse button
	{
		if (system_time () > latest)
			return false;

		snooze (snoozeTime);
		pView->GetMouse (&location, &buttons);
	}
	
	while (!buttons)
	{
		if (system_time () > latest)
			return false;

		snooze (snoozeTime);
		pView->GetMouse (&location, &buttons);
	}
	
	doubleClick = true;

#endif

	return doubleClick;
}

//-----------------------------------------------------------------------------
bool CDrawContext::waitDrag ()
{
	#if MACX && QUARTZ
	bool dragged = false;
	if (GetCurrentEventButtonState () & kEventMouseButtonPrimary)
	{
		const EventTypeSpec eventTypes[] = { { kEventClassMouse, kEventMouseUp }, { kEventClassMouse, kEventMouseDown }, { kEventClassMouse, kEventMouseDragged } };
		EventRef event;
		if (ReceiveNextEvent (GetEventTypeCount (eventTypes), eventTypes, kEventDurationForever, true, &event) == noErr)
		{
			if (GetEventKind (event) == kEventMouseDragged)
			{
				dragged = true;
			}
			ReleaseEvent (event);
		}
	}
	return dragged;

	#else
	if (!pFrame)
		return false;
	
	CPoint mouseLoc;
	getMouseLocation (mouseLoc);
	CRect observe (mouseLoc.h - 2, mouseLoc.v - 2, mouseLoc.h + 2, mouseLoc.v + 2);
	
	long currentTime = pFrame->getTicks ();
	bool wasOutside = false;

	while (((getMouseButtons () & ~(kMButton|kRButton)) & kLButton) != 0)
	{
		pFrame->doIdleStuff ();
		if (!wasOutside)
		{
			getMouseLocation (mouseLoc);
			if (!observe.pointInside (mouseLoc))
			{
				if (kDragDelay <= 0)
					return true;
				wasOutside = true;
			}
		}

		if (wasOutside && (pFrame->getTicks () - currentTime > kDragDelay))
			return true;
	}
	return false;
	#endif
}

//-----------------------------------------------------------------------------
#if MOTIF
//-----------------------------------------------------------------------------
long CDrawContext::getIndexColor (CColor color)
{
	// 24bit visual ?
	if (pFrame->getDepth () == 24) 
		return (unsigned int)color.blue << 16 | (unsigned int)color.green << 8 | (unsigned int)color.red;

	// 8bit stuff
	return getIndexColor8Bit (color, pDisplay, pFrame->getColormap ());
}

//-----------------------------------------------------------------------------
Colormap CDrawContext::getColormap ()
{
	if (pFrame)
		return pFrame->getColormap ();
	else
		return NULL;
}

//-----------------------------------------------------------------------------
Visual* CDrawContext::getVisual ()
{
	if (pFrame)
		return pFrame->getVisual ();
	else
		return NULL;
}

//-----------------------------------------------------------------------------
unsigned int CDrawContext::getDepth ()
{
	if (pFrame)
		return pFrame->getDepth ();
	else
		return NULL;
}

//-----------------------------------------------------------------------------
#elif BEOS
//-----------------------------------------------------------------------------
void CDrawContext::lineFromTo (CPoint& cstart, CPoint& cend)
{
	BPoint start (cstart.h, cstart.v);
	BPoint end (cend.h, cend.v);
	if (start.x == end.x)
	{
		if (start.y < end.y)
			end.y--;
		else if (end.y < start.y)
			start.y--;
	}
	else if (start.y == end.y)
	{
		if (start.x < end.x)
			end.x--;
		else if (end.x < start.x)
			start.x--;
	}
	else
	{
		if (start.x > end.x)
		{
			BPoint t = end;
			end = start;
			start = t;
		}
		end.x--;
		if (end.y > start.y)
			end.y--;
		else
			end.y++;
	}
	
	pView->MovePenTo (start);
	if (lineStyle == kLineSolid)
		pView->StrokeLine (end);
	else
	{
		pattern stripes = { {0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3} };
		pView->StrokeLine (end, stripes);
	}
}

//-----------------------------------------------------------------------------
#elif MAC
#if QUARTZ
CGContextRef CDrawContext::beginCGContext ()
{
	if (gCGContext)
	{
		CGContextSaveGState (gCGContext);
		return gCGContext;
	}
	return 0;
}

void CDrawContext::releaseCGContext (CGContextRef context)
{
	if (context)
	{
		CGContextRestoreGState (context);
		CGContextSynchronize (context);
	}
}

#endif
BitMapPtr CDrawContext::getBitmap ()
{
	#if QUARTZ
	return (BitMapPtr)GetPortBitMapForCopyBits (GetWindowPort ((WindowRef)pWindow));
	#else
	PixMapHandle pixMap = GetPortPixMap (GetWindowPort ((WindowRef)pWindow));
	if (pixMap)
	{
		LockPixels (pixMap);
		return (BitMapPtr)*pixMap;
	}
	#endif
	return 0;
}

//-----------------------------------------------------------------------------
void CDrawContext::releaseBitmap ()
{
	#if !QUARTZ
	PixMapHandle pixMap = GetPortPixMap (GetWindowPort ((WindowRef)pWindow));
	UnlockPixels (pixMap);
	#endif
}

//-----------------------------------------------------------------------------
CGrafPtr CDrawContext::getPort ()
{
	#if QUARTZ
	if (pWindow)
		return (CGrafPtr)GetWindowPort ((WindowRef)pWindow);
	return 0;
	#else
	if (!bInitialized)
	{
		CGrafPtr OrigPort;
		GDHandle OrigDevice;
		GetGWorld (&OrigPort, &OrigDevice);
		SetGWorld ((CGrafPtr)GetWindowPort ((WindowRef)pWindow), NULL);
	
		TextMode (srcOr);
		PenMode (patCopy);
		StuffHex (&fillPattern, "\pFFFFFFFFFFFFFFFF");
	
		SetGWorld (OrigPort, OrigDevice);
		
		bInitialized = true;
	}
	return (CGrafPtr)GetWindowPort ((WindowRef)pWindow);
	#endif
}

#endif


//-----------------------------------------------------------------------------
// COffscreenContext Implementation
//-----------------------------------------------------------------------------
COffscreenContext::COffscreenContext (CDrawContext *pContext, CBitmap *pBitmapBg, bool drawInBitmap)
	: CDrawContext (pContext->pFrame, NULL, NULL),
		pBitmap (0), pBitmapBg (pBitmapBg), height (20), width (20)
{
	if (pBitmapBg)
	{
		height = pBitmapBg->getHeight ();
		width  = pBitmapBg->getWidth ();
		
		clipRect (0, 0, width, height);
	}

	#if DEBUG
	gNbCOffscreenContext++;
	gBitmapAllocation += height * width;
	#endif
		
	bDestroyPixmap = false;
	
#if WINDOWS
	if (pOldBrush)
		SelectObject ((HDC)getSystemContext (), pOldBrush);
	if (pOldPen)
		SelectObject ((HDC)getSystemContext (), pOldPen);
	if (pOldFont)
		SelectObject ((HDC)getSystemContext (), pOldFont);
	pOldBrush = pOldPen = pOldFont = 0;

	pSystemContext = CreateCompatibleDC ((HDC)pContext->getSystemContext ());
	
	if (drawInBitmap)
		pWindow = pBitmapBg->getHandle ();
	else // create bitmap if no bitmap handle exists
	{
		bDestroyPixmap = true;
		pWindow = CreateCompatibleBitmap ((HDC)pContext->getSystemContext (), width, height);
	}
	oldBitmap = SelectObject ((HDC)pSystemContext, pWindow);

#elif MAC
	#if QUARTZ
	offscreenBitmap = 0;
	if (drawInBitmap)
	{
		if (pBitmapBg->getHandle ())
		{
			PixMapHandle pixMap = GetGWorldPixMap ((GWorldPtr)pBitmapBg->getHandle ());
			LockPixels (pixMap);
			CGColorSpaceRef colorspace = CGColorSpaceCreateDeviceRGB ();
			size_t pixDepth = GetPixDepth (pixMap) / 4;
			size_t rowBytes = GetPixRowBytes (pixMap);
			gCGContext = CGBitmapContextCreate (GetPixBaseAddr (pixMap), width, height, pixDepth, rowBytes, colorspace, kCGImageAlphaPremultipliedFirst);
			CGColorSpaceRelease (colorspace);
			if (gCGContext)
				CGContextTranslateCTM (gCGContext, 0, (float)height);
		}
	}
	else
	{ // todo !!!
	}
	
	#else
	
	if (drawInBitmap)
		pWindow = pBitmapBg->getHandle ();
	else
	{
		Rect	GWRect;
		GWRect.top  = 0;
		GWRect.left = 0;
		GWRect.right  = width;
		GWRect.bottom = height;
		NewGWorld ((GWorldPtr*)&pWindow, 0, &GWRect, NULL, NULL, 0);
		bDestroyPixmap = true;
	}

	StuffHex (&fillPattern, "\pFFFFFFFFFFFFFFFF");
	#endif
        
#elif MOTIF
 	// if no bitmap handle => create one
	if (!pWindow)
	{
		Drawable dWindow = pContext->pFrame->getWindow ();
		pWindow = (void*)XCreatePixmap (pDisplay, dWindow, width, height, pFrame->getDepth ());
		bDestroyPixmap = true;
	}

	// set the current font
	if (pSystemContext)
		setFont (kNormalFont);

#elif BEOS
	bDestroyPixmap = true;
	offscreenBitmap = new BBitmap (BRect (0, 0, width - 1, height - 1), B_RGB16, true, false);
	pView = new BView (BRect (0, 0, width - 1, height - 1), NULL, 0, 0);
	offscreenBitmap->Lock ();
	offscreenBitmap->AddChild (pView);

#endif

	if (!drawInBitmap)
	{
		// draw bitmap to Offscreen
		CRect r (0, 0, width, height);
		if (pBitmapBg)
			pBitmapBg->draw (this, r);
		else
		{
			setFillColor (kBlackCColor);
			fillRect (r);
		}
	}
}

//-----------------------------------------------------------------------------
COffscreenContext::COffscreenContext (CFrame *pFrame, long width, long height, const CColor backgroundColor)
	: CDrawContext (pFrame, NULL, NULL), 
	pBitmap (0), pBitmapBg (0), height (height), width (width), backgroundColor (backgroundColor)
{
	clipRect (0, 0, width, height);

	#if DEBUG
	gNbCOffscreenContext++;
	gBitmapAllocation += height * width;
	#endif

	bDestroyPixmap = true;

#if WINDOWS
	void *SystemWindow = pFrame->getSystemWindow ();
	void *SystemContext = GetDC ((HWND)SystemWindow);
	
	pSystemContext = CreateCompatibleDC ((HDC)SystemContext);
	#if DEBUG
	gNbDC++;
	#endif
	pWindow = CreateCompatibleBitmap ((HDC)SystemContext, width, height);

	oldBitmap = SelectObject ((HDC)pSystemContext, pWindow);
	ReleaseDC ((HWND)SystemWindow, (HDC)SystemContext);

	CRect r (0, 0, width, height);
	setFillColor (backgroundColor);
	setFrameColor (backgroundColor);
	fillRect (r);
	drawRect (r);

#elif MAC
	#if QUARTZ
	CGContextRef    context = NULL; 
	CGColorSpaceRef colorSpace; 
	int             bitmapByteCount; 
	int             bitmapBytesPerRow; 

	// each pixel is represented by four bytes 
	// (8 bits each of alpha, R, G, B) 
	bitmapBytesPerRow   = width * 4; 
	bitmapByteCount     = bitmapBytesPerRow * height; 

	// create an RGB color space 
	colorSpace = CGColorSpaceCreateDeviceRGB ();

	// create the bitmap 
	offscreenBitmap = malloc (bitmapByteCount);
	if (offscreenBitmap != NULL)
	{
		memset (offscreenBitmap, 0, bitmapByteCount);
		// create the context 
		context = CGBitmapContextCreate (offscreenBitmap,
		width, 
		height, 
		8,              // bits per component 
		bitmapBytesPerRow, 
		colorSpace, 
		kCGImageAlphaPremultipliedFirst);

		if (context == NULL)
		{
			// the context couldn't be created for some reason, 
			// and we have no use for the bitmap without the context 
			free (offscreenBitmap);
			offscreenBitmap = 0;
		}
		else
		{
			CGContextTranslateCTM (context, 0, (float)height);
		}
	}

	// the context retains the color space, so we can release it 
	CGColorSpaceRelease (colorSpace);
	gCGContext = context;

	#else
	QDErr	err;
	Rect	GWRect;
	
	GWRect.top = GWRect.left = 0;
	GWRect.right = width;
	GWRect.bottom = height;
	err = NewGWorld ((GWorldPtr*) &pWindow, 0, &GWRect, NULL, NULL, 0);
	if (err)
		pWindow = NULL;

	StuffHex (&fillPattern, "\pFFFFFFFFFFFFFFFF");

	CRect r (0, 0, width, height);
	setFillColor (backgroundColor);
	setFrameColor (backgroundColor);
	fillRect (r);
	drawRect (r);
	#endif
        
#elif MOTIF
	Drawable dWindow = pFrame->getWindow ();

	pWindow = (void*)XCreatePixmap (pDisplay, dWindow, width, height, pFrame->getDepth ());

	// clear the pixmap
	XGCValues values;
	values.foreground = getIndexColor (backgroundColor);
	GC gc = XCreateGC (pDisplay, (Drawable)pWindow, GCForeground, &values); 
	XFillRectangle (pDisplay, (Drawable)pWindow, gc, 0, 0, width, height);
	XFreeGC (pDisplay, gc);
	
	// set the current font
	if (pSystemContext)
		setFont (kNormalFont);

#elif BEOS
	BRect frame (0, 0, width - 1, height - 1);
	offscreenBitmap = new BBitmap (frame, B_RGB16, true, false);
	pView = new BView (BRect (0, 0, width - 1, height - 1), NULL, 0, 0);
	offscreenBitmap->Lock ();
	offscreenBitmap->AddChild (pView);
	if (backgroundColor.red != 255 || backgroundColor.green != 255 || backgroundColor.blue != 255)
	{
		rgb_color c = { backgroundColor.red, backgroundColor.green, backgroundColor.blue, 255 };
		pView->SetHighColor (c);
		pView->FillRect (frame);
	}
#endif
}

//-----------------------------------------------------------------------------
COffscreenContext::~COffscreenContext ()
{
	#if DEBUG
	gNbCOffscreenContext--;
	gBitmapAllocation -= height * width;
	#endif

	if (pBitmap)
		pBitmap->forget ();

#if WINDOWS
	if (pSystemContext)
	{
		DeleteDC ((HDC)pSystemContext);
		pSystemContext = 0;
		#if DEBUG
		gNbDC--;
		#endif
	}
	if (bDestroyPixmap && pWindow)
		DeleteObject (pWindow);

#elif MAC
	#if QUARTZ
	if (gCGContext)
		CGContextRelease (gCGContext);
	gCGContext = 0;
	if (offscreenBitmap)
		free (offscreenBitmap);
	else if (pBitmapBg && pBitmapBg->getHandle ())
	{
		PixMapHandle pixMap = GetGWorldPixMap ((GWorldPtr)pBitmapBg->getHandle ());
		UnlockPixels (pixMap);
	}
	#else
	if (bDestroyPixmap && pWindow)
		DisposeGWorld ((GWorldPtr)pWindow);
	#endif
        
#elif MOTIF
	if (bDestroyPixmap && pWindow)
		XFreePixmap (pDisplay, (Pixmap)pWindow);

#elif BEOS
	delete offscreenBitmap;
	pView = 0;	// deleted because attached to the offscreen
#endif
}

//-----------------------------------------------------------------------------
void COffscreenContext::copyTo (CDrawContext* pContext, CRect& srcRect, CPoint destOffset)
{
#if WINDOWS
	BitBlt ((HDC)pSystemContext,
			destOffset.h,
			destOffset.v,
			srcRect.width (),
			srcRect.height (),
			(HDC)pContext->getSystemContext (),
			srcRect.left + pContext->offset.h,
			srcRect.top + pContext->offset.v,
			SRCCOPY);
			
#elif MAC
	#if QUARTZ
	if (!pBitmapBg)
		return;
	#else
	if (!pWindow)
		return;
	#endif
	
	Rect source, dest;
	RGBColor savedForeColor, savedBackColor;
	
	source.left   = srcRect.left + pContext->offset.h + pContext->offsetScreen.h;
	source.top    = srcRect.top + pContext->offset.v + pContext->offsetScreen.v;
	source.right  = source.left + srcRect.right - srcRect.left;
	source.bottom = source.top + srcRect.bottom - srcRect.top;
	
	dest.left   = destOffset.h;
	dest.top    = destOffset.v;
	dest.right  = dest.left + srcRect.right - srcRect.left;
	dest.bottom = dest.top + srcRect.bottom - srcRect.top;

	GetForeColor (&savedForeColor);
	GetBackColor (&savedBackColor);
	::BackColor (whiteColor);
	::ForeColor (blackColor);

	CopyBits (pContext->getBitmap (), getBitmap (), &source, &dest, srcCopy, 0L);
	releaseBitmap ();
	pContext->releaseBitmap ();

	RGBForeColor (&savedForeColor);
	RGBBackColor (&savedBackColor);
#endif
}

//-----------------------------------------------------------------------------
void COffscreenContext::copyFrom (CDrawContext *pContext, CRect destRect, CPoint srcOffset)
{
#if WINDOWS
	BitBlt ((HDC)pContext->getSystemContext (),  // hdcDest
					destRect.left + pContext->offset.h, // xDest
					destRect.top + pContext->offset.v,  // yDest
					destRect.right - destRect.left,     // xWidth,
					destRect.bottom - destRect.top,     // yHeight
          
					(HDC)pSystemContext,                // hdcSrc
					srcOffset.h,                        // xSrc
					srcOffset.v,                        // ySrc
					SRCCOPY);                           // dwROP

#elif MAC
	#if QUARTZ
	if (!gCGContext)
		return;
	CGContextRef context = pContext->beginCGContext ();
	if (context)
	{
		size_t pixRowBytes = CGBitmapContextGetBytesPerRow (gCGContext);
		short pixDepth = CGBitmapContextGetBitsPerPixel (gCGContext);
		size_t size = pixRowBytes * CGBitmapContextGetHeight (gCGContext);

		CGImageRef image = 0;
		CGDataProviderRef provider = CGDataProviderCreateWithData (NULL, CGBitmapContextGetData (gCGContext), size, NULL);
		CGColorSpaceRef colorspace = CGColorSpaceCreateDeviceRGB ();
		CGImageAlphaInfo alphaInfo = CGBitmapContextGetAlphaInfo (gCGContext);
		image = CGImageCreate (CGBitmapContextGetWidth (gCGContext), CGBitmapContextGetHeight (gCGContext), 8 , pixDepth, pixRowBytes, colorspace, alphaInfo, provider, NULL, 0, kCGRenderingIntentDefault);
		if (image)
		{
			CGRect dest;
			dest.origin.x = destRect.left + srcOffset.h + pContext->offset.h;
			dest.origin.y = (destRect.top + pContext->offset.v) * -1 - (getHeight () - srcOffset.v);
			dest.size.width = getWidth ();
			dest.size.height = getHeight ();
			
			CGContextScaleCTM (context, 1, 1);

			CGRect clipRect;
			clipRect.origin.x = destRect.left + pContext->offset.h;
		    clipRect.origin.y = (destRect.top + pContext->offset.v) * -1  - destRect.height ();
		    clipRect.size.width = destRect.width (); 
		    clipRect.size.height = destRect.height ();
			
			CGContextClipToRect (context, clipRect);

			CGContextDrawImage (context, dest, image);
			
			CGImageRelease (image);
		}
		CGDataProviderRelease (provider);
		CGColorSpaceRelease (colorspace);
		
		pContext->releaseCGContext (context);
	}
	#else
	if (!pWindow)
		return;

	Rect source, dest;
	RGBColor savedForeColor, savedBackColor;
	
	source.left   = srcOffset.h;
	source.top    = srcOffset.v;
	source.right  = source.left + destRect.right - destRect.left;
	source.bottom = source.top + destRect.bottom - destRect.top;
	
	dest.top    = destRect.top + pContext->offset.v;
	dest.left   = destRect.left + pContext->offset.h;
	dest.bottom = destRect.bottom + pContext->offset.v;
	dest.right  = destRect.right + pContext->offset.h;

	GetForeColor (&savedForeColor);
	GetBackColor (&savedBackColor);
	::BackColor (whiteColor);
	::ForeColor (blackColor);

	CopyBits (getBitmap (), pContext->getBitmap (), &source, &dest, srcCopy, 0L);
	#if MACX
	QDAddRectToDirtyRegion (pContext->getPort (), &dest);
	#endif
	releaseBitmap ();
	pContext->releaseBitmap ();

	RGBForeColor (&savedForeColor);
	RGBBackColor (&savedBackColor);
	#endif

#elif MOTIF
	XCopyArea (pDisplay, (Drawable)pWindow, (Drawable)pContext->getWindow (),
						 (GC)pSystemContext, srcOffset.h, srcOffset.v,
						 destRect.width (), destRect.height (),
						 destRect.left, destRect.top);

#elif BEOS
	pContext->pView->SetDrawingMode (B_OP_COPY);
	BRect destination (destRect.left, destRect.top, destRect.right - 1, destRect.bottom - 1);
	BRect source = destination;
	source.OffsetTo (srcOffset.h, srcOffset.v);
	pView->Sync ();
	pContext->pView->DrawBitmap (offscreenBitmap, source, destination);
#endif
}

//-----------------------------------------------------------------------------
#if MAC
BitMapPtr COffscreenContext::getBitmap ()
{
	#if QUARTZ
	return (BitMapPtr)GetPortBitMapForCopyBits ((GWorldPtr)pBitmapBg->getHandle ());
	#else
	PixMapHandle pixMap = GetGWorldPixMap ((GWorldPtr)pWindow);
	if (pixMap)
	{
		LockPixels (pixMap);
		return (BitMapPtr)*pixMap;
	}
	return 0;
	#endif
}

//-----------------------------------------------------------------------------
void COffscreenContext::releaseBitmap ()
{
	#if QUARTZ
	#else
	PixMapHandle pixMap = GetGWorldPixMap ((GWorldPtr)pWindow);
	UnlockPixels (pixMap);
	#endif
}

#if !QUARTZ
//-----------------------------------------------------------------------------
CGrafPtr COffscreenContext::getPort ()
{
	if (!bInitialized)
		bInitialized = true;

	return (CGrafPtr)pWindow;
}
#endif // QUARTZ
#endif // MAC

//-----------------------------------------------------------------------------
char* kMsgCheckIfViewContainer	= "kMsgCheckIfViewContainer";

//-----------------------------------------------------------------------------
// CView
//-----------------------------------------------------------------------------
CView::CView (const CRect& size)
:	size (size), mouseableArea (size), pParent (0), pParentView (0),
	bDirty (false), bMouseEnabled (true), bTransparencyEnabled (false), pBackground (0)
{
	#if DEBUG
	gNbCView++;
	#endif
}

//-----------------------------------------------------------------------------
CView::~CView ()
{
	if (pBackground)
		pBackground->forget ();

	#if DEBUG
	gNbCView--;
	#endif
}

//-----------------------------------------------------------------------------
void CView::getMouseLocation (CDrawContext* context, CPoint &point)
{
	if (context)
	{
		if (pParentView && pParentView->notify (this, kMsgCheckIfViewContainer) == kMessageNotified)
		{
			long save[4];
			((CViewContainer*)pParentView)->modifyDrawContext (save, context);
			pParentView->getMouseLocation (context, point);
			((CViewContainer*)pParentView)->restoreDrawContext (context, save);
		}
		else
			context->getMouseLocation (point);
	}
}

//-----------------------------------------------------------------------------
void CView::getFrameTopLeftPos (CPoint& topLeft)
{
	topLeft.h += size.left;
	topLeft.v += size.top;
	if (pParentView && pParentView->notify (this, kMsgCheckIfViewContainer) == kMessageNotified)
		pParentView->getFrameTopLeftPos (topLeft);
}

//-----------------------------------------------------------------------------
void CView::redraw ()
{
	if (pParent)
		pParent->draw (this);
}

//-----------------------------------------------------------------------------
void CView::redrawRect (CDrawContext* context, const CRect& rect)
{
	// we always pass it on to the parent view as it knows what else must be drawn (needed for nested view containers)
	if (pParentView)
		pParentView->redrawRect (context, rect);
	else if (pParent)
		pParent->drawRect (context, rect);
}

//-----------------------------------------------------------------------------
void CView::draw (CDrawContext *pContext)
{
	setDirty (false);
}

//-----------------------------------------------------------------------------
void CView::mouse (CDrawContext *pContext, CPoint &where, long buttons)
{}

//-----------------------------------------------------------------------------
bool CView::onDrop (void **ptrItems, long nbItems, long type, CPoint &where)
{
	return false;
}

//-----------------------------------------------------------------------------
bool CView::onWheel (CDrawContext *pContext, const CPoint &where, float distance)
{
	return false;
}

//------------------------------------------------------------------------
void CView::update (CDrawContext *pContext)
{
	if (isDirty ())
	{
		#if NEW_UPDATE_MECHANISM
		if (pContext)
			redrawRect (pContext, size);
		else
			redraw ();
		#else
		#if USE_ALPHA_BLEND
		if (pContext)
		{
			if (bTransparencyEnabled)
				getParent ()->drawRect (pContext, size);
			else
				draw (pContext);
		}
		#else
		if (pContext)
			draw (pContext);
		#endif
		else
			redraw ();
		#endif // !NEW_UPDATE_MECHANISM
		setDirty (false);
	}
}

//------------------------------------------------------------------------------
long CView::onKeyDown (VstKeyCode& keyCode)
{
	return -1;
}

//------------------------------------------------------------------------------
long CView::onKeyUp (VstKeyCode& keyCode)
{
	return -1;
}

//------------------------------------------------------------------------------
long CView::notify (CView* sender, const char* message)
{
	return kMessageUnknown;
}

//------------------------------------------------------------------------------
void CView::looseFocus (CDrawContext *pContext)
{}

//------------------------------------------------------------------------------
void CView::takeFocus (CDrawContext *pContext)
{}

//------------------------------------------------------------------------------
void CView::setViewSize (CRect &rect)
{
	size = rect;
	setDirty ();
}

//-----------------------------------------------------------------------------
void *CView::getEditor ()
{ 
	return pParent ? pParent->getEditor () : 0; 
}


//-----------------------------------------------------------------------------
void CView::setBackground (CBitmap *background)
{
	if (pBackground)
		pBackground->forget ();
	pBackground = background;
	if (pBackground)
		pBackground->remember ();
}

#define FOREACHSUBVIEW for (CCView *pSv = pFirstView; pSv; pSv = pSv->pNext) {CView *pV = pSv->pView;
#define ENDFOR }

//-----------------------------------------------------------------------------
// CFrame Implementation
//-----------------------------------------------------------------------------
CFrame::CFrame (const CRect &inSize, void *inSystemWindow, void *inEditor)
:	CViewContainer (inSize, 0, 0),
	pSystemWindow (inSystemWindow), pEditor (inEditor),
	pModalView (0), pEditView (0), bFirstDraw (true), bDropActive (false),
	pFrameContext (0), bAddedWindow (false), pVstWindow (0), defaultCursor (0)
{
	setOpenFlag (true);
	
	pParent = this;

#if WINDOWS
	pHwnd = 0;
	OleInitialize (0);

	#if DYNAMICALPHABLEND
	pfnAlphaBlend = 0;
	pfnTransparentBlt = 0;

	hInstMsimg32dll = LoadLibrary ("msimg32.dll");
	if (hInstMsimg32dll)
	{
		pfnAlphaBlend = (PFNALPHABLEND)GetProcAddress (hInstMsimg32dll, "AlphaBlend");

		// get OS version
		OSVERSIONINFOEX	osvi;

		memset (&osvi, 0, sizeof (osvi));
		osvi.dwOSVersionInfoSize = sizeof (osvi);

		if (GetVersionEx ((OSVERSIONINFO *)&osvi))
		{
			// Is this win NT or better?
			if (osvi.dwPlatformId >= VER_PLATFORM_WIN32_NT)
			{
				// Yes, then TransparentBlt doesn't have the memory-leak and can be safely used
				pfnTransparentBlt = (PFNTRANSPARENTBLT)GetProcAddress (hInstMsimg32dll, "TransparentBlt");
			}
		}
	}
	#endif	// DYNAMICALPHABLEND
    
#elif MOTIF
	gc = 0;
	depth    = 0;
	pDisplay = 0;
	pVisual  = 0;
	window   = 0;

#elif BEOS
	pPlugView = NULL;
#endif

	initFrame (pSystemWindow);

#if WINDOWS
#if USE_GLOBAL_CONTEXT
	pFrameContext = new CDrawContext (this, 0, getSystemWindow ());
	#endif

#elif MAC
	Gestalt (gestaltSystemVersion, &pSystemVersion);
	#if QUARTZ
	pFrameContext = 0;
	
	#else
	pFrameContext = new CDrawContext (this, getSystemWindow (), getSystemWindow ());
	pFrameContext->offset.h = size.left;
	pFrameContext->offset.v = size.top;
	#endif
	
#elif MOTIF
	pFrameContext = new CDrawContext (this, gc, (void*)window);
#endif
}

//-----------------------------------------------------------------------------
CFrame::CFrame (const CRect& inSize, const char* inTitle, void* inEditor, const long inStyle)
:	CViewContainer (inSize, 0, 0),
	pSystemWindow (0), pEditor (inEditor),
	pModalView (0), pEditView (0), bFirstDraw (true), bDropActive (false),
	pFrameContext (0), defaultCursor (0)
{
	bAddedWindow  = true;
	setOpenFlag (false);
	pParent = this;

#if WINDOWS
	pHwnd = 0;
	OleInitialize (0);

	#if DYNAMICALPHABLEND
	pfnAlphaBlend = 0;
	pfnTransparentBlt = 0;

	hInstMsimg32dll = LoadLibrary ("msimg32.dll");
	if (hInstMsimg32dll)
	{
		pfnAlphaBlend = (PFNALPHABLEND)GetProcAddress (hInstMsimg32dll, "AlphaBlend");

		// get OS version
		OSVERSIONINFOEX	osvi;

		memset (&osvi, 0, sizeof (osvi));
		osvi.dwOSVersionInfoSize = sizeof (osvi);

		if (GetVersionEx ((OSVERSIONINFO *)&osvi))
		{
			// Is this win NT or better?
			if (osvi.dwPlatformId >= VER_PLATFORM_WIN32_NT)
			{
				// Yes, then TransparentBlt doesn't have the memory-leak and can be safely used
				pfnTransparentBlt = (PFNTRANSPARENTBLT)GetProcAddress (hInstMsimg32dll, "TransparentBlt");
			}
		}
	}
	#endif
    
#elif MOTIF
	gc = 0;
	depth    = 0;
	pDisplay = 0;
	pVisual  = 0;
	window   = 0;

#elif BEOS
	pPlugView = NULL;

#endif

	#if !PLUGGUI
	pVstWindow = (VstWindow*)malloc (sizeof (VstWindow));
	strcpy (((VstWindow*)pVstWindow)->title, inTitle);
	((VstWindow*)pVstWindow)->xPos   = (short)size.left;
	((VstWindow*)pVstWindow)->yPos   = (short)size.top;
	((VstWindow*)pVstWindow)->width  = (short)size.width ();
	((VstWindow*)pVstWindow)->height = (short)size.height ();
	((VstWindow*)pVstWindow)->style  = inStyle;
	((VstWindow*)pVstWindow)->parent     = 0;
	((VstWindow*)pVstWindow)->userHandle = 0;
	((VstWindow*)pVstWindow)->winHandle  = 0;
	#endif
}

//-----------------------------------------------------------------------------
CFrame::~CFrame ()
{
	setCursor (kCursorDefault);

	setDropActive (false);

	if (pFrameContext)
		pFrameContext->forget ();

#if WINDOWS
	OleUninitialize ();
	
	#if DYNAMICALPHABLEND
	if (hInstMsimg32dll)
		FreeLibrary (hInstMsimg32dll);
	#endif
		
	if (pHwnd)
	{
		SetWindowLong ((HWND)pHwnd, GWL_USERDATA, (long)NULL);
		DestroyWindow ((HWND)pHwnd);

		ExitWindowClass ();
	}

#elif MOTIF
 #if TEST_REGION
	XDestroyRegion (region);
 #endif

	// remove callbacks to avoid undesirable update
	if (pSystemWindow)
	{
		XtRemoveCallback ((Widget)pSystemWindow, XmNexposeCallback,  _drawingAreaCallback, this);
		XtRemoveCallback ((Widget)pSystemWindow, XmNinputCallback,   _drawingAreaCallback, this);
		XtRemoveCallback ((Widget)pSystemWindow, XmNdestroyCallback, _destroyCallback, this);
		
		freeGc ();
	}
#endif
	
	if (bAddedWindow)
		close ();
	if (pVstWindow)
		free (pVstWindow);

#if BEOS
	CBitmap::closeResource ();	// must be done only once at the end of the story.
#endif

#if MAC && QUARTZ
	if (controlRef)
		DisposeControl (controlRef);
	if (controlSpec.u.classRef)
	{
		OSStatus status = UnregisterToolboxObjectClass ((ToolboxObjectClassRef)controlSpec.u.classRef);
		if (status != noErr)
			fprintf (stderr, "UnregisterToolboxObjectClass failed : %d\n", status);
	}
#endif

}

//-----------------------------------------------------------------------------
bool CFrame::open (CPoint *point)
{
#if PLUGGUI
	return false;
#else
	if (!bAddedWindow)
		return false;
	if (getOpenFlag ())
	{
#if WINDOWS
		BringWindowToTop (GetParent (GetParent ((HWND)getSystemWindow ())));

#elif MOTIF
		Widget widget = (Widget)getSystemWindow ();
		while (widget && !XtIsTopLevelShell (widget))
			widget = XtParent (widget);
		if (widget)
			XRaiseWindow (getDisplay (), XtWindow (widget));

#elif BEOS
		pPlugView->Window ()->Activate (true);
#endif
		return false;
	}

	if (pVstWindow)
	{
		if (point)
		{
			((VstWindow*)pVstWindow)->xPos = (short)point->h;
			((VstWindow*)pVstWindow)->yPos = (short)point->v;
		}
		AudioEffectX *pAudioEffectX = (AudioEffectX*)(((AEffGUIEditor*)pEditor)->getEffect ());
		pSystemWindow = pAudioEffectX->openWindow ((VstWindow*)pVstWindow);
	}

	if (pSystemWindow)
	{
		if (initFrame (pSystemWindow))
			setOpenFlag (true);
	}

	return getOpenFlag ();
#endif
}

//-----------------------------------------------------------------------------
bool CFrame::close ()
{
#if PLUGGUI
	return false;
#else
	if (!bAddedWindow || !getOpenFlag () || !pSystemWindow)
		return false;

	AudioEffectX *pAudioEffectX = (AudioEffectX*)(((AEffGUIEditor*)pEditor)->getEffect ());
	pAudioEffectX->closeWindow ((VstWindow*)pVstWindow);

	pSystemWindow = 0;

	return true;
#endif
}

//-----------------------------------------------------------------------------
bool CFrame::initFrame (void *systemWin)
{
	if (!systemWin)
		return false;
	
#if WINDOWS

	InitWindowClass ();
	pHwnd = CreateWindowEx (0, gClassName, "Window",
			 WS_CHILD | WS_VISIBLE, 
			 0, 0, size.width (), size.height (), 
			 (HWND)pSystemWindow, NULL, GetInstance (), NULL);

	SetWindowLongPtr ((HWND)pHwnd, GWLP_USERDATA, (LONG_PTR)this);

#elif MAC

	#if QUARTZ
	dragEventHandler = 0;
	if (!registerWithToolbox ())
		return false;

	hasFocus = false;
	Rect r = {size.top, size.left, size.bottom, size.right};
	OSStatus status = CreateCustomControl (NULL, &r, &controlSpec, NULL, &controlRef);
	if (status != noErr)
	{
		fprintf (stderr, "Could not create Control : %d\n", status);
		return false;
	}
	SetControlDragTrackingEnabled (controlRef, true);
	SetAutomaticControlDragTrackingEnabledForWindow ((WindowRef)systemWin, true);
	#if !AU // for AudioUnits define AU and embed the controlRef at your AUCarbonViewBase
	WindowAttributes attributes;
	GetWindowAttributes ((WindowRef)systemWin, &attributes);
	if (attributes & kWindowCompositingAttribute) 
	{
		HIViewRef contentView;
		HIViewRef rootView = HIViewGetRoot ((WindowRef)systemWin);
		if (HIViewFindByID (rootView, kHIViewWindowContentID, &contentView) != noErr)
			contentView = rootView;
		HIViewAddSubview (contentView, controlRef);
	}
	else
	{
		ControlRef rootControl;
		GetRootControl ((WindowRef)systemWin, &rootControl);
		if (rootControl == NULL)
			CreateRootControl ((WindowRef)systemWin, &rootControl);
		EmbedControl(controlRef, rootControl);	
	}
	#endif
	#endif
	
#elif MOTIF
	// attach the callbacks
	XtAddCallback ((Widget)systemWin, XmNexposeCallback, _drawingAreaCallback, this);
	XtAddCallback ((Widget)systemWin, XmNinputCallback,  _drawingAreaCallback, this);
	XtAddCallback ((Widget)systemWin, XmNdestroyCallback, _destroyCallback, this);
	XtAddEventHandler ((Widget)systemWin, LeaveWindowMask, true, _eventHandler, this);

	// init a default gc
	window  = XtWindow ((Widget)systemWin);
	pDisplay = XtDisplay ((Widget)systemWin);
	XGCValues values;
	values.foreground = 1;
	gc = XCreateGC (pDisplay, (Drawable)window, GCForeground, &values); 
	
#if TEST_REGION
	region = XCreateRegion ();
#endif

	// get the std colormap
	XWindowAttributes attr;
	XGetWindowAttributes (pDisplay, window, &attr);
	colormap = attr.colormap;
	pVisual  = attr.visual;
	depth    = attr.depth;

	// init and load the fonts
	if (!gFontInit)
	{
		for (long i = 0; i < kNumStandardFonts; i++) 
		{
			gFontStructs[i] = XLoadQueryFont (pDisplay, gFontTable[i].string);
			assert (gFontStructs[i] != 0);
		}
		gFontInit = true;
	}

#elif BEOS
	BView* parentView = (BView*) pSystemWindow;
	BRect frame = parentView->Frame ();
	frame.OffsetTo (B_ORIGIN);
	pPlugView = new PlugView (frame, this);
	parentView->AddChild (pPlugView);
#endif

	setDropActive (true);

	return true;
}

//-----------------------------------------------------------------------------
bool CFrame::setDropActive (bool val)
{	
	if (!bDropActive && !val)
		return true;

#if WINDOWS
	if (!pHwnd)
		return false;
	if (val)
		RegisterDragDrop ((HWND)pHwnd, (IDropTarget*)createDropTarget (this));
	else
		RevokeDragDrop ((HWND)pHwnd);

#elif MAC
#if MAC_OLD_DRAG
	if (val)
		install_drop (this);
	else
		remove_drop (this);
#endif
#endif

	bDropActive = val;
	return true;
}

#if MOTIF
//-----------------------------------------------------------------------------
void CFrame::freeGc ()
{
	if (gc)
		XFreeGC (pDisplay, gc);
	gc = 0;
}
#endif

//-----------------------------------------------------------------------------
CDrawContext* CFrame::createDrawContext ()
{
	if (pFrameContext)
	{
		pFrameContext->remember ();
		return pFrameContext;
	}
	CDrawContext* pContext = 0;
	#if WINDOWS
	pContext = new CDrawContext (this, NULL, getSystemWindow ());

	#elif MAC
	pContext = new CDrawContext (this, NULL, getSystemWindow ());

	#elif MOTIF
	pContext = new CDrawContext (this, gc, (void*)window);

	#elif BEOS
	pContext = new CDrawContext (this, pPlugView, 0);
	#endif
	
	return pContext;
}

//-----------------------------------------------------------------------------
void CFrame::draw (CDrawContext *pContext)
{
	if (bFirstDraw)
		bFirstDraw = false;
	
	if (!pContext)
		pContext = pFrameContext;

	// draw the background and the children
	CViewContainer::draw (pContext);
}

//-----------------------------------------------------------------------------
void CFrame::drawRect (CDrawContext *pContext, const CRect& updateRect)
{
	if (bFirstDraw)
		bFirstDraw = false;

	bool localContext = false;	
	if (!pContext)
	{
		pContext = pFrameContext;
		if (!pContext)
		{
			localContext = true;
			pContext = createDrawContext ();
		}
	}

	#if USE_CLIPPING_DRAWRECT
	CRect oldClip;
	pContext->getClipRect (oldClip);
	CRect newClip (updateRect);
	newClip.bound (oldClip);
	pContext->setClipRect (newClip);
	#endif
	
	// draw the background and the children
	CViewContainer::drawRect (pContext, updateRect);

	#if USE_CLIPPING_DRAWRECT
	pContext->setClipRect (oldClip);
	#endif

	if (localContext)
		pContext->forget ();
}

//-----------------------------------------------------------------------------
void CFrame::draw (CView *pView)
{
	CView *pViewToDraw = 0;

		// Search it in the view list
	if (pView && isChild(pView))
	{
		pViewToDraw = pView;
	}

	bool localContext = false;	
	CDrawContext *pContext = pFrameContext;
	if (!pContext)
	{
		localContext = true;
		pContext = createDrawContext ();
	}

	if (pContext)
	{
		if (pViewToDraw)
			pViewToDraw->draw (pContext);
		else
			draw (pContext);

		if (localContext)
			pContext->forget ();
	}
}

//-----------------------------------------------------------------------------
void CFrame::mouse (CDrawContext *pContext, CPoint &where, long buttons)
{
	if (!pContext)
		pContext = pFrameContext;
	
	if (pEditView)
	{
		pEditView->looseFocus ();
		pEditView = 0;
	}

	if (buttons == -1 && pContext)
		buttons = pContext->getMouseButtons ();

	if (pModalView)
	{
		if (pModalView->hitTest (where, buttons))
			pModalView->mouse (pContext, where, buttons);
	}
	else 
	{
		CViewContainer::mouse (pContext, where, buttons);
	}
}

//-----------------------------------------------------------------------------
long CFrame::onKeyDown (VstKeyCode& keyCode)
{
	long result = -1;

	if (pEditView)
		result = pEditView->onKeyDown (keyCode);

	if (result == -1 && pModalView)
		result = pModalView->onKeyDown (keyCode);

	if (result == -1)
	{
		result = CViewContainer::onKeyDown(keyCode);
	}

	return result;
}

//-----------------------------------------------------------------------------
long CFrame::onKeyUp (VstKeyCode& keyCode)
{
	long result = -1;

	if (pEditView)
		result = pEditView->onKeyUp (keyCode);

	if (result == -1 && pModalView)
		result = pModalView->onKeyUp (keyCode);

	if (result == -1)
	{
		result = CViewContainer::onKeyUp(keyCode);
	}

	return result;
}

//-----------------------------------------------------------------------------
bool CFrame::onDrop (void **ptrItems, long nbItems, long type, CPoint &where)
{
	if (pModalView || pEditView)
		return false;

	return CViewContainer::onDrop(ptrItems, nbItems, type, where);
}

//-----------------------------------------------------------------------------
bool CFrame::onWheel (CDrawContext *pContext, const CPoint &where, float distance)
{
	bool result = false;

	CView *view = pModalView ? pModalView : getViewAt (where);
	if (view)
	{
		CDrawContext *pContext2;
		if (pContext)
			pContext2 = pContext;
		else
			pContext2 = pFrameContext;

		bool localContext = false;
		if (!pContext2)
		{
			localContext = true;
			pContext = createDrawContext ();
		}

		result = view->onWheel (pContext2, where, distance);

		if (localContext)
			pContext2->forget ();
	
	#if BEOS
		pPlugView->UnlockLooper ();
	#endif
	}
	return result;
}
		
//-----------------------------------------------------------------------------
void CFrame::update (CDrawContext *pContext)
{
	if (!getOpenFlag ())
		return;

	#if WINDOWS && USE_ALPHA_BLEND
	CDrawContext* oldFrameContext = pFrameContext;
	COffscreenContext* dc = new COffscreenContext (this, size.width (), size.height ());
	dc->copyTo (pContext, size);
	pFrameContext = dc;
	#else
	CDrawContext* dc = pContext;
	#endif

	if (bDirty)
	{
		draw (dc);
		setDirty (false);
	}
	else
	{
		#if USE_CLIPPING_DRAWRECT
		CRect oldClipRect;
		dc->getClipRect (oldClipRect);
		#endif
		FOREACHSUBVIEW
			#if USE_CLIPPING_DRAWRECT
			CRect viewSize;
			viewSize = pV->getViewSize (viewSize);
			viewSize.bound (oldClipRect);
			dc->setClipRect (viewSize);
			#endif
			pV->update (dc);
		ENDFOR
		#if USE_CLIPPING_DRAWRECT
		dc->setClipRect (oldClipRect);
		#endif
	}

	#if MACX && !QUARTZ
	if (QDIsPortBufferDirty (GetWindowPort ((WindowRef)pSystemWindow)))
	{
		QDFlushPortBuffer (GetWindowPort ((WindowRef)pSystemWindow), NULL);
	}
	#endif
	#if WINDOWS && USE_ALPHA_BLEND
	dc->copyFrom (pContext, size);
	delete dc;
	pFrameContext = oldFrameContext;
	#endif
}

//-----------------------------------------------------------------------------
bool CFrame::isSomethingDirty ()
{
	if (/*pModalView ||*/ isDirty ())
		return true;
	else
	{
		FOREACHSUBVIEW
			if (pV->isDirty ())
				return true;
		ENDFOR
	}
	return false;
}

//-----------------------------------------------------------------------------
void CFrame::idle ()
{
	if (!getOpenFlag ())
		return;

	// don't do an idle before a draw
	if (bFirstDraw)
		return;

	if (!isSomethingDirty ())
		return;
		
	bool localContext = false;
	CDrawContext *pContext = pFrameContext;
	if (!pContext)
	{
		localContext = true;
		#if BEOS
		if (pPlugView->LockLooperWithTimeout (0) != B_OK)
			return;
		#endif
		pContext = createDrawContext ();
	}

	update (pContext);

	if (localContext)
		pContext->forget ();

	#if BEOS
	pPlugView->UnlockLooper ();
	#endif
}

//-----------------------------------------------------------------------------
void CFrame::doIdleStuff ()
{
#if PLUGGUI
	if (pEditor)
		((PluginGUIEditor*)pEditor)->doIdleStuff ();
#else
	if (pEditor)
		((AEffGUIEditor*)pEditor)->doIdleStuff ();
#endif
}

//-----------------------------------------------------------------------------
unsigned long CFrame::getTicks ()
{
#if PLUGGUI
	if (pEditor)
		return ((PluginGUIEditor*)pEditor)->getTicks ();
#else
	if (pEditor)
		return ((AEffGUIEditor*)pEditor)->getTicks ();
#endif
	return 0;
}

//-----------------------------------------------------------------------------
long CFrame::getKnobMode ()
{
#if PLUGGUI
	return PluginGUIEditor::getKnobMode ();
#else
	return AEffGUIEditor::getKnobMode ();
#endif
}

//-----------------------------------------------------------------------------
#if WINDOWS
HWND CFrame::getOuterWindow ()
{
	int diffWidth, diffHeight;
	RECT  rctTempWnd, rctPluginWnd;
	HWND  hTempWnd = (HWND)pHwnd;
	GetWindowRect (hTempWnd, &rctPluginWnd);
    
	while (hTempWnd != NULL)
	{
		// Looking for caption bar
		if (GetWindowLong (hTempWnd, GWL_STYLE) & WS_CAPTION)
			return hTempWnd;

		// Looking for last parent
		if (!GetParent (hTempWnd))
			return hTempWnd;
    
		// get difference between plugin-window and current parent
		GetWindowRect (GetParent (hTempWnd), &rctTempWnd);
	    
		diffWidth  = (rctTempWnd.right - rctTempWnd.left) - (rctPluginWnd.right - rctPluginWnd.left);
		diffHeight = (rctTempWnd.bottom - rctTempWnd.top) - (rctPluginWnd.bottom - rctPluginWnd.top);
		
		// Looking for size mismatch
		if ((abs (diffWidth) > 60) || (abs (diffHeight) > 60)) // parent belongs to host
			return (hTempWnd);

		if (diffWidth < 0)
			diffWidth = 0;
        if (diffHeight < 0)
			diffHeight = 0; 
		
		// get the next parent window
		hTempWnd = GetParent (hTempWnd);
	}

	return NULL;
}
#endif

//-----------------------------------------------------------------------------
bool CFrame::getPosition (long &x, long &y)
{
	if (!getOpenFlag ())
		return false;
	
	// get the position of the Window including this frame in the main pWindow
#if WINDOWS
	HWND wnd = (HWND)getOuterWindow ();
	HWND wndParent = GetParent (wnd);

	RECT  rctTempWnd;
	GetWindowRect (wnd, &rctTempWnd);

	POINT point;
	point.x = rctTempWnd.left;
	point.y = rctTempWnd.top;

	MapWindowPoints (HWND_DESKTOP, wndParent, &point, 1);
	
	x = point.x;
	y = point.y;

#elif MAC
	Rect bounds;
	GetWindowBounds ((WindowRef)pSystemWindow, kWindowContentRgn, &bounds);
	
	x   = bounds.left;
	y   = bounds.top;

	#if QUARTZ
	HIViewRef contentView = 0;
	HIViewFindByID (HIViewGetRoot ((WindowRef)pSystemWindow), kHIViewWindowContentID, &contentView);
	if (contentView)
	{
		HIPoint p = { 0.f, 0.f };
		if (HIViewConvertPoint (&p, controlRef, contentView) == noErr)
		{
			x += p.x;
			y += p.y;
		}
	}
	#endif

#elif MOTIF
	Position xWin, yWin;

	// get the topLevelShell of the pSystemWindow
	Widget parent = (Widget)getSystemWindow ();
	Widget parentOld = parent;
	while (parent != 0 && !XtIsTopLevelShell (parent))
	{
		parentOld = parent;
		parent = XtParent (parent);
	}

	if (parent == 0)
		parent = parentOld;

	if (parent)
	{
		XtVaGetValues (parent, XtNx, &xWin, XtNy, &yWin, NULL);
		x = xWin - 8;
		y = yWin - 30;
	}

#elif BEOS
	BRect frame = pPlugView->Window ()->Frame ();
	x = (long) frame.left;
	y = (long) frame.top;
#endif
	return true;
}

//-----------------------------------------------------------------------------
void CFrame::setViewSize (CRect& inRect)
{
	setSize (inRect.width (), inRect.height ());
}

//-----------------------------------------------------------------------------
bool CFrame::setSize (long width, long height)
{
	if (!getOpenFlag ())
		return false;
	
	if ((width == size.width ()) && (height == size.height ()))
	 return false;

#if !PLUGGUI
	if (pEditor)
	{
		AudioEffectX* effect = (AudioEffectX*)((AEffGUIEditor*)pEditor)->getEffect ();
		if (effect && effect->canHostDo ("sizeWindow"))
		{
			if (effect->sizeWindow (width, height))
			{
				size.right = size.left + width;
				size.bottom = size.top + height;

				#if WINDOWS
				SetWindowPos ((HWND)pHwnd, HWND_TOP, 0, 0, width, height, SWP_NOMOVE);
				
				#elif (MAC && QUARTZ)
				Rect bounds;
				CRect2Rect (size, bounds);
				SetControlBounds (controlRef, &bounds);
				#endif
				
				return true;
			}
		}
	}
#endif

	// keep old values
	long oldWidth  = size.width ();
	long oldHeight = size.height ();

	// set the new size
	size.right  = size.left + width;
	size.bottom = size.top  + height;

#if WINDOWS
	RECT  rctTempWnd, rctParentWnd;
	HWND  hTempWnd;
	long   iFrame = (2 * GetSystemMetrics (SM_CYFIXEDFRAME));
	
	long diffWidth  = 0;
	long diffHeight = 0;
	
	hTempWnd = (HWND)pHwnd;
	
	while ((diffWidth != iFrame) && (hTempWnd != NULL)) // look for FrameWindow
	{
		HWND hTempParentWnd = GetParent (hTempWnd);
		char buffer[1024];
		GetClassName (hTempParentWnd, buffer, 1024);
		if (!hTempParentWnd || !strcmp (buffer, "MDIClient"))
			break;
		GetWindowRect (hTempWnd, &rctTempWnd);
		GetWindowRect (hTempParentWnd, &rctParentWnd);
		
		SetWindowPos (hTempWnd, HWND_TOP, 0, 0, width + diffWidth, height + diffHeight, SWP_NOMOVE);
		
		diffWidth  += (rctParentWnd.right - rctParentWnd.left) - (rctTempWnd.right - rctTempWnd.left);
		diffHeight += (rctParentWnd.bottom - rctParentWnd.top) - (rctTempWnd.bottom - rctTempWnd.top);
		
		if ((diffWidth > 80) || (diffHeight > 80)) // parent belongs to host
			return true;

		if (diffWidth < 0)
			diffWidth = 0;
        if (diffHeight < 0)
			diffHeight = 0;
		
		hTempWnd = hTempParentWnd;
	}
	
	if (hTempWnd)
		SetWindowPos (hTempWnd, HWND_TOP, 0, 0, width + diffWidth, height + diffHeight, SWP_NOMOVE);

#elif MAC
	if (getSystemWindow ())
	{
		Rect bounds;
		GetPortBounds (GetWindowPort ((WindowRef)getSystemWindow ()), &bounds);
		SizeWindow ((WindowRef)getSystemWindow (), (bounds.right - bounds.left) - oldWidth + width,
								(bounds.bottom - bounds.top) - oldHeight + height, true);
		#if MACX
		SetPort (GetWindowPort ((WindowRef)getSystemWindow ()));
		#endif
		#if QUARTZ
		CRect2Rect (size, bounds);
		SetControlBounds (controlRef, &bounds);
		#endif
	}

#elif MOTIF
	Dimension heightWin, widthWin;

	// get the topLevelShell of the pSystemWindow
	Widget parent = (Widget)getSystemWindow ();
	Widget parentOld = parent;
	while (parent != 0 && !XtIsTopLevelShell (parent))
	{
		parentOld = parent;
		parent = XtParent (parent);
	}

	if (parent == 0)
		parent = parentOld;
	if (parent)
	{
		XtVaGetValues (parent, XtNwidth, &widthWin, XtNheight, &heightWin, NULL);	
		long diffWidth  = widthWin - oldWidth;
		long diffHeight = heightWin - oldHeight;
		XtVaSetValues (parent, XmNwidth, width + diffWidth, 
									 XmNheight, height + diffHeight, NULL);
	}

#elif BEOS
	BView* parent = pPlugView->Parent ();
	parent->SetResizingMode (B_FOLLOW_ALL_SIDES);
	BRect frame = pPlugView->Frame ();
	pPlugView->Window ()->ResizeBy (width - frame.Width () - 1, height - frame.Height () - 1);
	parent->SetResizingMode (B_FOLLOW_NONE);
#endif

	CRect myViewSize (0, 0, size.width (), size.height ());
	CViewContainer::setViewSize (myViewSize);

	return true;
}

//-----------------------------------------------------------------------------
bool CFrame::getSize (CRect *pRect)
{
	if (!getOpenFlag ())
		return false;

#if WINDOWS
	// return the size relative to the client rect of this window
	// get the main window
	HWND wnd = GetParent ((HWND)getSystemWindow ());
	HWND wndParent = GetParent (wnd);
	HWND wndParentParent = GetParent (wndParent);

	RECT  rctTempWnd;
	GetWindowRect (wnd, &rctTempWnd);
	
	POINT point;
	point.x = rctTempWnd.left;
	point.y = rctTempWnd.top;

	MapWindowPoints (HWND_DESKTOP, wndParentParent, &point, 1);
	
	pRect->left   = point.x;
	pRect->top    = point.y;
	pRect->right  = pRect->left + rctTempWnd.right - rctTempWnd.left;
	pRect->bottom = pRect->top  + rctTempWnd.bottom - rctTempWnd.top;

#elif MAC
	Rect bounds;
	GetPortBounds (GetWindowPort ((WindowRef)getSystemWindow ()), &bounds);

	pRect->left   = bounds.left;
	pRect->top    = bounds.top;
	pRect->right  = bounds.right;
	pRect->bottom = bounds.bottom;
	
#elif MOTIF
	Dimension height, width;
	XtVaGetValues ((Widget)getSystemWindow (),
								 XtNwidth, &width, XtNheight, &height, NULL);

	Position x, y;
	Position xTotal = 0, yTotal = 0;
	Widget parent = (Widget)getSystemWindow ();
	while (parent != 0 && !XtIsTopLevelShell (parent) && !XmIsDialogShell (parent))
	{
		XtVaGetValues (parent, XtNx, &x, XtNy, &y, NULL);	
		xTotal += x;
		yTotal += y;
		parent = XtParent (parent);
	}

	pRect->left   = xTotal;
	pRect->top    = yTotal;
	pRect->right  = width + pRect->left;
	pRect->bottom = height + pRect->top;

#elif BEOS
	BRect v = pPlugView->Frame ();
	(*pRect) (v.left, v.top, v.right + 1, v.bottom + 1);
#endif
	return true;
}

//-----------------------------------------------------------------------------
bool CFrame::getSize (CRect& outSize)
{
	return getSize (&outSize);
}

//-----------------------------------------------------------------------------
long CFrame::setModalView (CView *pView)
{
	// There's already a modal view so we get out
	if (pView && pModalView)
			return 0;

	if (pModalView)
		removeView (pModalView, false);
	
	pModalView = pView;
	if (pModalView)
		addView (pModalView);

	return 1;
}

//-----------------------------------------------------------------------------
void CFrame::beginEdit (long index)
{
#if PLUGGUI
	#if AU
	if (pEditor)
		((PluginGUIEditor*)pEditor)->beginEdit (index);
	#endif
#else
	if (pEditor)
		((AEffGUIEditor*)pEditor)->beginEdit (index);
//		((AudioEffectX*)(((AEffGUIEditor*)pEditor)->getEffect ()))->beginEdit (index);
#endif
}

//-----------------------------------------------------------------------------
void CFrame::endEdit (long index)
{
#if PLUGGUI
	#if AU
	if (pEditor)
		((PluginGUIEditor*)pEditor)->endEdit (index);
	#endif
#else
	if (pEditor)
		((AEffGUIEditor*)pEditor)->endEdit (index);
//		((AudioEffectX*)(((AEffGUIEditor*)pEditor)->getEffect ()))->endEdit (index);
#endif
}

//-----------------------------------------------------------------------------
CView *CFrame::getCurrentView ()
{
	if (pModalView)
		return pModalView;
	
	return CViewContainer::getCurrentView ();
}

//-----------------------------------------------------------------------------
bool CFrame::getCurrentLocation (CPoint &where)
{
#if WINDOWS
	HWND hwnd = (HWND)this->getSystemWindow ();
	POINT _where;
	GetCursorPos (&_where);
	where (_where.x, _where.y);
	if (hwnd)
	{
		RECT rctTempWnd;
		GetWindowRect (hwnd, &rctTempWnd);
		where.offset (-rctTempWnd.left, -rctTempWnd.top);
	}
	return true;
#endif

	// create a local context
	CDrawContext *pContextTemp = createDrawContext ();

	// get the current position
	if (pContextTemp)
	{
		pContextTemp->getMouseLocation (where);
		pContextTemp->forget ();
	}
	return true;
}

#if MACX
#define kThemeResizeUpDownCursor      21
#endif
//-----------------------------------------------------------------------------
void CFrame::setCursor (CCursorType type)
{
	#if WINDOWS
	if (!defaultCursor)
		defaultCursor = GetCursor ();
	switch (type)
	{
		case kCursorDefault:
			SetCursor ((HCURSOR)defaultCursor);
			break;
		case kCursorWait:
			SetCursor (LoadCursor (0, IDC_WAIT));
			break;
		case kCursorHSize:
			SetCursor (LoadCursor (0, IDC_SIZEWE));
			break;
		case kCursorVSize:
			SetCursor (LoadCursor (0, IDC_SIZENS));
			break;
		case kCursorNESWSize:
			SetCursor (LoadCursor (0, IDC_SIZENESW));
			break;
		case kCursorNWSESize:
			SetCursor (LoadCursor (0, IDC_SIZENWSE));
			break;
		case kCursorSizeAll:
			SetCursor (LoadCursor (0, IDC_SIZEALL));
			break;
	}
	#elif MAC
	#if MACX
	switch (type)
	{
		case kCursorDefault:
			SetThemeCursor (kThemeArrowCursor);
			break;
		case kCursorWait:
			SetThemeCursor (kThemeWatchCursor);
			break;
		case kCursorHSize:
			SetThemeCursor (pSystemVersion < 0x1030 ? kThemeCrossCursor : kThemeResizeLeftRightCursor);
			break;
		case kCursorVSize:
			SetThemeCursor (pSystemVersion < 0x1030 ? kThemeCrossCursor : kThemeResizeUpDownCursor);
			break;
		case kCursorNESWSize:
			SetThemeCursor (kThemeCrossCursor);
			break;
		case kCursorNWSESize:
			SetThemeCursor (kThemeCrossCursor);
			break;
		case kCursorSizeAll:
			SetThemeCursor (kThemeCrossCursor);
			break;
	}
	#else
	//if (!defaultCursor)
	//	defaultCursor = GetCursor (0);
	switch (type)
	{
		case kCursorDefault:
			InitCursor ();
			break;
		case kCursorWait:
			SetCursor (*GetCursor (watchCursor));
			break;
		case kCursorHSize:
			SetCursor (*GetCursor (crossCursor));
			break;
		case kCursorVSize:
			SetCursor (*GetCursor (crossCursor));
			break;
		case kCursorNESWSize:
			SetCursor (*GetCursor (crossCursor));
			break;
		case kCursorNWSESize:
			SetCursor (*GetCursor (crossCursor));
			break;
		case kCursorSizeAll:
			SetCursor (*GetCursor (plusCursor));
			break;
	}
	#endif
	#endif
}

//-----------------------------------------------------------------------------
void CFrame::setEditView (CView *pView)
{
	CView *pOldEditView = pEditView;
	pEditView = pView;

	if (pOldEditView)
		pOldEditView->looseFocus ();
}

//-----------------------------------------------------------------------------
void CFrame::invalidate (const CRect &rect)
{
	CRect rectView;
	FOREACHSUBVIEW
	if (pV)
	{
		pV->getViewSize (rectView);
		if (rect.rectOverlap (rectView))
			pV->setDirty (true);
	}
	ENDFOR
}

//-----------------------------------------------------------------------------
// CCView Implementation
//-----------------------------------------------------------------------------
CCView::CCView (CView *pView)
 :  pView (pView), pNext (0), pPrevious (0)
{
	if (pView)
		pView->remember ();
}

//-----------------------------------------------------------------------------
CCView::~CCView ()
{ 
	if (pView)
		pView->forget (); 
}

//-----------------------------------------------------------------------------
// CViewContainer Implementation
//-----------------------------------------------------------------------------
CViewContainer::CViewContainer (const CRect &rect, CFrame *pParent, CBitmap *pBackground)
: CView (rect), pFirstView (0), pLastView (0), 
 mode (kNormalUpdate), pOffscreenContext (0), bDrawInOffscreen (true)
{
	#if MACX || USE_ALPHA_BLEND
	bDrawInOffscreen = false;
	#endif
	backgroundOffset (0, 0);
	this->pParent = pParent;
	setBackground (pBackground);
	backgroundColor = kBlackCColor;	
}

//-----------------------------------------------------------------------------
CViewContainer::~CViewContainer ()
{
	// remove all views
	removeAll (true);

	#if !BEOS
	 if (pOffscreenContext)
		pOffscreenContext->forget ();
	pOffscreenContext = 0;
	#endif
}

//-----------------------------------------------------------------------------
void CViewContainer::setViewSize (CRect &rect)
{
	CView::setViewSize (rect);

	#if !BEOS
	if (pOffscreenContext && bDrawInOffscreen)
	{
		pOffscreenContext->forget ();
		pOffscreenContext = new COffscreenContext (pParent, size.width (), size.height (), kBlackCColor);
	}
	#endif
}

//-----------------------------------------------------------------------------
void CViewContainer::setBackgroundColor (CColor color)
{
	backgroundColor = color;
}

//------------------------------------------------------------------------------
long CViewContainer::notify (CView* sender, const char* message)
{
	if (message == kMsgCheckIfViewContainer)
		return kMessageNotified;
	return kMessageUnknown;
}

//-----------------------------------------------------------------------------
void CViewContainer::addView (CView *pView)
{
	if (!pView)
		return;

	CCView *pSv = new CCView (pView);
	
	pView->pParent = pParent;
	pView->pParentView = this;

	CCView *pV = pFirstView;
	if (!pV)
	{
		pLastView = pFirstView = pSv;
	}
	else
	{
		while (pV->pNext)
			pV = pV->pNext;
		pV->pNext = pSv;
		pSv->pPrevious = pV;
		pLastView = pSv;
	}
	pView->attached (this);
	pView->setDirty ();
}

//-----------------------------------------------------------------------------
void CViewContainer::addView (CView *pView, CRect &mouseableArea, bool mouseEnabled)
{
	if (!pView)
		return;

	pView->setMouseEnabled (mouseEnabled);
	pView->setMouseableArea (mouseableArea);

	addView (pView);
}

//-----------------------------------------------------------------------------
void CViewContainer::removeAll (const bool &withForget)
{
	CCView *pV = pFirstView;
	while (pV)
	{
		CCView *pNext = pV->pNext;
		if (pV->pView)
		{
			pV->pView->removed (this);
			if (withForget)
				pV->pView->forget ();
		}

		delete pV;

		pV = pNext;
	}
	pFirstView = 0;
	pLastView = 0;
}

//-----------------------------------------------------------------------------
void CViewContainer::removeView (CView *pView, const bool &withForget)
{
	CCView *pV = pFirstView;
	while (pV)
	{
		if (pView == pV->pView)
		{
			CCView *pNext = pV->pNext;
			CCView *pPrevious = pV->pPrevious;
			if (pV->pView)
			{
				pV->pView->removed (this);
				if (withForget)
					pV->pView->forget ();
			}
			delete pV;
			if (pPrevious)
			{
				pPrevious->pNext = pNext;
				if (pNext)
					pNext->pPrevious = pPrevious;
				else
					pLastView = pPrevious;
			}
			else
			{
				pFirstView = pNext;
				if (pNext)
					pNext->pPrevious = 0;
				else
					pLastView = 0;	
			}
			break;
		}
		else
			pV = pV->pNext;
	}
}

//-----------------------------------------------------------------------------
bool CViewContainer::isChild (CView *pView)
{
	bool found = false;

	CCView *pV = pFirstView;
	while (pV)
	{
		if (pView == pV->pView)
		{
			found = true;
			break;
		}
		pV = pV->pNext;
	}
	return found;
}

//-----------------------------------------------------------------------------
long CViewContainer::getNbViews ()
{
	long nb = 0;
	CCView *pV = pFirstView;
	while (pV)
	{
		pV = pV->pNext;
		nb++;
	}
	return nb;
}

//-----------------------------------------------------------------------------
CView *CViewContainer::getView (long index)
{
	long nb = 0;
	CCView *pV = pFirstView;
	while (pV)
	{
		if (nb == index)
			return pV->pView;
		pV = pV->pNext;
		nb++;
	}
	return 0;
}

//-----------------------------------------------------------------------------
void CViewContainer::draw (CDrawContext *pContext)
{
	CDrawContext *pC;
	long save[4];

	#if BEOS
	// create offscreen
	if (pBackground)
		pC = new COffscreenContext (pContext, pBackground);
	else
		pC = new COffscreenContext (pParent, size.width (), size.height (), backgroundColor);
	
	#else
	if (!pOffscreenContext && bDrawInOffscreen)
		pOffscreenContext = new COffscreenContext (pParent, size.width (), size.height (), kBlackCColor);
	#if USE_ALPHA_BLEND
	if (pOffscreenContext && bTransparencyEnabled)
		pOffscreenContext->copyTo (pContext, size);
	#endif

	if (bDrawInOffscreen)
		pC = pOffscreenContext;
	else
	{
		pC = pContext;
		modifyDrawContext (save, pContext);
	}

	CRect r (0, 0, size.width (), size.height ());

 	#if USE_CLIPPING_DRAWRECT
	CRect oldClip;
	pContext->getClipRect (oldClip);
	CRect newClip (r);
	newClip.bound (oldClip);
	pContext->setClipRect (newClip);
	#endif

	// draw the background
	if (pBackground)
	{
		if (bTransparencyEnabled)
			pBackground->drawTransparent (pC, r, backgroundOffset);
		else
			pBackground->draw (pC, r, backgroundOffset);
	}
	else if (!bTransparencyEnabled)
	{
		pC->setFillColor (backgroundColor);
		pC->fillRect (r);
	}
	#endif
	
	// draw each view
	FOREACHSUBVIEW
		#if USE_CLIPPING_DRAWRECT
		CRect vSize;
		pV->getViewSize (vSize);
		vSize.bound (oldClip);
		pContext->setClipRect (vSize);
		#endif
		pV->draw (pC);
	ENDFOR

	#if USE_CLIPPING_DRAWRECT
	pContext->setClipRect (oldClip);
	#endif
	
	// transfert offscreen
	if (bDrawInOffscreen)
		((COffscreenContext*)pC)->copyFrom (pContext, size);
	else
		restoreDrawContext (pContext, save);

	#if BEOS
	delete pC;
	#endif

	setDirty (false);
}

//-----------------------------------------------------------------------------
void CViewContainer::drawBackgroundRect (CDrawContext *pContext, CRect& _updateRect)
{
	if (pBackground)
	{
		CRect oldClip;
		pContext->getClipRect (oldClip);
		CRect newClip (_updateRect);
		newClip.bound (oldClip);
		pContext->setClipRect (newClip);
		CRect tr (0, 0, pBackground->getWidth (), pBackground->getHeight ());
		if (bTransparencyEnabled)
			pBackground->drawTransparent (pContext, tr, backgroundOffset);
		else
			pBackground->draw (pContext, tr, backgroundOffset);
		pContext->setClipRect (oldClip);
	}
	else if (!bTransparencyEnabled)
	{
		pContext->setFillColor (backgroundColor);
		pContext->fillRect (_updateRect);
	}
}

//-----------------------------------------------------------------------------
void CViewContainer::drawRect (CDrawContext *pContext, const CRect& _updateRect)
{
	CDrawContext *pC;
	long save[4];

	#if BEOS
	// create offscreen
	if (pBackground)
		pC = new COffscreenContext (pContext, pBackground);
	else
		pC = new COffscreenContext (pParent, size.width (), size.height (), backgroundColor);
	
	#else
	if (!pOffscreenContext && bDrawInOffscreen)
		pOffscreenContext = new COffscreenContext (pParent, size.width (), size.height (), kBlackCColor);
	#if USE_ALPHA_BLEND
	if (pOffscreenContext && bTransparencyEnabled)
		pOffscreenContext->copyTo (pContext, size);
	#endif

	if (bDrawInOffscreen)
		pC = pOffscreenContext;
	else
	{
		pC = pContext;
		modifyDrawContext (save, pContext);
	}

	CRect updateRect (_updateRect);
	updateRect.bound (size);

	CRect clientRect (updateRect);
	clientRect.offset (-size.left, -size.top);

	#if USE_CLIPPING_DRAWRECT
	CRect oldClip;
	pContext->getClipRect (oldClip);
	CRect newClip (clientRect);
	newClip.bound (oldClip);
	pContext->setClipRect (newClip);
	#endif
	
	// draw the background
	drawBackgroundRect (pC, clientRect);
	#endif
	
	// draw each view
	FOREACHSUBVIEW
		if (pV->checkUpdate (clientRect))
			pV->drawRect (pC, clientRect);
	ENDFOR

	#if USE_CLIPPING_DRAWRECT
	pContext->setClipRect (oldClip);
	#endif

	// transfert offscreen
	if (bDrawInOffscreen)
		((COffscreenContext*)pC)->copyFrom (pContext, updateRect, CPoint (clientRect.left, clientRect.top));
	else
		restoreDrawContext (pContext, save);

	#if BEOS
	delete pC;
	#endif

	setDirty (false);
}

//-----------------------------------------------------------------------------
void CViewContainer::redrawRect (CDrawContext* context, const CRect& rect)
{
	CRect _rect (rect);
	_rect.offset (size.left, size.top);
	if (bTransparencyEnabled)
	{
		// as this is transparent, we call the parentview to redraw this area.
		if (pParentView)
			pParentView->redrawRect (context, _rect);
		else if (pParent)
			pParent->drawRect (context, _rect);
	}
	else
		drawRect (context, _rect);
}

//-----------------------------------------------------------------------------
bool CViewContainer::hitTestSubViews (const CPoint& where, const long buttons)
{
	CPoint where2 (where);
	where2.offset (-size.left, -size.top);

	CCView *pSv = pLastView;
	while (pSv)
	{
		CView *pV = pSv->pView;
		if (pV && pV->getMouseEnabled () && pV->hitTest (where2, buttons))
			return true;
		pSv = pSv->pPrevious;
	}
	return false;
}

//-----------------------------------------------------------------------------
bool CViewContainer::hitTest (const CPoint& where, const long buttons)
{
	//return hitTestSubViews (where); would change default behavior
	return CView::hitTest (where, buttons);
}

//-----------------------------------------------------------------------------
void CViewContainer::mouse (CDrawContext *pContext, CPoint &where, long buttons)
{
	// convert to relativ pos
	CPoint where2 (where);
	where2.offset (-size.left, -size.top);

	if (buttons == -1 && pContext)
		buttons = pContext->getMouseButtons ();

	CCView *pSv = pLastView;
	while (pSv)
	{
		CView *pV = pSv->pView;
		if (pV && pV->getMouseEnabled () && pV->hitTest (where2, buttons))
		{
			pV->mouse (pContext, where2, buttons);
			break;
		}
		pSv = pSv->pPrevious;
	}
}

//-----------------------------------------------------------------------------
long CViewContainer::onKeyDown (VstKeyCode& keyCode)
{
	long result = -1;

	CCView* pSv = pLastView;
	while (pSv)
	{
		long result = pSv->pView->onKeyDown (keyCode);
		if (result != -1)
			break;

		pSv = pSv->pPrevious;
	}

	return result;
}

//-----------------------------------------------------------------------------
long CViewContainer::onKeyUp (VstKeyCode& keyCode)
{
	long result = -1;

	CCView* pSv = pLastView;
	while (pSv)
	{
		long result = pSv->pView->onKeyUp (keyCode);
		if (result != -1)
			break;

		pSv = pSv->pPrevious;
	}

	return result;
}

//-----------------------------------------------------------------------------
bool CViewContainer::onDrop (void **ptrItems, long nbItems, long type, CPoint &where)
{
	if (!pParent)
		return false;

	// convert to relativ pos
	CPoint where2 (where);
	where2.offset (-size.left, -size.top);

	bool result = false;
	CCView *pSv = pLastView;
	while (pSv)
	{
		CView *pV = pSv->pView;
		if (pV && pV->getMouseEnabled () && where2.isInside (pV->mouseableArea))
		{
			if (pV->onDrop (ptrItems, nbItems, type, where2))
			{
				result = true;
				break;
			}
		}
		pSv = pSv->pPrevious;
	}
	return result;
}

//-----------------------------------------------------------------------------
bool CViewContainer::onWheel (CDrawContext *pContext, const CPoint &where, float distance)
{
	bool result = false;
	CView *view = getViewAt (where);
	if (view)
	{
		// convert to relativ pos
		CPoint where2 (where);
		where2.offset (-size.left, -size.top);

		long save[4];
		modifyDrawContext (save, pContext);
	
		result = view->onWheel (pContext, where2, distance);

		restoreDrawContext (pContext, save);
	}
	return result;
}

//-----------------------------------------------------------------------------
void CViewContainer::update (CDrawContext *pContext)
{
	switch (mode)
	{
		//---Normal : redraw all...
		case kNormalUpdate:
			if (isDirty ())
			{
				#if NEW_UPDATE_MECHANISM
				CRect ur (0, 0, size.width (), size.height ());
				redrawRect (pContext, ur);
				#else
				#if USE_ALPHA_BLEND
				if (bTransparencyEnabled)
				{
					CPoint topLeft;
					getFrameTopLeftPos (topLeft);
					topLeft.offset (-size.left, -size.top);
					CRect updateRect (size);
					updateRect.offset (topLeft.x, topLeft.y);
					getParent ()->drawRect (pContext, updateRect);
				}
				else
				#endif
				draw (pContext);
				#endif // !NEW_UPDATE_MECHANISM
				setDirty (false);
			}
		break;
	
		//---Redraw only dirty controls-----
		case kOnlyDirtyUpdate:
		{
			#if NEW_UPDATE_MECHANISM
			if (bDirty)
			{
				CRect ur (0, 0, size.width (), size.height ());
				redrawRect (pContext, ur);
			}
			else
			{
				CRect updateRect (size);
				updateRect.offset (-size.left, -size.top);
				FOREACHSUBVIEW
					if (pV->isDirty () && pV->checkUpdate (updateRect))
					{
						if (pV->notify (this, kMsgCheckIfViewContainer))
							pV->update (pContext);
						else
							pV->redrawRect (pContext, pV->size);
					}
				ENDFOR
			}
			#else
			#if USE_ALPHA_BLEND
			if (bTransparencyEnabled)
			{
				if (bDirty)
				{
					CPoint topLeft;
					getFrameTopLeftPos (topLeft);
					topLeft.offset (-size.left, -size.top);
					CRect updateRect (size);
					updateRect.offset (topLeft.x, topLeft.y);
					getParent ()->drawRect (pContext, updateRect);
				}
				else
				{
					CRect updateRect (size);
					updateRect.offset (-size.left, -size.top);
					CPoint topLeft;
					getFrameTopLeftPos (topLeft);
					topLeft.offset (-size.left, -size.top);
					FOREACHSUBVIEW
						if (pV->isDirty () && pV->checkUpdate (updateRect))
						{
							if (pV->notify (this, kMsgCheckIfViewContainer))
							{
								pV->update (pContext);
							}
							else
							{
								CRect viewSize;
								pV->getViewSize (viewSize);
								viewSize.offset (size.left, size.top);
								viewSize.offset (topLeft.x, topLeft.y);
								getParent ()->drawRect (pContext, viewSize);
							}
						}
					ENDFOR
				}
				setDirty (false);
				return;
			}
			#endif
			if (bDirty)
				draw (pContext);
			else if (bDrawInOffscreen && pOffscreenContext) 
			{
				bool doCopy = false;
				if (isDirty ())
					doCopy = true;

				FOREACHSUBVIEW
					pV->update (pOffscreenContext);
				ENDFOR

				// transfert offscreen
				if (doCopy)
					pOffscreenContext->copyFrom (pContext, size);
			}
			else
			{
				long save[4];
				modifyDrawContext (save, pContext);

				FOREACHSUBVIEW
					if (pV->isDirty ())
					{
						long oldMode = 0;
						CViewContainer* child = 0;
						if (pV->notify (this, kMsgCheckIfViewContainer))
						{
							child = (CViewContainer*)pV;
							oldMode = child->getMode ();
							child->setMode (kNormalUpdate);
						}
						CRect viewSize;
						pV->getViewSize (viewSize);
						drawBackgroundRect (pContext, viewSize);
						pV->update (pContext);
						if (child)
							child->setMode (oldMode);
					}
				ENDFOR

				restoreDrawContext (pContext, save);
			}
			#endif // !NEW_UPDATE_MECHANISM
			setDirty (false);
		break;
		}
	}
}

//-----------------------------------------------------------------------------
void CViewContainer::looseFocus (CDrawContext *pContext)
{
	FOREACHSUBVIEW
		pV->looseFocus (pContext);
	ENDFOR
}

//-----------------------------------------------------------------------------
void CViewContainer::takeFocus (CDrawContext *pContext)
{
	FOREACHSUBVIEW
		pV->takeFocus (pContext);
	ENDFOR
}

//-----------------------------------------------------------------------------
bool CViewContainer::isDirty ()
{
	if (bDirty)
		return true;
		
	FOREACHSUBVIEW
		if (pV->isDirty ())
			return true;
	ENDFOR
	return false;
}

//-----------------------------------------------------------------------------
CView *CViewContainer::getCurrentView ()
{
	if (!pParent)
		return 0;

	// get the current position
	CPoint where;
	pParent->getCurrentLocation (where);

	// convert to relativ pos
	where.offset (-size.left, -size.top);

	CCView *pSv = pLastView;
	while (pSv)
	{
		CView *pV = pSv->pView;
		if (pV && where.isInside (pV->mouseableArea))
			return pV;
		pSv = pSv->pPrevious;
	}

	return 0;
}

//-----------------------------------------------------------------------------
CView *CViewContainer::getViewAt (const CPoint& p)
{
	if (!pParent)
		return 0;

	CPoint where (p);

	// convert to relativ pos
	where.offset (-size.left, -size.top);

	CCView *pSv = pLastView;
	while (pSv)
	{
		CView *pV = pSv->pView;
		if (pV && where.isInside (pV->mouseableArea))
			return pV;
		pSv = pSv->pPrevious;
	}

	return 0;
}

//-----------------------------------------------------------------------------
bool CViewContainer::removed (CView* parent)
{
	#if !BEOS
	 if (pOffscreenContext)
		pOffscreenContext->forget ();
	pOffscreenContext = 0;
	#endif

	return true;
}

//-----------------------------------------------------------------------------
bool CViewContainer::attached (CView* view)
{
	#if !BEOS
	// create offscreen bitmap
	if (!pOffscreenContext && bDrawInOffscreen)
		pOffscreenContext = new COffscreenContext (pParent, size.width (), size.height (), kBlackCColor);
	#endif

	return true;
}

//-----------------------------------------------------------------------------
void CViewContainer::useOffscreen (bool b)
{
	bDrawInOffscreen = b;
	
	#if !BEOS
	if (!bDrawInOffscreen && pOffscreenContext)
	{
		pOffscreenContext->forget ();
		pOffscreenContext = 0;
	}
	#endif
}

//-----------------------------------------------------------------------------
void CViewContainer::modifyDrawContext (long save[4], CDrawContext* pContext)
{
	// store
	save[0] = pContext->offsetScreen.h;
	save[1] = pContext->offsetScreen.v;
	save[2] = pContext->offset.h;
	save[3] = pContext->offset.v;

	pContext->offsetScreen.h += size.left;
	pContext->offsetScreen.v += size.top;
	pContext->offset.h += size.left;
	pContext->offset.v += size.top;
}

//-----------------------------------------------------------------------------
void CViewContainer::restoreDrawContext (CDrawContext* pContext, long save[4])
{
	// restore
	pContext->offsetScreen.h = save[0];
	pContext->offsetScreen.v = save[1];
	pContext->offset.h = save[2];
	pContext->offset.v = save[3];
}

#if WINDOWS && USE_LIBPNG
class PNGResourceStream
{
public:
		PNGResourceStream ()
		: streamPos (0)
		, resData (0)
		, resSize (0)
		{
		}

		~PNGResourceStream ()
		{
		}

		bool open (long resourceID)
		{
			HRSRC rsrc = FindResource (GetInstance (), MAKEINTRESOURCE (resourceID), "PNG");
			if (rsrc)
			{
				resSize = SizeofResource (GetInstance (), rsrc);
				HGLOBAL resDataLoad = LoadResource (GetInstance (), rsrc);
				if (resDataLoad)
				{
					resData = LockResource (resDataLoad);
					return true;
				}
			}
			return false;
		}

		void read (unsigned char* ptr, size_t size)
		{
			if (streamPos + size <= resSize)
			{
				memcpy (ptr, ((unsigned char*)resData+streamPos), size);
				streamPos += size;
			}
		}

		static void readCallback (png_struct* pngPtr, unsigned char* ptr, size_t size)
		{
			void* obj = png_get_io_ptr (pngPtr);
			if (obj)
				((PNGResourceStream*)obj)->read (ptr, size);
		}
protected:
	HGLOBAL resData;
	unsigned long streamPos;
	unsigned long resSize;
};
#endif

//-----------------------------------------------------------------------------
// CBitmap Implementation
//-----------------------------------------------------------------------------
CBitmap::CBitmap (long resourceID)
	: resourceID (resourceID), width (0), height (0)
{
	#if DEBUG
	gNbCBitmap++;
	#endif

#if WINDOWS
	pMask = 0;
	pHandle = 0;
	#if USE_LIBPNG
	PNGResourceStream resStream;
	if (resStream.open (resourceID))
	{
		// setup libpng
		png_structp png_ptr;
		png_infop info_ptr;
		png_ptr = png_create_read_struct (PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
		if (png_ptr)
		{
			info_ptr = png_create_info_struct (png_ptr);
			if (info_ptr)
			{
				if (setjmp (png_jmpbuf (png_ptr)) == 0)
				{
					int bit_depth, color_type;
					png_set_read_fn (png_ptr, (void *)&resStream, PNGResourceStream::readCallback);
					png_read_info (png_ptr, info_ptr);
					png_get_IHDR (png_ptr, info_ptr, (png_uint_32*)&width, (png_uint_32*)&height, &bit_depth, &color_type, 0, 0, 0);
					long bytesPerRow = width * (32 / 8);
					while (bytesPerRow & 0x03)
						bytesPerRow++;
					// create BITMAP
					BITMAPINFO* bmInfo = new BITMAPINFO;
					BITMAPINFOHEADER* header = (BITMAPINFOHEADER*)bmInfo;
					memset (header, 0, sizeof(BITMAPINFOHEADER));
					header->biSize = sizeof(BITMAPINFOHEADER);
					header->biWidth = width;
					header->biHeight = height;
					header->biPlanes = 1;
					header->biBitCount = 32;
					header->biCompression = BI_RGB;
					header->biClrUsed = 0;
					void* bits;
					pHandle = CreateDIBSection (NULL, bmInfo, DIB_RGB_COLORS, &bits, NULL, 0);
					delete bmInfo;
					if (pHandle)
					{
						if (color_type == PNG_COLOR_TYPE_PALETTE)
							png_set_palette_to_rgb (png_ptr);
						if (color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
							png_set_gray_to_rgb (png_ptr);
						if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
							png_set_gray_1_2_4_to_8 (png_ptr);
						if (png_get_valid (png_ptr, info_ptr, PNG_INFO_tRNS))
							png_set_tRNS_to_alpha (png_ptr);
						else
							png_set_filler (png_ptr, 0xFF, PNG_FILLER_AFTER);
						if (bit_depth == 16)
						{
							png_set_swap (png_ptr);
							png_set_strip_16 (png_ptr);
						}
						if (color_type == PNG_COLOR_TYPE_RGB || color_type == PNG_COLOR_TYPE_RGB_ALPHA)
							png_set_bgr (png_ptr);
						png_read_update_info (png_ptr, info_ptr);

						unsigned char** rows = new unsigned char*[1];
						rows[0] = (unsigned char*)bits + (height-1) * bytesPerRow;
						for (long i = 0; i < height; i++)
						{
							png_read_rows (png_ptr, rows, NULL, 1);
							rows[0] -= bytesPerRow;
						}
						delete [] rows;
						png_read_end (png_ptr, 0);
						// premultiply alpha
						unsigned long* pixelPtr = (unsigned long*)bits;
						for (int y = 0; y <height; y++)
						{
							for (int x = 0; x < width; x++)
							{
								unsigned char* pixel = (unsigned char*)pixelPtr;
								if (pixel[3] != 0)
								{
									pixel[0] = ((pixel[0] * pixel[3]) >> 8);
									pixel[1] = ((pixel[1] * pixel[3]) >> 8);
									pixel[2] = ((pixel[2] * pixel[3]) >> 8);
								}
								else
									*pixelPtr = 0UL;
								pixelPtr++;
							}
						}
					}
				}
			}
			png_destroy_read_struct (&png_ptr, &info_ptr, NULL);
		}
		return;
	}
	#endif
	pHandle = LoadBitmap (GetInstance (), MAKEINTRESOURCE (resourceID));
	BITMAP bm;
	if (pHandle && GetObject (pHandle, sizeof (bm), &bm))
	{
		width  = bm.bmWidth; 
		height = bm.bmHeight; 
	}

#elif MAC
	
	pHandle = 0;
	pMask = 0;
	
	#if MACX
	#if PLUGGUI
	extern CFBundleRef ghInst;
	CFBundleRef gBundleRef = ghInst;
	#endif
	if (gBundleRef)
	{
		char filename [PATH_MAX];
		sprintf (filename, "bmp%05d", (int)resourceID);
		CFStringRef cfStr = CFStringCreateWithCString (NULL, filename, kCFStringEncodingASCII);
		if (cfStr)
		{
			CFURLRef url = NULL;
			int i = 0;
			while (url == NULL)
			{
				static CFStringRef resTypes [] = { CFSTR("bmp"), CFSTR("png"), CFSTR("jpg"), CFSTR("pict"), NULL };
				url = CFBundleCopyResourceURL ((CFBundleRef)gBundleRef, cfStr, resTypes[i], NULL);
				if (resTypes[++i] == NULL)
					break;
			}
			CFRelease (cfStr);
			if (url)
			{
				FSRef fsRef;
				if (CFURLGetFSRef (url, &fsRef))
				{
					FSSpec fsSpec;
					FSCatalogInfoBitmap infoBitmap = kFSCatInfoNone;
					if (FSGetCatalogInfo (&fsRef, infoBitmap, NULL, NULL, &fsSpec, NULL) == noErr)
					{
						ComponentInstance gi;
						GetGraphicsImporterForFile (&fsSpec, &gi);
						if (gi)
						{
							Rect r;
							GraphicsImportGetSourceRect (gi, &r);
							OSErr err = NewGWorld ((GWorldPtr*)&pHandle, 32, &r, 0, 0, 0);
							if (!err)
							{
								width = r.right;
								height = r.bottom;
								GraphicsImportSetGWorld (gi, (GWorldPtr)pHandle, 0);
								GraphicsImportDraw (gi);
							}
							CloseComponent (gi);
						}
					}
				}
				CFRelease (url);
			}
			else
			{
				#if DEVELOPMENT
				fprintf (stderr, "Bitmap Nr.:%d not found.\n", resourceID);
				#endif
			}
		}
	}
	#endif
	
	if (pHandle == 0)
	{
		Handle picHandle = GetResource ('PICT', resourceID);
		if (picHandle)
		{
			HLock (picHandle);
			
			PictInfo info;
			GetPictInfo ((PicHandle)picHandle, &info, recordComments, 0, systemMethod, 0);
			width  = info.sourceRect.right;
			height = info.sourceRect.bottom;
			
			OSErr err = NewGWorld ((GWorldPtr*)&pHandle, 32, &info.sourceRect, 0, 0, 0);
			if (!err)
			{
				GWorldPtr oldPort;
				GDHandle oldDevice;
				GetGWorld (&oldPort, &oldDevice);
				SetGWorld ((GWorldPtr)pHandle, 0);
				
				DrawPicture ((PicHandle)picHandle, &info.sourceRect);
				
				SetGWorld (oldPort, oldDevice);
			}

			HUnlock (picHandle);
			ReleaseResource (picHandle);
		}
	}

#elif MOTIF
	bool found = false;
	long  i = 0;
	long  ncolors, cpp;

	pHandle = 0;
	pMask  = 0;

	// find the good pixmap resource
	while (xpmResources[i].id != 0)
	{
		if (xpmResources[i].id == resourceID) 
		{
			if (xpmResources[i].xpm != NULL) 
			{
				found = true;
				ppDataXpm = xpmResources[i].xpm;
				
				xpmGetValues (ppDataXpm, &width, &height, &ncolors, &cpp);
				break;
			}
		}
		i++;
	}

	if (!found)
		ppDataXpm = 0;

#elif BEOS
	bbitmap = 0;
	transparencySet = false;
	if (resourceFile == 0)
	{
		// this is a hack to find the plug-in on the disk to access resources.
		const char* locate_me = "";
		int32	cookie = 0;
		image_info	iinfo;
		uint32	here = uint32 (locate_me);
		while (get_next_image_info (0, &cookie, &iinfo) == B_OK)
		{
			uint32	begin = uint32 (iinfo.text);
			if (begin <= here && here <= begin + iinfo.text_size)
				break;
		}
		BFile resource (iinfo.name, B_READ_ONLY);
		resourceFile = new BResources (&resource);
		resourceFile->PreloadResourceType ();
	}
	size_t	outSize;
	const char* res = (const char*) resourceFile->LoadResource ('RAWT', resourceID, &outSize);
	if (res)
	{
		BMemoryIO	memoryIO (res, outSize);
		bbitmap = BTranslationUtils::GetBitmap (&memoryIO);
		if (bbitmap)
		{
			BRect rect = bbitmap->Bounds ();
			width = (long) rect.Width () + 1;
			height = (long) rect.Height () + 1;
		}
	}
	if (!bbitmap)
		fprintf (stderr, "********* Resource %d could NOT be loaded!\n", (int)resourceID);
#endif

	setTransparentColor (kTransparentCColor);
	
	#if DEBUG
	gBitmapAllocation += height * width;
	#endif
}

//-----------------------------------------------------------------------------
CBitmap::CBitmap (CFrame &frame, long width, long height)
	: width (width), height (height)
{
	#if DEBUG
	gNbCBitmap++;
	#endif

#if WINDOWS
	HDC hScreen = GetDC (0);
	pHandle = CreateCompatibleBitmap (hScreen, width, height);
	ReleaseDC (0, hScreen);	
	pMask = 0;

#elif MAC
	pHandle = 0;
	pMask = 0;
	
	Rect r;
	r.left = r.top = 0;
	r.right = width;
	r.bottom = height;

    #if QUARTZ
	NewGWorld ((GWorldPtr*)&pHandle, 32, &r, 0, 0, 0);

    #else
	NewGWorld ((GWorldPtr*)&pHandle, 0, &r, 0, 0, 0);

	#endif

#elif MOTIF
	pXdisplay = frame.getDisplay ();
	Drawable pWindow = frame.getWindow ();

	pMask = 0;
	pHandle = (void*)XCreatePixmap (pXdisplay, (Drawable)pWindow, width, height, frame.getDepth ());

#elif BEOS
	bbitmap = 0;
	transparencySet = false;
#endif
	
	setTransparentColor (kTransparentCColor);
}

//-----------------------------------------------------------------------------
CBitmap::CBitmap ()
	: width (0), height (0), resourceID (0)
{
	#if WINDOWS
	pHandle = 0;
	pMask = 0;
	
	#elif MAC
	pHandle = 0;
	pMask = 0;
	
	#elif MOTIF
	pMask = 0;
	pHandle = 0;
	
	#elif BEOS
	bbitmap = 0;

	#endif
}

//-----------------------------------------------------------------------------
CBitmap::~CBitmap ()
{
	#if DEBUG
	gNbCBitmap--;
	gBitmapAllocation -= height * width;
	#endif

	#if WINDOWS
	if (pHandle)
		DeleteObject (pHandle);
	if (pMask)
		DeleteObject (pMask);
	
	#elif MAC
	if (pHandle)
		DisposeGWorld ((GWorldPtr)pHandle);
	if (pMask)
		DisposeGWorld ((GWorldPtr)pMask);

	#elif MOTIF
	if (pHandle)
		XFreePixmap (pXdisplay, (Pixmap)pHandle);
	if (pMask) 
		XFreePixmap (pXdisplay, (Pixmap)pMask);

	#elif BEOS
	if (bbitmap)
		delete bbitmap;
	#endif
}

//-----------------------------------------------------------------------------
void *CBitmap::getHandle ()
 {
	#if WINDOWS||MOTIF
	return pHandle; 

	#elif MAC
	return pHandle;

	#elif BEOS
	return bbitmap;
	#endif
}

//-----------------------------------------------------------------------------
bool CBitmap::isLoaded ()
{
	#if MOTIF
	if (ppDataXpm)
		return true;
	
	#else
	if (getHandle ())
		return true;
	#endif

	return false;
}

#if QUARTZ
#if OLD_TRANSPARENT_BITMAP_MODE
class CDataProvider 
{
public:
	CDataProvider (CBitmap* bitmap) : bmp (bitmap) 
	{ 
		pos = 0; 
		PixMapHandle pixMap = GetGWorldPixMap ((GWorldPtr)bmp->getHandle ());
		ptr = (unsigned char*)GetPixBaseAddr (pixMap);
		color = bmp->getTransparentColor ();
	}

	static size_t getBytes (void *info, void *buffer, size_t count)
	{	// this could be optimized ;-)
		CDataProvider* p = (CDataProvider*)info;
		unsigned char* dst = (unsigned char*)buffer;
		unsigned char* src = p->ptr + p->pos;
		for (unsigned long i = 0; i < count / 4; i++)
		{
			if (src[1] == p->color.red && src[2] == p->color.green && src[3] == p->color.blue)
			{
				*dst++ = 0;
				src++;
			}
			else
				*dst++ = *src++;
			*dst++ = *src++;
			*dst++ = *src++;
			*dst++ = *src++;
		}
		p->pos += count;
		return count;
	}

	static void skipBytes (void *info, size_t count)
	{
		CDataProvider* p = (CDataProvider*)info;
		p->pos += count;
	}

	static void rewind (void *info)
	{
		CDataProvider* p = (CDataProvider*)info;
		p->pos = 0;
	}

	static void releaseProvider (void *info)
	{
		CDataProvider* p = (CDataProvider*)info;
		delete p;
	}

	unsigned long pos;
	CBitmap* bmp;
	unsigned char* ptr;
	CColor color;
};
#endif // OLD_TRANSPARENT_BITMAP_MODE

//-----------------------------------------------------------------------------
CGImageRef CBitmap::createCGImage (bool transparent)
{
	PixMapHandle pixMap = GetGWorldPixMap ((GWorldPtr)pHandle);
	
	Rect bounds;
	GetPixBounds (pixMap, &bounds);

	size_t pixRowBytes = GetPixRowBytes (pixMap);
	short pixDepth = GetPixDepth (pixMap);
	size_t size = pixRowBytes * (bounds.bottom - bounds.top);

	CGImageRef image = 0;
	CGDataProviderRef provider = 0;
	#if OLD_TRANSPARENT_BITMAP_MODE
	static CGDataProviderCallbacks callbacks = { CDataProvider::getBytes, CDataProvider::skipBytes, CDataProvider::rewind, CDataProvider::releaseProvider };
	if (transparent)
		provider = CGDataProviderCreate (new CDataProvider (this), &callbacks);
	else
	#endif
		provider = CGDataProviderCreateWithData (NULL, GetPixBaseAddr (pixMap), size, NULL);
	CGColorSpaceRef colorspace = CGColorSpaceCreateDeviceRGB ();
	CGImageAlphaInfo alphaInfo = kCGImageAlphaFirst;
	if (GetPixDepth (pixMap) != 32)
		alphaInfo = kCGImageAlphaNone;
	image = CGImageCreate (bounds.right - bounds.left, bounds.bottom - bounds.top, 8 , pixDepth, pixRowBytes, colorspace, alphaInfo, provider, NULL, true, kCGRenderingIntentDefault);
	CGDataProviderRelease (provider);
	CGColorSpaceRelease (colorspace);

	return image;
}
#endif

//-----------------------------------------------------------------------------
void CBitmap::draw (CDrawContext *pContext, CRect &rect, const CPoint &offset)
{
#if WINDOWS
#if USE_ALPHA_BLEND
	if (pHandle)
	{
		HGDIOBJ hOldObj;
		HDC hdcMemory = CreateCompatibleDC ((HDC)pContext->pSystemContext);
		hOldObj = SelectObject (hdcMemory, pHandle);

		BLENDFUNCTION blendFunction;
		blendFunction.BlendOp = AC_SRC_OVER;
		blendFunction.BlendFlags = 0;
		blendFunction.SourceConstantAlpha = 0xff;
		blendFunction.AlphaFormat = AC_SRC_ALPHA;

		#if DYNAMICALPHABLEND
		(*pfnAlphaBlend) ((HDC)pContext->pSystemContext, 
					rect.left + pContext->offset.h, rect.top + pContext->offset.v,
					rect.width (), rect.height (), 
					(HDC)hdcMemory,
					offset.h, offset.v,
					rect.width (), rect.height (),
					blendFunction);
		#else
		AlphaBlend ((HDC)pContext->pSystemContext, 
					rect.left + pContext->offset.h, rect.top + pContext->offset.v,
					rect.width (), rect.height (), 
					(HDC)hdcMemory,
					offset.h, offset.v,
					rect.width (), rect.height (),
					blendFunction);
		#endif
		SelectObject (hdcMemory, hOldObj);
		DeleteDC (hdcMemory);
	}
#else
	if (pHandle)
	{
		HGDIOBJ hOldObj;
		HDC hdcMemory = CreateCompatibleDC ((HDC)pContext->pSystemContext);
		hOldObj = SelectObject (hdcMemory, pHandle);
		BitBlt ((HDC)pContext->pSystemContext, 
						rect.left + pContext->offset.h, rect.top + pContext->offset.v, rect.width (), rect.height (), 
						(HDC)hdcMemory, offset.h, offset.v, SRCCOPY);
		SelectObject (hdcMemory, hOldObj);
		DeleteDC (hdcMemory);
	}
#endif
#elif MAC

	#if QUARTZ
	if (pHandle)
	{
		CGContextRef context = pContext->beginCGContext ();
		if (context)
		{
			CGImageRef image = createCGImage ();

			if (image)
			{
				CGRect dest;
				dest.origin.x = rect.left - offset.h + pContext->offset.h;
				dest.origin.y = (rect.top + pContext->offset.v) * -1 - (getHeight () - offset.v);
				dest.size.width = getWidth ();
				dest.size.height = getHeight ();
				
				CGContextScaleCTM (context, 1, 1);

				CRect ccr;
				pContext->getClipRect (ccr);
				CGRect cgClipRect = CGRectMake (ccr.left + pContext->offset.h, (ccr.top +  + pContext->offset.v) * -1 - ccr.height (), ccr.width (), ccr.height ());
				CGContextClipToRect (context, cgClipRect);

				CGRect clipRect;
				clipRect.origin.x = rect.left + pContext->offset.h;
			    clipRect.origin.y = (rect.top + pContext->offset.v) * -1  - rect.height ();
			    clipRect.size.width = rect.width (); 
			    clipRect.size.height = rect.height ();
				
				CGContextClipToRect (context, clipRect);

				CGContextDrawImage (context, dest, image);
				CGImageRelease (image);
			}
		}
		pContext->releaseCGContext (context);
	}
	
	#else
	Rect source, dest;
	dest.top    = rect.top  + pContext->offset.v;
	dest.left   = rect.left + pContext->offset.h;
	dest.bottom = dest.top  + rect.height ();
	dest.right  = dest.left + rect.width ();
		
	source.top    = offset.v;
	source.left   = offset.h;
	source.bottom = source.top  + rect.height ();
	source.right  = source.left + rect.width ();

	pContext->getPort ();
	BitMapPtr bitmapPtr = pContext->getBitmap ();
	
	if (pHandle && bitmapPtr)
	{
		PixMapHandle pmHandle = GetGWorldPixMap ((GWorldPtr)pHandle);
		if (pmHandle && LockPixels (pmHandle))
		{
			RGBColor oldForeColor, oldBackColor;
			GetForeColor (&oldForeColor);
			GetBackColor (&oldBackColor);
			::BackColor (whiteColor);
			::ForeColor (blackColor);
			
			CopyBits ((BitMapPtr)*pmHandle, bitmapPtr, &source, &dest, srcCopy, 0L);
			#if MACX
			QDAddRectToDirtyRegion (pContext->getPort (), &dest);
			#endif
			
			RGBForeColor (&oldForeColor);
			RGBBackColor (&oldBackColor);
			
			UnlockPixels (pmHandle);
		}
	}
	
	pContext->releaseBitmap ();
        #endif
        
#elif MOTIF
	if (!pHandle)
	{
		// the first time try to decode the pixmap
		pHandle = createPixmapFromXpm (pContext);
		if (!pHandle)
			return;
		
		// keep a trace of the display for deletion
		pXdisplay = pContext->pDisplay;
	}
	
#if DEVELOPMENT
	if (!(offset.h >= 0 && offset.v >= 0 &&
				rect.width () <= (getWidth () - offset.h) &&
				rect.height () <= (getHeight () - offset.v)))
	{
		fprintf (stderr, "%s(%d) -> Assert failed: try to display outside from the bitmap\n", __FILE__, __LINE__);
		return;
	}
#endif

	XCopyArea (pContext->pDisplay, (Drawable)pHandle, 
						 (Drawable)pContext->pWindow, 
						 (GC)pContext->pSystemContext, offset.h, offset.v,
						 rect.width (), rect.height (), rect.left, rect.top);

#elif BEOS
	BRect	brect (rect.left, rect.top, rect.right - 1, rect.bottom - 1);
	BRect	drect = brect;
	brect.OffsetTo (offset.h, offset.v);
	drect.OffsetBy (pContext->offset.h, pContext->offset.v);
	pContext->pView->SetDrawingMode (B_OP_COPY);
	pContext->pView->DrawBitmap (bbitmap, brect, drect);
#endif
}

//-----------------------------------------------------------------------------
void CBitmap::drawTransparent (CDrawContext *pContext, CRect &rect, const CPoint &offset)
{
#if WINDOWS
#if USE_ALPHA_BLEND
	if (pHandle)
	{
		HGDIOBJ hOldObj;
		HDC hdcMemory = CreateCompatibleDC ((HDC)pContext->pSystemContext);
		hOldObj = SelectObject (hdcMemory, pHandle);

		BLENDFUNCTION blendFunction;
		blendFunction.BlendOp = AC_SRC_OVER;
		blendFunction.BlendFlags = 0;
		blendFunction.SourceConstantAlpha = 0xff;
		blendFunction.AlphaFormat = AC_SRC_ALPHA;

		#if DYNAMICALPHABLEND
		(*pfnAlphaBlend) ((HDC)pContext->pSystemContext, 
					rect.left + pContext->offset.h, rect.top + pContext->offset.v,
					rect.width (), rect.height (), 
					(HDC)hdcMemory,
					offset.h, offset.v,
					rect.width (), rect.height (),
					blendFunction);
		#else
		AlphaBlend ((HDC)pContext->pSystemContext, 
					rect.left + pContext->offset.h, rect.top + pContext->offset.v,
					rect.width (), rect.height (), 
					(HDC)hdcMemory,
					offset.h, offset.v,
					rect.width (), rect.height (),
					blendFunction);
		#endif
		SelectObject (hdcMemory, hOldObj);
		DeleteDC (hdcMemory);
	}
#else
	BITMAP bm;
	HDC hdcBitmap;
	POINT ptSize;

	hdcBitmap = CreateCompatibleDC ((HDC)pContext->pSystemContext);
	SelectObject (hdcBitmap, pHandle);	 // Select the bitmap

	GetObject (pHandle, sizeof (BITMAP), (LPSTR)&bm);
	ptSize.x = bm.bmWidth;            // Get width of bitmap
	ptSize.y = bm.bmHeight;           // Get height of bitmap
	DPtoLP (hdcBitmap, &ptSize, 1);   // Convert from device to logical points

	DrawTransparent (pContext, rect, offset, hdcBitmap, ptSize, (HBITMAP)pMask, RGB(transparentCColor.red, transparentCColor.green, transparentCColor.blue));

	DeleteDC (hdcBitmap);
#endif	
#elif MAC

	#if QUARTZ
	if (pHandle)
	{
		CGContextRef context = pContext->beginCGContext ();
		if (context)
		{
			CGImageRef image = createCGImage (true);

			if (image)
			{
				CGRect dest;
				dest.origin.x = rect.left + offset.h + pContext->offset.h;
				dest.origin.y = (rect.top + pContext->offset.v) * -1 - (getHeight () - offset.v);
				dest.size.width = getWidth ();
				dest.size.height = getHeight ();
				
				CGContextScaleCTM (context, 1, 1);

				CRect ccr;
				pContext->getClipRect (ccr);
				CGRect cgClipRect = CGRectMake (ccr.left + pContext->offset.h, (ccr.top +  + pContext->offset.v) * -1 - ccr.height (), ccr.width (), ccr.height ());
				CGContextClipToRect (context, cgClipRect);

				CGRect clipRect;
				clipRect.origin.x = rect.left + pContext->offset.h;
			    clipRect.origin.y = (rect.top + pContext->offset.v) * -1  - rect.height ();
			    clipRect.size.width = rect.width (); 
			    clipRect.size.height = rect.height ();
				
				CGContextClipToRect (context, clipRect);

				CGContextDrawImage (context, dest, image);
				CGImageRelease (image);
			}
		}
		pContext->releaseCGContext (context);
	}

	#else
	Rect source, dest;
	dest.top    = rect.top  + pContext->offset.v;
	dest.left   = rect.left + pContext->offset.h;
	dest.bottom = dest.top  + rect.height ();
	dest.right  = dest.left + rect.width ();
	
	source.top    = offset.v;
	source.left   = offset.h;
	source.bottom = source.top  + rect.height ();
	source.right  = source.left + rect.width ();

	pContext->getPort ();
	BitMapPtr bitmapPtr = pContext->getBitmap ();

	if (pHandle && bitmapPtr)
	{
		PixMapHandle pmHandle = GetGWorldPixMap ((GWorldPtr)pHandle);
		if (pmHandle && LockPixels (pmHandle))
		{
			RGBColor oldForeColor, oldBackColor;
			GetForeColor (&oldForeColor);
			GetBackColor (&oldBackColor);
			
			RGBColor col;
			CColor2RGBColor (transparentCColor, col);
			RGBBackColor (&col);
			::ForeColor (blackColor);
			
			if (pMask)
			{
				PixMapHandle pmHandleMask = GetGWorldPixMap ((GWorldPtr)pMask);
				if (pmHandleMask && LockPixels (pmHandleMask))
				{
					CopyMask ((BitMapPtr)*pmHandle, (BitMapPtr)*pmHandleMask, bitmapPtr, 
								&source, &source, &dest);
										
					UnlockPixels (pmHandleMask);
				}
			}
			else
				CopyBits ((BitMapPtr)*pmHandle, bitmapPtr, &source, &dest, transparent, 0L);
			
			RGBForeColor (&oldForeColor);
			RGBBackColor (&oldBackColor);

			#if MACX
			QDAddRectToDirtyRegion (pContext->getPort (), &dest);
			#endif
			
			UnlockPixels (pmHandle);
		}
	}

	pContext->releaseBitmap ();
	#endif
        
#elif MOTIF
	if (!pHandle) 
	{
		// the first time try to decode the pixmap
		pHandle = createPixmapFromXpm (pContext);
		if (!pHandle)
			return;
		
		// keep a trace of the display for deletion
		pXdisplay = pContext->pDisplay;
	}
	
	if (pMask == 0)
	{
		// get image from the pixmap
		XImage* image = XGetImage (pContext->pDisplay, (Drawable)pHandle, 
                0, 0, width, height, AllPlanes, ZPixmap);
		assert (image);
		
		// create the bitmap mask
		pMask = (void*)XCreatePixmap (pContext->pDisplay, (Drawable)pContext->pWindow, 
                width, height, 1);
		assert (pMask);
		
		// create a associated GC
		XGCValues values;
		values.foreground = 1;
		GC gc = XCreateGC (pContext->pDisplay, (Drawable)pMask, GCForeground, &values); 
		
		// clear the mask
		XFillRectangle (pContext->pDisplay, (Drawable)pMask, gc, 0, 0, width, height);
   
		// get the transparent color index
		int color = pContext->getIndexColor (transparentCColor);
		
		// inverse the color
		values.foreground = 0;
		XChangeGC (pContext->pDisplay, gc, GCForeground, &values);
     
		// compute the mask
		XPoint *points = new XPoint [height * width];
		int x, y, nbPoints = 0;
		switch (image->depth) 
		{
		case 8:
			for (y = 0; y < height; y++) 
			{
				char* src = image->data + (y * image->bytes_per_line);
				
				for (x = 0; x < width; x++) 
				{
					if (src[x] == color) 
					{
						points[nbPoints].x = x;
						points[nbPoints].y = y;
						nbPoints++;
					}
				}
			}
			break;

		case 24: {
			int bytesPerPixel = image->bits_per_pixel >> 3;
			char *lp = image->data;
			for (y = 0; y < height; y++)
			{
				char* cp = lp;
				for (x = 0; x < width; x++)
				{
					if (*(int*)cp == color)
					{
						points[nbPoints].x = x;
						points[nbPoints].y = y;
						nbPoints++;
					}
					cp += bytesPerPixel;
				}
				lp += image->bytes_per_line;
			}
		} break;

		default :
			break;
		}

		XDrawPoints (pContext->pDisplay, (Drawable)pMask, gc,
								 points, nbPoints, CoordModeOrigin);
     
		// free 
		XFreeGC (pContext->pDisplay, gc);
		delete []points;

		// delete 
		XDestroyImage (image);
	}
	
	// set the new clipmask
	XGCValues value;
	value.clip_mask = (Pixmap)pMask;
	value.clip_x_origin = rect.left - offset.h;
	value.clip_y_origin = rect.top - offset.v;
	XChangeGC (pContext->pDisplay, (GC)pContext->pSystemContext,
						 GCClipMask|GCClipXOrigin|GCClipYOrigin, &value);

	XCopyArea (pContext->pDisplay, (Drawable)pHandle, (Drawable)pContext->pWindow, 
						 (GC)pContext->pSystemContext, offset.h, offset.v,
						 rect.width (), rect.height (), rect.left, rect.top);
	
	// unset the clipmask
	XSetClipMask (pContext->pDisplay, (GC)pContext->pSystemContext, None);
	

#elif BEOS
	if (!transparencySet)
	{
		uint32 c32 = transparentCColor.red | (transparentCColor.green  << 8) | (transparentCColor.blue << 16);
		uint32 *pix = (uint32*) bbitmap->Bits ();
		uint32 ctr = B_TRANSPARENT_32_BIT.red | (B_TRANSPARENT_32_BIT.green << 8) | (B_TRANSPARENT_32_BIT.blue << 16) | (B_TRANSPARENT_32_BIT.alpha << 24);
		
		for (int32 z = 0, count = bbitmap->BitsLength () / 4; z < count; z++)
		{
			if ((pix[z] & 0xffffff) == c32) 
				pix[z] = ctr;
		}
		transparencySet = true;
	}
	BRect	brect (rect.left, rect.top, rect.right - 1, rect.bottom - 1);
	BRect	drect = brect;
	brect.OffsetTo (offset.h, offset.v);
	drect.OffsetBy (pContext->offset.h, pContext->offset.v);
	pContext->pView->SetDrawingMode (B_OP_OVER);
	pContext->pView->DrawBitmap (bbitmap, brect, drect);

#endif
}

//-----------------------------------------------------------------------------
void CBitmap::drawAlphaBlend (CDrawContext *pContext, CRect &rect, const CPoint &offset, unsigned char alpha)
{
#if WINDOWS
	if (pHandle)
	{
		HGDIOBJ hOldObj;
		HDC hdcMemory = CreateCompatibleDC ((HDC)pContext->pSystemContext);
		hOldObj = SelectObject (hdcMemory, pHandle);

		BLENDFUNCTION blendFunction;
		blendFunction.BlendOp = AC_SRC_OVER;
		blendFunction.BlendFlags = 0;
		blendFunction.SourceConstantAlpha = alpha;
		blendFunction.AlphaFormat = 0;//AC_SRC_NO_ALPHA;

		#if DYNAMICALPHABLEND
		(*pfnAlphaBlend) ((HDC)pContext->pSystemContext, 
					rect.left + pContext->offset.h, rect.top + pContext->offset.v,
					rect.width (), rect.height (), 
					(HDC)hdcMemory,
					offset.h, offset.v,
					rect.width (), rect.height (),
					blendFunction);
		#else
		AlphaBlend ((HDC)pContext->pSystemContext, 
					rect.left + pContext->offset.h, rect.top + pContext->offset.v,
					rect.width (), rect.height (), 
					(HDC)hdcMemory,
					offset.h, offset.v,
					rect.width (), rect.height (),
					blendFunction);
		#endif
		SelectObject (hdcMemory, hOldObj);
		DeleteDC (hdcMemory);
	}

#elif MAC

    #if QUARTZ
	if (pHandle)
	{
		CGContextRef context = pContext->beginCGContext ();
		if (context)
		{
			CGContextSetAlpha (context, (float)alpha / 255.f);
			
			CGImageRef image = createCGImage ();

			if (image)
			{
				CGRect dest;
				dest.origin.x = rect.left + offset.h + pContext->offset.h;
				dest.origin.y = (rect.top + pContext->offset.v) * -1 - (getHeight () - offset.v);
				dest.size.width = getWidth ();
				dest.size.height = getHeight ();
				
				CGContextScaleCTM (context, 1, 1);

				CRect ccr;
				pContext->getClipRect (ccr);
				CGRect cgClipRect = CGRectMake (ccr.left + pContext->offset.h, (ccr.top +  + pContext->offset.v) * -1 - ccr.height (), ccr.width (), ccr.height ());
				CGContextClipToRect (context, cgClipRect);

				CGRect clipRect;
				clipRect.origin.x = rect.left + pContext->offset.h;
			    clipRect.origin.y = (rect.top + pContext->offset.v) * -1  - rect.height ();
			    clipRect.size.width = rect.width (); 
			    clipRect.size.height = rect.height ();
				
				CGContextClipToRect (context, clipRect);

				CGContextDrawImage (context, dest, image);
				CGImageRelease (image);
			}
		}
		pContext->releaseCGContext (context);
	}
	
    #else
	Rect source, dest;
	dest.top    = rect.top  + pContext->offset.v;
	dest.left   = rect.left + pContext->offset.h;
	dest.bottom = dest.top  + rect.height ();
	dest.right  = dest.left + rect.width ();
		
	source.top    = offset.v;
	source.left   = offset.h;
	source.bottom = source.top  + rect.height ();
	source.right  = source.left + rect.width ();
		
	pContext->getPort ();
	BitMapPtr bitmapPtr = pContext->getBitmap ();
	if (bitmapPtr)
	{
		RGBColor col;
		CColor color = {alpha, alpha, alpha, 0};
		CColor2RGBColor (color, col);
		OpColor (&col);
		
		if (pHandle)
		{
			PixMapHandle pmHandle = GetGWorldPixMap ((GWorldPtr)pHandle);
			if (pmHandle && LockPixels (pmHandle))
			{
				RGBColor oldForeColor, oldBackColor;
				GetForeColor (&oldForeColor);
				GetBackColor (&oldBackColor);
				::BackColor (whiteColor);
				::ForeColor (blackColor);
			
				CopyBits ((BitMapPtr)*pmHandle, bitmapPtr, &source, &dest, blend, 0L);
				#if MACX
				QDAddRectToDirtyRegion (pContext->getPort (), &dest);
				#endif
			
				RGBForeColor (&oldForeColor);
				RGBBackColor (&oldBackColor);
			
				UnlockPixels (pmHandle);
			}
		}
	}

	pContext->releaseBitmap ();
        #endif
#endif
}
//-----------------------------------------------------------------------------
void CBitmap::setTransparentColor (const CColor color)
{
	transparentCColor = color;
}

//-----------------------------------------------------------------------------
void CBitmap::setTransparencyMask (CDrawContext* pContext, const CPoint& offset)
{
#if WINDOWS
	if (pMask)
		DeleteObject (pMask);

	CRect r (0, 0, width, height);
	r.offset (offset.h, offset.v);
	pMask = CreateMaskBitmap (pContext, r, transparentCColor);

#elif MAC
	#if QUARTZ
	#else
	if (pMask)
		DisposeGWorld ((GWorldPtr)pMask);
	pMask = 0;
	
	Rect r;
	r.left = r.top = 0;
	r.right = width;
	r.bottom = height;
	OSErr err = NewGWorld ((GWorldPtr*)&pMask, 1, &r, 0, 0, 0); // create monochrome GWorld
	if (!err)
	{
		GWorldPtr oldPort;
		GDHandle oldDevice;
		GetGWorld (&oldPort, &oldDevice);
		SetGWorld ((GWorldPtr)pMask, 0);

		PixMapHandle pmHandle = GetGWorldPixMap ((GWorldPtr)pMask);
		BitMapPtr sourcePtr = pContext->getBitmap ();
		
		if (sourcePtr && pmHandle && LockPixels (pmHandle))
		{
			RGBColor oldForeColor, oldBackColor;
			GetForeColor (&oldForeColor);
			GetBackColor (&oldBackColor);
			
			RGBColor col;
			CColor2RGBColor (transparentCColor, col);
			RGBBackColor (&col);

			::ForeColor (blackColor);

			Rect src = r;
			src.left   += offset.h;
			src.right  += offset.h;
			src.top    += offset.v;
			src.bottom += offset.v;

			CopyBits (sourcePtr, (BitMapPtr)*pmHandle, &src, &r, srcCopy, 0L);

			RGBForeColor (&oldForeColor);
			RGBBackColor (&oldBackColor);
			
			UnlockPixels (pmHandle);
		}
		
		pContext->releaseBitmap ();
				
		SetGWorld (oldPort, oldDevice);
	}
	#endif
        
#else
	// todo: implement me!
#endif
}

//-----------------------------------------------------------------------------
//----------------------------------------------------------------------------
#if MOTIF
//-----------------------------------------------------------------------------
void* CBitmap::createPixmapFromXpm (CDrawContext *pContext)
{
	if (!ppDataXpm)
		return NULL;
  
	Pixmap pixmap = 0;
	XpmAttributes attributes;
	
	attributes.valuemask = XpmCloseness|XpmColormap|XpmVisual|XpmDepth; 
	attributes.closeness = 100000;
	attributes.visual = pContext->getVisual ();
	attributes.depth  = pContext->getDepth ();

	// use the pContext colormap instead of the DefaultColormapOfScreen
	attributes.colormap = pContext->getColormap ();

	int status;
	if (attributes.depth == 8 || attributes.depth == 24)
	{
#if USE_XPM
		status = XpmCreatePixmapFromData (pContext->pDisplay,
							(Drawable)pContext->pWindow, ppDataXpm, &pixmap, NULL, &attributes);
		if (status != XpmSuccess)
		{
			fprintf (stderr, "createPixmapFromXpm-> XpmError: %s\n", XpmGetErrorString(status));
			return NULL;
		}
#else
		status = createPixmapFromData (pContext->pDisplay,
         (Drawable)pContext->pWindow, ppDataXpm, &pixmap, &attributes);
		if (!status)
		{
			fprintf (stderr, "createPixmapFromXpm-> Error\n");
			return NULL;
		}
#endif
	}
	else
	{
		fprintf (stderr, "createPixmapFromXpm-> Depth %d not supported\n", attributes.depth);
		return NULL;
	}

#if DEVELOPMENT
	fprintf (stderr, "createPixmapFromXpm-> There are %d requested colors\n", attributes.ncolors);
#endif

	return (void*)pixmap;
}
#endif

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
#if BEOS
//----------------------------------------------------------------------------

BResources* CBitmap::resourceFile = 0;

//----------------------------------------------------------------------------

void CBitmap::closeResource ()
{
	if (resourceFile)
	{
		delete resourceFile;
		resourceFile = 0;
	}
}

//----------------------------------------------------------------------------
#endif
END_NAMESPACE_VSTGUI

//-----------------------------------------------------------------------------
// CFileSelector Implementation
//-----------------------------------------------------------------------------
#define stringAnyType  "Any Type (*.*)"
#define stringAllTypes "All Types: ("
#define stringSelect   "Select"
#define stringCancel   "Cancel"
#define stringLookIn   "Look in"
#define kPathMax        1024

#if WINDOWS
static UINT APIENTRY SelectDirectoryHook (HWND hdlg, UINT message, WPARAM wParam, LPARAM lParam);
static LRESULT CALLBACK SelectDirectoryButtonProc (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
WNDPROC fpOldSelectDirectoryButtonProc;
static UINT APIENTRY WinSaveHook (HWND hdlg, UINT msg, WPARAM wParam, LPARAM lParam);
static bool bFolderSelected;
static bool bDidCancel;
static char selDirPath[kPathMax];
#endif

BEGIN_NAMESPACE_VSTGUI

#if PLUGGUI
//-----------------------------------------------------------------------------
CFileSelector::CFileSelector (void*)
: vstFileSelect (0)
{}
#else
//-----------------------------------------------------------------------------
CFileSelector::CFileSelector (AudioEffectX* effect)
: effect (effect), vstFileSelect (0)
{}
#endif

//-----------------------------------------------------------------------------
CFileSelector::~CFileSelector ()
{
	if (vstFileSelect)
	{
		#if !PLUGGUI
		if (effect && effect->canHostDo ("closeFileSelector"))
			effect->closeFileSelector (vstFileSelect);
		else
		#endif
		{
			if (vstFileSelect->reserved == 1 && vstFileSelect->returnPath)
			{
				delete []vstFileSelect->returnPath;
				vstFileSelect->returnPath = 0;
				vstFileSelect->sizeReturnPath = 0;
			}
			if (vstFileSelect->returnMultiplePaths)
			{
				for (long i = 0; i < vstFileSelect->nbReturnPath; i++)
				{
					delete []vstFileSelect->returnMultiplePaths[i];
					vstFileSelect->returnMultiplePaths[i] = 0;
				}
				delete[] vstFileSelect->returnMultiplePaths;
				vstFileSelect->returnMultiplePaths = 0;
			}
		}
	}
}

//-----------------------------------------------------------------------------
long CFileSelector::run (VstFileSelect *vstFileSelect)
{
	this->vstFileSelect = vstFileSelect;
	vstFileSelect->nbReturnPath = 0;
	if (vstFileSelect->returnPath)
		vstFileSelect->returnPath[0] = 0;

	#if !PLUGGUI
	if (effect
	#if MACX 
		&& vstFileSelect->command != kVstFileSave 
	#endif
		&& effect->canHostDo ("openFileSelector") && effect->canHostDo ("closeFileSelector"))
	{
		if (effect->openFileSelector (vstFileSelect))
			return vstFileSelect->nbReturnPath;
	}
	else
	#endif
	{
#if WINDOWS
		char filter[512];
		char filePathBuffer[kPathMax];
		strcpy (filePathBuffer, "");
		char* filePath = filePathBuffer;
		char fileName[kPathMax];
		strcpy (fileName, "");
		filter[0] = 0;
		filePath[0] = 0;
		fileName[0] = 0;

		//-----------------------------------------
		if (vstFileSelect->command == kVstFileLoad ||
			vstFileSelect->command == kVstMultipleFilesLoad ||
			vstFileSelect->command == kVstDirectorySelect)
		{
			char* multiBuffer = 0;
			if (vstFileSelect->command == kVstMultipleFilesLoad)
			{
				multiBuffer = new char [kPathMax * 100];
				strcpy (multiBuffer, "");
				filePath = multiBuffer;
			}

			if (vstFileSelect->command != kVstDirectorySelect) 
			{
				char allBuffer [kPathMax] = {0};
				char* p = filter;
				char* p2 = allBuffer;

				const char* ext;
				const char* extensions [100];
				long i, j, extCount = 0;
				char string[24];

				for (long ty = 0; ty < vstFileSelect->nbFileTypes; ty++)
				{
					for (i = 0; i < 2 ; i++)
					{				
						if (i == 0)
						{
							ext = vstFileSelect->fileTypes[ty].dosType;
						
							strcpy (p, vstFileSelect->fileTypes[ty].name);
							strcat (p, " (.");
							strcat (p, ext);
							strcat (p, ")");
							p += strlen (p) + 1;

							strcpy (string, "*.");
							strcat (string, ext);
							strcpy (p, string);
							p += strlen (p);	
						}
						else
						{
							if (!strcmp (vstFileSelect->fileTypes[ty].dosType, vstFileSelect->fileTypes[ty].unixType) || !strcmp (vstFileSelect->fileTypes[ty].unixType, ""))
								break; // for
							ext = vstFileSelect->fileTypes[ty].unixType;
							strcpy (string, ";*.");
							strcat (string, ext);
							strcpy (p, string);
							p += strlen (p);	
						}
						bool found = false;
						for (j = 0; j < extCount;j ++)
						{
							if (strcmp (ext, extensions [j]) == 0)
							{
								found = true;
								break;
							}
						}
						if (!found && extCount < 100)
							extensions [extCount++] = ext;
					}
					p ++;
				} // end for filetype
			
				if (extCount > 1)
				{
					for (i = 0; i < extCount ;i ++)
					{					
						ext = extensions [i];
						strcpy (string, "*.");
						strcat (string, ext);

						if (p2 != allBuffer)
						{
							strcpy (p2, ";");
							p2++;
						}
						strcpy (p2, string);
						p2 += strlen (p2);
					}

					// add the : All types
					strcpy (p, stringAllTypes);			
					strcat (p, allBuffer);
					strcat (p, ")");
					p += strlen (p) + 1;
					strcpy (p, allBuffer);
					p += strlen (p) + 1;			
				}

				strcpy (p, stringAnyType);
				p += strlen (p) + 1;
				strcpy (p, "*.*");
				p += strlen (p) + 1;

				*p++ = 0;
				*p++ = 0;
			}

			OPENFILENAME ofn = {0};
			ofn.lStructSize  = sizeof (OPENFILENAME);
			HWND owner = 0;
			#if !PLUGGUI
			if (effect->getEditor () && ((AEffGUIEditor*)effect->getEditor ())->getFrame ())
				owner = (HWND)((AEffGUIEditor*)effect->getEditor ())->getFrame ()->getSystemWindow ();
			#endif
			ofn.hwndOwner    = owner;
	
			if (vstFileSelect->command == kVstDirectorySelect) 
				ofn.lpstrFilter = "HideFileFilter\0*.___\0\0"; // to hide files
			else
				ofn.lpstrFilter  = filter[0] ? filter : 0;
			ofn.nFilterIndex = 1;
			ofn.lpstrCustomFilter = NULL;
			ofn.lpstrFile    = filePath;
			if (vstFileSelect->command == kVstMultipleFilesLoad)
				ofn.nMaxFile    = 100 * kPathMax - 1;
			else
				ofn.nMaxFile    = sizeof (filePathBuffer) - 1;

			ofn.lpstrFileTitle  = fileName;
			ofn.nMaxFileTitle   = 64;
			ofn.lpstrInitialDir = vstFileSelect->initialPath;
			ofn.lpstrTitle      = vstFileSelect->title;
			ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_EXPLORER | OFN_ENABLESIZING | OFN_ENABLEHOOK;
			if (vstFileSelect->command == kVstDirectorySelect)
			{
				ofn.Flags &= ~OFN_FILEMUSTEXIST;
				ofn.lpfnHook = SelectDirectoryHook;
			}

			if (vstFileSelect->command == kVstMultipleFilesLoad)
				ofn.Flags |= OFN_ALLOWMULTISELECT;
		
			vstFileSelect->nbReturnPath = 0;
			bDidCancel = true;

			if (GetOpenFileName (&ofn) || 
				((vstFileSelect->command == kVstDirectorySelect) && !bDidCancel && strlen (selDirPath) != 0))  
			{
				switch (vstFileSelect->command)
				{
				case kVstFileLoad:
					vstFileSelect->nbReturnPath = 1;
					if (!vstFileSelect->returnPath)
					{
						vstFileSelect->reserved = 1;
						vstFileSelect->returnPath = new char[strlen (ofn.lpstrFile) + 1];
						vstFileSelect->sizeReturnPath = (long)strlen (ofn.lpstrFile) + 1;			
					}
					strcpy (vstFileSelect->returnPath, ofn.lpstrFile);
					break;
				
				case kVstMultipleFilesLoad:
					{
					char string[kPathMax], directory[kPathMax];
					char *previous = ofn.lpstrFile;
					size_t len;
					bool dirFound = false;
					bool first = true;
					directory[0] = 0; // !!
					vstFileSelect->returnMultiplePaths = new char*[kPathMax];
					long i = 0;
					while (1)
					{
						if (*previous != 0)
						{   // something found
							if (!dirFound) 
							{
								dirFound = true;
								strcpy (directory, previous);
								len = strlen (previous) + 1;  // including 0
								previous += len;

								if (*previous == 0)
								{  // 1 selected file only		
									vstFileSelect->returnMultiplePaths[i] = new char [strlen (directory) + 1];
									strcpy (vstFileSelect->returnMultiplePaths[i++], directory);
								}
								else
								{
									if (directory[strlen (directory) - 1] != '\\')
										strcat (directory, "\\");
								}
							}
							else 
							{
								sprintf (string, "%s%s", directory, previous);
								len = strlen (previous) + 1;  // including 0
								previous += len;

								vstFileSelect->returnMultiplePaths[i] = new char [strlen (string) + 1];
								strcpy (vstFileSelect->returnMultiplePaths[i++], string);
							}
						}
						else
							break;
					}
					vstFileSelect->nbReturnPath = i;					
					} break;

				case kVstDirectorySelect: 
					vstFileSelect->nbReturnPath = 1;
					if (!vstFileSelect->returnPath)
					{
						vstFileSelect->reserved = 1;
						vstFileSelect->returnPath = new char[strlen (selDirPath) + 1];
						vstFileSelect->sizeReturnPath = (long)strlen (selDirPath) + 1;			
					}
					strcpy (vstFileSelect->returnPath, selDirPath);
				}
				if (multiBuffer)
					delete []multiBuffer;
				return vstFileSelect->nbReturnPath;
			}
			if (multiBuffer)
				delete []multiBuffer;
		}

		//-----------------------------------------
		else if (vstFileSelect->command == kVstFileSave)
		{
			char* p = filter;
			for (long ty = 0; ty < vstFileSelect->nbFileTypes; ty++)
			{
				const char* ext = vstFileSelect->fileTypes[ty].dosType;
				if (ext)
				{
					strcpy (p, vstFileSelect->fileTypes[ty].name);
					strcat (p, " (.");
					strcat (p, ext);
					strcat (p, ")");
					p += strlen (p) + 1;
	
					char string[24];
					strcpy (string, "*.");
					strcat (string, ext);
					strcpy (p, string);
					p += strlen (p) + 1;
				}
			}
			*p++ = 0;
			*p++ = 0;
		
			OPENFILENAME ofn = {0};
			ofn.lStructSize  = sizeof (OPENFILENAME);
			HWND owner = 0;
			#if !PLUGGUI
			if (effect->getEditor () && ((AEffGUIEditor*)effect->getEditor ())->getFrame ())
				owner = (HWND)((AEffGUIEditor*)effect->getEditor ())->getFrame ()->getSystemWindow ();
			#endif
			ofn.hwndOwner    = owner;
			ofn.hInstance    = GetInstance ();
			ofn.lpstrFilter = filter[0] ? filter : 0;
			ofn.nFilterIndex = 1;
			ofn.lpstrFile = filePath;
			ofn.lpstrCustomFilter = NULL;
			ofn.nMaxFile = sizeof (filePathBuffer) - 1;
			ofn.lpstrFileTitle = fileName;
			ofn.nMaxFileTitle = 64;
			ofn.lpstrInitialDir = vstFileSelect->initialPath;
			ofn.lpstrTitle = vstFileSelect->title;
			ofn.Flags = OFN_EXPLORER | OFN_ENABLESIZING | OFN_HIDEREADONLY | OFN_ENABLEHOOK;
			
			if (vstFileSelect->nbFileTypes >= 1)
				ofn.lpstrDefExt = vstFileSelect->fileTypes[0].dosType;
			
			// add a template view
			ofn.lCustData = (DWORD)0;
			ofn.lpfnHook = WinSaveHook;
			
			if (GetSaveFileName (&ofn))	
			{
				vstFileSelect->nbReturnPath = 1;
				if (!vstFileSelect->returnPath)
				{
					vstFileSelect->reserved = 1;
					vstFileSelect->returnPath = new char[strlen (ofn.lpstrFile) + 1];
					vstFileSelect->sizeReturnPath = (long)strlen (ofn.lpstrFile) + 1;			
				}
				strcpy (vstFileSelect->returnPath, ofn.lpstrFile);
			
				return vstFileSelect->nbReturnPath;
			}
			#if _DEBUG
			else
			{
				DWORD err = CommDlgExtendedError (); // for breakpoint
			}
			#endif
		}

#elif MAC
#if CARBON
		#if MACX
		// new approach for supporting long filenames on mac os x is to use unix path mode
		// if vstFileSelect->future[0] is 1 on entry and 0 on exit the resulting paths are UTF8 encoded paths
		bool unixPathMode = (vstFileSelect->future[0] == 1);
		#endif
		NavEventUPP	eventUPP = NewNavEventUPP (CFileSelector::navEventProc);
		if (vstFileSelect->command == kVstFileSave)
		{
			NavDialogCreationOptions dialogOptions;
			NavGetDefaultDialogCreationOptions (&dialogOptions);
			dialogOptions.windowTitle = CFStringCreateWithCString (NULL, vstFileSelect->title[0] ? vstFileSelect->title : "Select a Destination", kCFStringEncodingUTF8);
			CFStringRef defSaveName = 0;
			#if MACX
			if (unixPathMode && vstFileSelect->initialPath)
			{
				char* name = strrchr (vstFileSelect->initialPath, '/');
				if (name && name[1] != 0)
				{
					defSaveName = dialogOptions.saveFileName = CFStringCreateWithCString (NULL, name+1, kCFStringEncodingUTF8);
					name[0] = 0;
					dialogOptions.optionFlags |= kNavPreserveSaveFileExtension;
				}
				else if (name == 0)
				{
					defSaveName = dialogOptions.saveFileName = CFStringCreateWithCString (NULL, vstFileSelect->initialPath, kCFStringEncodingUTF8);
					dialogOptions.optionFlags |= kNavPreserveSaveFileExtension;
					vstFileSelect->initialPath = 0;
				}
			}
			else
			#endif
			if (vstFileSelect->initialPath && ((FSSpec*)vstFileSelect->initialPath)->name)
			{
				FSSpec* defaultSpec = (FSSpec*)vstFileSelect->initialPath;
				defSaveName = CFStringCreateWithPascalString (NULL, defaultSpec->name, kCFStringEncodingASCII);
				if (defSaveName)
				{
					dialogOptions.saveFileName = defSaveName;
					dialogOptions.optionFlags |= kNavPreserveSaveFileExtension;
				}
				*defaultSpec->name = 0;
			}
			NavDialogRef dialogRef;
			if (NavCreatePutFileDialog (&dialogOptions, NULL, kNavGenericSignature, eventUPP, this, &dialogRef) == noErr) 
			{
			    AEDesc defaultLocation;   
			    AEDesc* defLocPtr = 0;   
				if (vstFileSelect->initialPath)
				{
					#if MACX
					if (unixPathMode)
					{
						FSRef fsRef;
						if (FSPathMakeRef ((const unsigned char*)vstFileSelect->initialPath, &fsRef, NULL) == noErr)
						{
				            if (AECreateDesc (typeFSRef, &fsRef, sizeof(FSRef), &defaultLocation) == noErr)
				                defLocPtr = &defaultLocation;
						}
					}
					else
					#endif
					{
						FSSpec* defaultSpec = (FSSpec*)vstFileSelect->initialPath;
				        if (defaultSpec->parID && defaultSpec->vRefNum)
				        {
				            if (AECreateDesc (typeFSS, defaultSpec, sizeof(FSSpec), &defaultLocation) == noErr)
				                defLocPtr = &defaultLocation;
						}
					}
				}
		        if (defLocPtr)
		            NavCustomControl (dialogRef, kNavCtlSetLocation, (void*)defLocPtr);
				NavDialogRun (dialogRef);

				if (defLocPtr)
		            AEDisposeDesc (defLocPtr);

				NavReplyRecord navReply;
				if (NavDialogGetReply (dialogRef, &navReply) == noErr)
				{
					FSRef parentFSRef;
					AEKeyword theAEKeyword;
					DescType typeCode;
					Size actualSize;
			        // get the FSRef referring to the parent directory
				    if (AEGetNthPtr(&navReply.selection, 1, typeFSRef,
		        		&theAEKeyword, &typeCode, &parentFSRef, sizeof(FSRef), &actualSize) == noErr)
					{
						#if MACX
						if (unixPathMode)
						{
							bool success = true;
							vstFileSelect->nbReturnPath = 1;
							if (vstFileSelect->returnPath == 0)
							{
								vstFileSelect->reserved = 1;
								vstFileSelect->returnPath = new char [PATH_MAX];
							}
							if (FSRefMakePath (&parentFSRef, (unsigned char*)vstFileSelect->returnPath, PATH_MAX) == noErr)
							{
								char saveFileName [PATH_MAX];
								if (CFStringGetCString (navReply.saveFileName, saveFileName, PATH_MAX, kCFStringEncodingUTF8))
								{
									strcat (vstFileSelect->returnPath, "/");
									strcat (vstFileSelect->returnPath, saveFileName);
									vstFileSelect->future[0] = 0;
								}
								else
									success = false;
							}
							else
								success = false;
							if (!success && vstFileSelect->reserved)
							{
								vstFileSelect->nbReturnPath = 0;
								delete [] vstFileSelect->returnPath;
							}
						}
						else
						#endif
						{
							FSSpec spec;
							FSCatalogInfoBitmap infoBitmap = kFSCatInfoNone;
							FSGetCatalogInfo (&parentFSRef, infoBitmap, NULL, NULL, &spec, NULL);
							CInfoPBRec pbRec = {0};	
							pbRec.dirInfo.ioDrDirID = spec.parID;
							pbRec.dirInfo.ioVRefNum = spec.vRefNum;
							pbRec.dirInfo.ioNamePtr = spec.name;
							if (PBGetCatInfoSync (&pbRec) == noErr)
							{
								spec.parID = pbRec.dirInfo.ioDrDirID;
								// the cfstring -> pascalstring can fail if the filename length > 63 (FSSpec sucks)
								if (CFStringGetPascalString (navReply.saveFileName, (unsigned char*)&spec.name, sizeof (spec.name), kCFStringEncodingASCII))
								{
									vstFileSelect->nbReturnPath = 1;
									if (!vstFileSelect->returnPath)
									{
										vstFileSelect->reserved = 1;
										vstFileSelect->returnPath = new char [sizeof (FSSpec)];
									}
									memcpy (vstFileSelect->returnPath, &spec, sizeof (FSSpec));
								}
							}
						}
					}
					NavDisposeReply (&navReply);
				}
				if (defSaveName)
					CFRelease (defSaveName);
				NavDialogDispose (dialogRef);
				DisposeNavEventUPP (eventUPP);
				return vstFileSelect->nbReturnPath;
			}
			if (defSaveName)
				CFRelease (defSaveName);
		}
		else if (vstFileSelect->command == kVstDirectorySelect)
		{
			NavDialogCreationOptions dialogOptions;
			NavGetDefaultDialogCreationOptions (&dialogOptions);
			dialogOptions.windowTitle = CFStringCreateWithCString (NULL, vstFileSelect->title[0] ? vstFileSelect->title : "Select Directory", kCFStringEncodingUTF8);
			NavDialogRef dialogRef;
			if (NavCreateChooseFolderDialog (&dialogOptions, eventUPP, NULL, this, &dialogRef) == noErr)
			{
			    AEDesc defaultLocation;   
			    AEDesc* defLocPtr = 0;   
				if (vstFileSelect->initialPath)
				{
					#if MACX
					if (unixPathMode)
					{
						FSRef fsRef;
						if (FSPathMakeRef ((const unsigned char*)vstFileSelect->initialPath, &fsRef, NULL) == noErr)
						{
				            if (AECreateDesc (typeFSRef, &fsRef, sizeof(FSRef), &defaultLocation) == noErr)
				                defLocPtr = &defaultLocation;
						}
					}
					else
					#endif
					{
						FSSpec* defaultSpec = (FSSpec*)vstFileSelect->initialPath;
				        if (defaultSpec->parID && defaultSpec->vRefNum)       
				            if (AECreateDesc (typeFSS, defaultSpec, sizeof(FSSpec), &defaultLocation) == noErr)
				                defLocPtr = &defaultLocation;
			        }
				}
		        if (defLocPtr)
		            NavCustomControl (dialogRef, kNavCtlSetLocation, (void*)defLocPtr);
				NavDialogRun (dialogRef);
				if (defLocPtr)
		            AEDisposeDesc (defLocPtr);
				NavReplyRecord navReply;
				if (NavDialogGetReply (dialogRef, &navReply) == noErr)
				{
					FSRef parentFSRef;
					AEKeyword theAEKeyword;
					DescType typeCode;
					Size actualSize;
				    if (AEGetNthPtr(&navReply.selection, 1, typeFSRef,
		        		&theAEKeyword, &typeCode, &parentFSRef, sizeof(FSRef), &actualSize) == noErr)
					{
						#if MACX
						if (unixPathMode)
						{
							vstFileSelect->nbReturnPath = 1;
							if (vstFileSelect->returnPath == 0)
							{
								vstFileSelect->reserved = 1;
								vstFileSelect->returnPath = new char [PATH_MAX];
							}
							if (FSRefMakePath (&parentFSRef, (unsigned char*)vstFileSelect->returnPath, PATH_MAX) != noErr)
							{
								vstFileSelect->nbReturnPath = 0;
								if (vstFileSelect->reserved)
									delete [] vstFileSelect->returnPath;
							}
							else
								vstFileSelect->future[0] = 0;
						}
						else
						#endif
						{
							FSSpec spec;
							FSCatalogInfoBitmap infoBitmap = kFSCatInfoNone;
							FSGetCatalogInfo (&parentFSRef, infoBitmap, NULL, NULL, &spec, NULL);
							vstFileSelect->nbReturnPath = 1;
							if (!vstFileSelect->returnPath)
							{
								vstFileSelect->reserved = 1;
								vstFileSelect->returnPath = new char [sizeof (FSSpec)];
							}
							memcpy (vstFileSelect->returnPath, &spec, sizeof (FSSpec));
						}
					}
					
					NavDisposeReply (&navReply);
				}
				NavDialogDispose (dialogRef);
				DisposeNavEventUPP (eventUPP);
				return vstFileSelect->nbReturnPath;
			}
		}
		else // FileLoad
		{
			NavDialogCreationOptions dialogOptions;
			NavGetDefaultDialogCreationOptions (&dialogOptions);
			if (vstFileSelect->command == kVstFileLoad)
			{
				dialogOptions.windowTitle = CFStringCreateWithCString (NULL, vstFileSelect->title[0] ? vstFileSelect->title : "Select a File to Open", kCFStringEncodingUTF8);
				dialogOptions.optionFlags &= ~kNavAllowMultipleFiles;
			}
			else
			{
				dialogOptions.windowTitle = CFStringCreateWithCString (NULL, vstFileSelect->title[0] ? vstFileSelect->title : "Select Files to Open", kCFStringEncodingUTF8);
				dialogOptions.optionFlags |= kNavAllowMultipleFiles;
			}
			NavObjectFilterUPP objectFilterUPP = NewNavObjectFilterUPP (CFileSelector::navObjectFilterProc);
			NavDialogRef dialogRef;
			if (NavCreateGetFileDialog (&dialogOptions, NULL, eventUPP, NULL, objectFilterUPP, this, &dialogRef) == noErr)
			{
			    AEDesc defaultLocation;   
			    AEDesc* defLocPtr = 0;   
				if (vstFileSelect->initialPath)
				{
					#if MACX
					if (unixPathMode)
					{
						FSRef fsRef;
						if (FSPathMakeRef ((const unsigned char*)vstFileSelect->initialPath, &fsRef, NULL) == noErr)
						{
				            if (AECreateDesc (typeFSRef, &fsRef, sizeof(FSRef), &defaultLocation) == noErr)
				                defLocPtr = &defaultLocation;
						}
					}
					else
					#endif
					{
						FSSpec* defaultSpec = (FSSpec*)vstFileSelect->initialPath;
				        if (defaultSpec->parID && defaultSpec->vRefNum)       
				            if (AECreateDesc (typeFSS, defaultSpec, sizeof(FSSpec), &defaultLocation) == noErr)
				                defLocPtr = &defaultLocation;
			        }
				}
		        if (defLocPtr)
		            NavCustomControl (dialogRef, kNavCtlSetLocation, (void*)defLocPtr);

				NavDialogRun (dialogRef);

				if (defLocPtr)
		            AEDisposeDesc (defLocPtr);

				NavReplyRecord navReply;
				if (NavDialogGetReply (dialogRef, &navReply) == noErr)
				{
					FSRef parentFSRef;
					AEKeyword theAEKeyword;
					DescType typeCode;
					Size actualSize;
					if (vstFileSelect->command == kVstFileLoad)
					{
					    if (AEGetNthPtr(&navReply.selection, 1, typeFSRef,
			        		&theAEKeyword, &typeCode, &parentFSRef, sizeof(FSRef), &actualSize) == noErr)
						{
							#if MACX
							if (unixPathMode)
							{
								vstFileSelect->nbReturnPath = 1;
								if (vstFileSelect->returnPath == 0)
								{
									vstFileSelect->reserved = 1;
									vstFileSelect->returnPath = new char [PATH_MAX];
								}
								if (FSRefMakePath (&parentFSRef, (unsigned char*)vstFileSelect->returnPath, PATH_MAX) != noErr)
								{
									vstFileSelect->nbReturnPath = 0;
									if (vstFileSelect->reserved)
										delete [] vstFileSelect->returnPath;
								}
								else
									vstFileSelect->future[0] = 0;
							}
							else
							#endif
							{
								FSSpec spec;
								FSCatalogInfoBitmap infoBitmap = kFSCatInfoNone;
								FSGetCatalogInfo (&parentFSRef, infoBitmap, NULL, NULL, &spec, NULL);
								vstFileSelect->nbReturnPath = 1;
								if (!vstFileSelect->returnPath)
								{
									vstFileSelect->reserved = 1;
									vstFileSelect->returnPath = new char [sizeof (FSSpec)];
								}
								memcpy (vstFileSelect->returnPath, &spec, sizeof (FSSpec));
							}
						}
					}
					else
					{
						AECountItems (&navReply.selection, &vstFileSelect->nbReturnPath);
						vstFileSelect->returnMultiplePaths = new char* [vstFileSelect->nbReturnPath];
						int index = 1;
					    while (AEGetNthPtr(&navReply.selection, index++, typeFSRef,
			        		&theAEKeyword, &typeCode, &parentFSRef, sizeof(FSRef), &actualSize) == noErr)
						{
							#if MACX
							if (unixPathMode)
							{
								vstFileSelect->returnMultiplePaths[index-2] = new char[PATH_MAX];
								FSRefMakePath (&parentFSRef, (unsigned char*)vstFileSelect->returnMultiplePaths[index-2], PATH_MAX);
								vstFileSelect->future[0] = 0;
							}
							else
							#endif
							{
								FSSpec spec;
								FSCatalogInfoBitmap infoBitmap = kFSCatInfoNone;
								FSGetCatalogInfo (&parentFSRef, infoBitmap, NULL, NULL, &spec, NULL);
								vstFileSelect->returnMultiplePaths[index-2] = new char[sizeof (FSSpec)];
								memcpy (vstFileSelect->returnMultiplePaths[index-2], &spec, sizeof (FSSpec));
							}
						}
					}
				}
				DisposeNavObjectFilterUPP (objectFilterUPP);
				DisposeNavEventUPP (eventUPP);
				NavDialogDispose (dialogRef);
				return vstFileSelect->nbReturnPath;
			}
			DisposeNavObjectFilterUPP (objectFilterUPP);
		}
		DisposeNavEventUPP (eventUPP);
#else
		StandardFileReply reply;
		if (vstFileSelect->command == kVstFileSave)
		{
			unsigned char defName[64];
			defName[0] = 0;
			StandardPutFile ("\pSelect a Destination", defName, &reply);
			if (reply.sfGood && reply.sfFile.name[0] != 0)
			{
				if (!vstFileSelect->returnPath)
				{
					vstFileSelect->reserved = 1;
					vstFileSelect->returnPath = new char [301];
				}
				memcpy (vstFileSelect->returnPath, &reply.sfFile, 300);
				vstFileSelect->nbReturnPath = 1;
				return 1;
			}
		}

		else if (vstFileSelect->command == kVstDirectorySelect) 
		{
		#if USENAVSERVICES
			if (NavServicesAvailable ())
			{
				NavReplyRecord navReply;
				NavDialogOptions dialogOptions;
				short ret = false;
				AEDesc defLoc;
				defLoc.descriptorType = typeFSS;
				defLoc.dataHandle = NewHandle (sizeof (FSSpec));
				FSSpec	finalFSSpec;
				finalFSSpec.parID   = 0;	// *dirID;
				finalFSSpec.vRefNum = 0;	// *volume;
				finalFSSpec.name[0] = 0;

				NavGetDefaultDialogOptions (&dialogOptions);
				dialogOptions.dialogOptionFlags &= ~kNavAllowMultipleFiles;
				dialogOptions.dialogOptionFlags |= kNavSelectDefaultLocation;
				strcpy ((char* )dialogOptions.message, "Select Directory");
				c2pstr ((char* )dialogOptions.message);
				NavChooseFolder (&defLoc, &navReply, &dialogOptions, 0 /* eventUPP */, 0, 0);
				DisposeHandle (defLoc.dataHandle);
				
				AEDesc 	resultDesc;	
				AEKeyword keyword;
				resultDesc.dataHandle = 0L;

				if (navReply.validRecord && AEGetNthDesc (&navReply.selection, 1, typeFSS, &keyword, &resultDesc) == noErr)
				{
					ret = true;
					vstFileSelect->nbReturnPath = 1;
					if (!vstFileSelect->returnPath)
					{
						vstFileSelect->reserved = 1;
						vstFileSelect->returnPath = new char [sizeof (FSSpec)];
					}
					memcpy (vstFileSelect->returnPath, *resultDesc.dataHandle, sizeof (FSSpec));
				}
				NavDisposeReply (&navReply);
				return vstFileSelect->nbReturnPath;
			}
			else
		#endif
			{
				// Can't select a Folder; the Application does not support it, and Navigational Services are not available...
				return 0;
			}
		}

		else
		{
			SFTypeList typelist;
			long numFileTypes = vstFileSelect->nbFileTypes;
			//seem not to work... if (numFileTypes <= 0)
			{
				numFileTypes = -1;	// all files
				typelist[0] = 'AIFF';
			}
			/*else
			{
				if (numFileTypes > 4)
					numFileTypes = 4;
				for (long i = 0; i < numFileTypes; i++)
					memcpy (&typelist[i], vstFileSelect->fileTypes[i].macType, 4);
			}*/
			StandardGetFile (0L, numFileTypes, typelist, &reply);
			if (reply.sfGood)
			{
				if (!vstFileSelect->returnPath)
				{
					vstFileSelect->reserved = 1;
					vstFileSelect->returnPath = new char [301];
				}
				memcpy (vstFileSelect->returnPath, &reply.sfFile, 300);
				vstFileSelect->nbReturnPath = 1;
				return 1;
			}
		}
#endif // CARBON
#else
		//CAlert::alert ("The current Host application doesn't support FileSelector !", "Warning");
#endif
	}
	return 0;
}

#if MAC && CARBON
//-----------------------------------------------------------------------------
pascal void CFileSelector::navEventProc (const NavEventCallbackMessage callBackSelector, NavCBRecPtr callBackParms, NavCallBackUserData callBackUD) 
{
	CFileSelector* fs = (CFileSelector*)callBackUD;
	switch (callBackSelector)
	{
		case kNavCBEvent:
		{
			#if !PLUGGUI
			AudioEffectX* effect = fs->effect;
			if (effect && callBackParms->eventData.eventDataParms.event->what == nullEvent)
				effect->masterIdle ();
			#endif
			break;
		}
	}
}

//-----------------------------------------------------------------------------
pascal Boolean CFileSelector::navObjectFilterProc (AEDesc *theItem, void *info, void *callBackUD, NavFilterModes filterMode)
{
    Boolean result = false;
	CFileSelector* fs = (CFileSelector*)callBackUD;
    NavFileOrFolderInfo *theInfo = (NavFileOrFolderInfo*)info;

	if (theInfo->isFolder || fs->vstFileSelect->nbFileTypes == 0)
		result = true;
	else
	{
	    FSRef ref;
		AECoerceDesc (theItem, typeFSRef, theItem);
		if (AEGetDescData (theItem, &ref, sizeof (FSRef)) == noErr)
		{
			LSItemInfoRecord infoRecord;
			if (LSCopyItemInfoForRef (&ref, kLSRequestExtension | kLSRequestTypeCreator, &infoRecord) == noErr)
			{
				char extension [128];
				extension[0] = 0;
				if (infoRecord.extension)
					CFStringGetCString (infoRecord.extension, extension, 128, kCFStringEncodingUTF8);
				for (long i = 0; i < fs->vstFileSelect->nbFileTypes; i++)
				{
					VstFileType* ft = &fs->vstFileSelect->fileTypes[i];
					if ((OSType)ft->macType == infoRecord.filetype)
					{
						result = true;
						break;
					}
					else if (infoRecord.extension)
					{
						if (!strcmp (extension, ft->unixType) || !strcmp (extension, ft->dosType))
						{
							result = true;
							break;
						}
					}
				}
				if (infoRecord.extension)
					CFRelease (infoRecord.extension);
			}
		}
	}
	return result;
}
#endif

END_NAMESPACE_VSTGUI

#if WINDOWS
#include <Commdlg.h>
//-----------------------------------------------------------------------------
UINT APIENTRY SelectDirectoryHook (HWND hdlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_NOTIFY: 
	{
		OFNOTIFY *lpon = (OFNOTIFY *)lParam;
	
		switch (lpon->hdr.code)
		{
		case CDN_FILEOK:
			CommDlg_OpenSave_GetFolderPath (GetParent (hdlg), selDirPath, kPathMax);
			bDidCancel = false;
			break;
		
		case CDN_INITDONE: {
			#define HIDE_ITEMS 4
			int  i;
			UINT hide_items[HIDE_ITEMS] = {edt1, stc3, cmb1, stc2};	

			for (i = 0; i < HIDE_ITEMS; i++)
				CommDlg_OpenSave_HideControl (GetParent (hdlg), hide_items[i]);
			
			CommDlg_OpenSave_SetControlText (GetParent (hdlg), stc4, (char*)(const char*)stringLookIn);
			CommDlg_OpenSave_SetControlText (GetParent (hdlg), IDOK, (char*)(const char*)stringSelect);
			CommDlg_OpenSave_SetControlText (GetParent (hdlg), IDCANCEL, (char*)(const char*)stringCancel);
		} break;
		}
	} break;

	case WM_INITDIALOG:
		fpOldSelectDirectoryButtonProc = (WNDPROC)SetWindowLongPtr (
					GetDlgItem (GetParent (hdlg), IDOK), 
					GWLP_WNDPROC, (LONG_PTR)SelectDirectoryButtonProc);
		break;
		
	case WM_DESTROY:
		SetWindowLong (GetDlgItem (GetParent (hdlg), IDOK), 
				GWLP_WNDPROC, (LONG_PTR)fpOldSelectDirectoryButtonProc);
	}
	return false;
}

//-----------------------------------------------------------------------------
LRESULT CALLBACK SelectDirectoryButtonProc (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_SETTEXT: 
		if (! (strcmp ((char *)lParam, stringSelect) == 0))
			return false;
		break;
	
	case WM_LBUTTONUP:
	case WM_RBUTTONUP: {
		char mode[256];
		GetWindowText (hwnd, mode, 256);
		if (!strcmp (mode, stringSelect))
		{
			bFolderSelected = true;
			char oldDirPath[kPathMax];
			CommDlg_OpenSave_GetFolderPath (GetParent (hwnd), oldDirPath, kPathMax);
			// you need a lot of tricks to get name of currently selected folder:
			// the following call of the original windows procedure causes the
			// selected folder to open and after that you can retrieve its name
			// by calling ..._GetFolderPath (...)
			CallWindowProc ((WNDPROC)fpOldSelectDirectoryButtonProc, hwnd, message, wParam, lParam);
			CommDlg_OpenSave_GetFolderPath (GetParent (hwnd), selDirPath, kPathMax);

			if (1) // consumers like it like this
			{
				if (strcmp (oldDirPath, selDirPath) == 0 || selDirPath [0] == 0)
				{
					// the same folder as the old one, means nothing selected: close
					bFolderSelected = true;
					bDidCancel = false;
					PostMessage (GetParent (hwnd), WM_CLOSE, 0, 0);
					return false;
				}
				else
				{
					// another folder is selected: browse into it
					bFolderSelected = false;
					return true;
				}
			}
			else // original code
			{
				if (strcmp (oldDirPath, selDirPath) == 0 || selDirPath [0] == 0)
				{
					// the same folder as the old one, means nothing selected: stay open
					bFolderSelected = false;
					return true;
				}
			}
		}

		bDidCancel = false;
		PostMessage (GetParent (hwnd), WM_CLOSE, 0, 0); 
		return false;
	} break;
	} // end switch

	return CallWindowProc ((WNDPROC)fpOldSelectDirectoryButtonProc, hwnd, message, wParam, lParam);
}

//-----------------------------------------------------------------------------
static void showPathInWindowTitle (HWND hParent, LPOFNOTIFY lpon)
{
	#define WINDOWTEXTSIZE 260 + 64
	OPENFILENAME *ofn = lpon->lpOFN;
	char text[WINDOWTEXTSIZE];
	char *p;
	size_t len;

	// Put the path into the Window Title
	if (lpon->lpOFN->lpstrTitle)
		strcpy (text, lpon->lpOFN->lpstrTitle);
	else
	{
		char *pp;

		GetWindowText (hParent, text, WINDOWTEXTSIZE);
		pp = strchr (text, '-');
		if (pp)
			*--pp = 0;
	}

	p = strcat (text, " - [");
	p = text;
	len = strlen (text); 
	p += len;
	len = WINDOWTEXTSIZE - len - 2;
	CommDlg_OpenSave_GetFolderPath (hParent, p, len);
	strcat (text, "]");
	SetWindowText (hParent, text);
}

//------------------------------------------------------------------------
UINT APIENTRY WinSaveHook (HWND hdlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_NOTIFY: {
		LPOFNOTIFY lpon = (LPOFNOTIFY)lParam; 
		if (!lpon)
			break;

		switch (lpon->hdr.code)
		{
		case CDN_FOLDERCHANGE: 
			showPathInWindowTitle (GetParent (hdlg), lpon);
			break;
		}
	} break;
	} // end switch

	return 0;
}
#endif

//-----------------------------------------------------------------------------

#if WINDOWS

#if USE_MOUSE_HOOK
HHOOK MouseHook = 0L;

LRESULT CALLBACK MouseProc (int nCode, WPARAM wParam, LPARAM lParam)
{
	if (nCode < 0)
		return CallNextHookEx (MouseHook, nCode, wParam, lParam);

	if (wParam == 522)
	{
		MOUSEHOOKSTRUCT* struct2 = (MOUSEHOOKSTRUCT*) lParam;
		if (struct2->hwnd == ???)
		{
			return -1;
		}
	}
	return CallNextHookEx (MouseHook, nCode, wParam, lParam);
}
#endif

//-----------------------------------------------------------------------------
bool InitWindowClass ()
{
	gUseCount++;
	if (gUseCount == 1)
	{
		sprintf (gClassName, "Plugin%x", GetInstance ());
		
		WNDCLASS windowClass;
		windowClass.style = CS_GLOBALCLASS;//|CS_OWNDC; // add Private-DC constant 

		windowClass.lpfnWndProc = WindowProc; 
		windowClass.cbClsExtra  = 0; 
		windowClass.cbWndExtra  = 0; 
		windowClass.hInstance   = GetInstance (); 
		windowClass.hIcon = 0; 

		windowClass.hCursor = LoadCursor (NULL, IDC_ARROW);
		windowClass.hbrBackground = GetSysColorBrush (COLOR_BTNFACE); 
		windowClass.lpszMenuName  = 0; 
		windowClass.lpszClassName = gClassName; 
		RegisterClass (&windowClass);

		#if USE_MOUSE_HOOK
		MouseHook = SetWindowsHookEx (WH_MOUSE, MouseProc, GetInstance (), 0);
		#endif

		bSwapped_mouse_buttons = GetSystemMetrics (SM_SWAPBUTTON) > 0;
	}
	return true;
}

//-----------------------------------------------------------------------------
void ExitWindowClass ()
{
	gUseCount--;
	if (gUseCount == 0)
	{
		UnregisterClass (gClassName, GetInstance ());

		#if USE_MOUSE_HOOK
		if (MouseHook)
		{
			UnhookWindowsHookEx (MouseHook);
			MouseHook = 0L;
		}
		#endif
	}
}

//-----------------------------------------------------------------------------
LONG_PTR WINAPI WindowProc (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	USING_NAMESPACE_VSTGUI
	CFrame* pFrame = (CFrame*)GetWindowLongPtr (hwnd, GWLP_USERDATA);

	switch (message)
	{
	case WM_MOUSEWHEEL:
	{
		if (pFrame)
		{
			VSTGUI_CDrawContext context (pFrame, 0, hwnd);
			VSTGUI_CPoint where (LOWORD (lParam), HIWORD (lParam));
			short zDelta = (short) HIWORD(wParam);
			pFrame->onWheel (&context, where, (float)zDelta / WHEEL_DELTA);
		}
		break;
	}
	case WM_CTLCOLOREDIT:
	{
		if (pFrame)
		{
			VSTGUI_CTextEdit *textEdit = (VSTGUI_CTextEdit*)pFrame->getEditView ();
			if (textEdit)
			{
				VSTGUI_CColor fontColor = textEdit->getFontColor ();
				SetTextColor ((HDC) wParam, RGB (fontColor.red, fontColor.green, fontColor.blue));

				VSTGUI_CColor backColor = textEdit->getBackColor ();
				SetBkColor ((HDC) wParam, RGB (backColor.red, backColor.green, backColor.blue));

				if (textEdit->platformFontColor)
					DeleteObject (textEdit->platformFontColor);
				textEdit->platformFontColor = CreateSolidBrush (RGB (backColor.red, backColor.green, backColor.blue));
				return (LRESULT)(textEdit->platformFontColor);
			}
		}
	}
	break;

	case WM_PAINT:
	{
		RECT r;
		if (pFrame && GetUpdateRect (hwnd, &r, false))
		{
			PAINTSTRUCT ps;
			HDC hdc = BeginPaint (hwnd, &ps);

			VSTGUI_CDrawContext context (pFrame, hdc, hwnd);
			
			#if 1
			CRect updateRect (ps.rcPaint.left, ps.rcPaint.top, ps.rcPaint.right, ps.rcPaint.bottom);
			pFrame->drawRect (&context, updateRect);
			#else
			pFrame->draw (&context);
			#endif

			EndPaint (hwnd, &ps);
			return 0;
		}
	}
	break;

	case WM_MEASUREITEM :
	{
		MEASUREITEMSTRUCT* ms = (MEASUREITEMSTRUCT*)lParam;
		if (pFrame && ms && ms->CtlType == ODT_MENU && ms->itemData)
		{
			VSTGUI_COptionMenu* optMenu = (VSTGUI_COptionMenu*)pFrame->getEditView ();
			if (optMenu && optMenu->getScheme ())
			{
				VSTGUI_CPoint size;

				VSTGUI_CDrawContext context (pFrame, 0, hwnd);
				optMenu->getScheme ()->getItemSize ((const char*)ms->itemData, &context, size);

				ms->itemWidth  = size.h;
				ms->itemHeight = size.v;
				return TRUE;
			}
		}
	}
	break;

	case WM_DRAWITEM :
	{
		DRAWITEMSTRUCT* ds = (DRAWITEMSTRUCT*)lParam;
		if (pFrame && ds && ds->CtlType == ODT_MENU && ds->itemData)
		{
			VSTGUI_COptionMenu* optMenu = (VSTGUI_COptionMenu*)pFrame->getEditView ();
			if (optMenu && optMenu->getScheme ())
			{
				long state = 0;
				if (ds->itemState & ODS_CHECKED)
					state |= VSTGUI_COptionMenuScheme::kChecked;
				if (ds->itemState & ODS_DISABLED) // ODS_GRAYED?
					state |= VSTGUI_COptionMenuScheme::kDisabled;
				if (ds->itemState & ODS_SELECTED)
					state |= VSTGUI_COptionMenuScheme::kSelected;
					
				CRect r (ds->rcItem.left, ds->rcItem.top, ds->rcItem.right, ds->rcItem.bottom);
				r.bottom++;
				
				VSTGUI_CDrawContext* pContext = new VSTGUI_CDrawContext (pFrame, ds->hDC, 0);
				optMenu->getScheme ()->drawItem ((const char*)ds->itemData, ds->itemID, state, pContext, r);
				delete pContext;
				return TRUE;
			}
		}
	}
	break;
	
	case WM_RBUTTONDOWN:
	case WM_MBUTTONDOWN:
	case WM_LBUTTONDOWN:
		if (pFrame)
		{
		#if 1
			VSTGUI_CDrawContext context (pFrame, 0, hwnd);
			VSTGUI_CPoint where (LOWORD (lParam), HIWORD (lParam));
			pFrame->mouse (&context, where);
		#else
			VSTGUI_CPoint where (LOWORD (lParam), HIWORD (lParam));
			pFrame->mouse ((VSTGUI_CDrawContext*)0, where);
		#endif

			return 0;
		}
		break;
		
	case WM_DESTROY:
		if (pFrame)
		{
			pFrame->setOpenFlag (false);
			pFrame->setParentSystemWindow (0);
		}
		break;
	}
	return DefWindowProc (hwnd, message, wParam, lParam);
}

//-----------------------------------------------------------------------------
HANDLE CreateMaskBitmap (CDrawContext* pContext, CRect& rect, CColor transparentColor)
{
	HBITMAP pMask = CreateBitmap (rect.width (), rect.height (), 1, 1, 0);

	HDC hSrcDC = (HDC)pContext->getSystemContext ();
	HDC hDstDC = CreateCompatibleDC (hSrcDC);
	SelectObject (hDstDC, pMask);

	COLORREF oldBkColor = SetBkColor (hSrcDC, RGB (transparentColor.red, transparentColor.green, transparentColor.blue));
	
	BitBlt (hDstDC, 0, 0, rect.width (), rect.height (), hSrcDC, rect.left, rect.top, SRCCOPY);
	
	SetBkColor (hSrcDC, oldBkColor);
	DeleteDC (hDstDC);
	
	return pMask;
}

//-----------------------------------------------------------------------------
void DrawTransparent (CDrawContext* pContext, CRect& rect, const CPoint& offset,
					  HDC hdcBitmap, POINT ptSize, HBITMAP pMask, COLORREF color)
{
	if (pMask == NULL)
	{
		if (pfnTransparentBlt)
		{
			HDC		hdcSystemContext = (HDC)pContext->getSystemContext ();
			long	x, y;
			long	width  = rect.width ();
			long	height = rect.height ();

			x = rect.x + pContext->offset.x;
			y = rect.y + pContext->offset.y;

			pfnTransparentBlt (hdcSystemContext, x, y, width, height, hdcBitmap, offset.x, offset.y, width, height, color);
		}
		else
		{
			// OPTIMIZATION: we only do four instead of EIGHT blits
			HDC		hdcSystemContext = (HDC)pContext->getSystemContext ();
			HDC		hdcMask = CreateCompatibleDC (hdcSystemContext);

			COLORREF	crOldBack = SetBkColor (hdcSystemContext, 0xFFFFFF);
			COLORREF	crOldText = SetTextColor (hdcSystemContext, 0x000000);
			HBITMAP		bmMaskOld, maskMap;

			long	x, y;
			long	width  = rect.width ();
			long	height = rect.height ();

			x = rect.x + pContext->offset.x;
			y = rect.y + pContext->offset.y;

			// Create mask-bitmap in memory
			maskMap = CreateBitmap (width, height, 1, 1, NULL);
			bmMaskOld = (HBITMAP)SelectObject (hdcMask, maskMap);

			// Copy bitmap into mask-bitmap and converting it into a black'n'white mask
			SetBkColor (hdcBitmap, color);
			BitBlt (hdcMask, 0, 0, width, height, hdcBitmap, offset.x, offset.y, SRCCOPY);

			// Copy image masked to screen
			BitBlt (hdcSystemContext, x, y, width, height, hdcBitmap, offset.x, offset.y, SRCINVERT);
			BitBlt (hdcSystemContext, x, y, width, height, hdcMask, 0, 0, SRCAND);
			BitBlt (hdcSystemContext, x, y, width, height, hdcBitmap, offset.x, offset.y, SRCINVERT);

			DeleteObject (SelectObject (hdcMask, bmMaskOld));
			DeleteDC (hdcMask);

			SetBkColor (hdcSystemContext, crOldBack);
			SetTextColor (hdcSystemContext, crOldText);
		}
	}
	else
	{
		// OPTIMIZATION: we only do five instead of EIGHT blits
		HDC		hdcSystemContext = (HDC)pContext->getSystemContext ();
		HDC		hdcMask = CreateCompatibleDC (hdcSystemContext);
		HDC		hdcMem = CreateCompatibleDC (hdcSystemContext);
		HBITMAP	bmAndMem;
		HBITMAP	bmMemOld, bmMaskOld;

		long	x, y;
		long	width = rect.width ();
		long	height = rect.height ();

		x = rect.x + pContext->offset.x;
		y = rect.y + pContext->offset.y;

		bmAndMem = CreateCompatibleBitmap(hdcSystemContext, width, height);

		bmMaskOld   = (HBITMAP)SelectObject (hdcMask, pMask);
		bmMemOld    = (HBITMAP)SelectObject (hdcMem, bmAndMem);

		BitBlt (hdcMem, 0, 0, width, height, hdcSystemContext, x, y, SRCCOPY);
		BitBlt (hdcMem, 0, 0, width, height, hdcBitmap, offset.x, offset.y, SRCINVERT);
		BitBlt (hdcMem, 0, 0, width, height, hdcMask, offset.x, offset.y, SRCAND);
		BitBlt (hdcMem, 0, 0, width, height, hdcBitmap, offset.x, offset.y, SRCINVERT);
		BitBlt (hdcSystemContext, x, y, width, height, hdcMem, 0, 0, SRCCOPY);

		DeleteObject (SelectObject (hdcMem, bmMemOld));
		SelectObject (hdcMask, bmMaskOld);

		DeleteDC (hdcMem);
		DeleteDC(hdcMask);
	}
}
#endif

//-----------------------------------------------------------------------------
#if MAC || MOTIF || BEOS
// return a degre value between [0, 360 * 64[
long convertPoint2Angle (CPoint &pm, CPoint &pt)
{
	long angle;
	if (pt.h == pm.h)
	{
		if (pt.v < pm.v)
			angle = 5760;	// 90 * 64
		else
			angle = 17280; // 270 * 64
	}
	else if (pt.v == pm.v)
	{
		if (pt.h < pm.h)
			angle = 11520;	// 180 * 64
		else
			angle = 0;	
	}
	else
	{
		// 3666.9299 = 180 * 64 / pi
		angle = (long)(3666.9298 * atan ((double)(pm.v - pt.v) / (double)(pt.h - pm.h)));
    
		if (pt.v < pm.v)
		{
			if (pt.h < pm.h)
				angle += 11520; // 180 * 64
		}
		else
		{
			if (pt.h < pm.h)
				angle += 11520; // 180 * 64
			else
				angle += 23040; // 360 * 64
		}
	}
	return angle;
}
#endif


//-----------------------------------------------------------------------------
#if MOTIF
XRectangle rect;
static bool first = true;

//-----------------------------------------------------------------------------
void _destroyCallback (Widget widget, XtPointer clientData, XtPointer callData)
{
	CFrame* pFrame = (CFrame*)clientData;
	if (pFrame)
	{
		pFrame->freeGc ();
		pFrame->setOpenFlag (false);
		pFrame->pSystemWindow = 0;
	}
}

//-----------------------------------------------------------------------------
void _drawingAreaCallback (Widget widget, XtPointer clientData, XtPointer callData)
{
	CFrame* pFrame = (CFrame*)clientData;
	XmDrawingAreaCallbackStruct *cbs = (XmDrawingAreaCallbackStruct *)callData;
	XEvent *event = cbs->event;

	//-------------------------------------
	if (cbs->reason == XmCR_INPUT)
	{
		if (event->xbutton.type == ButtonRelease)
			return;

		if (event->xbutton.type != ButtonPress &&
				event->xbutton.type != KeyPress)
			return;

		Window pWindow = pFrame->getWindow ();
		CDrawContext context (pFrame, pFrame->getGC (), (void*)pWindow);

		CPoint where (event->xbutton.x, event->xbutton.y);
		pFrame->mouse (&context, where);
	}
	//------------------------------------
	else if (cbs->reason == XmCR_EXPOSE)
	{
		XExposeEvent *expose = (XExposeEvent*)event;
#if TEST_REGION
		rect.x      = expose->x;
		rect.y      = expose->y;
		rect.width  = expose->width;
		rect.height = expose->height;
		if (first)
		{
			pFrame->region = XCreateRegion ();
			first = false;
		}

		XUnionRectWithRegion (&rect, pFrame->region, pFrame->region);
#endif
		if (expose->count == 0)
		{
#if TEST_REGION
			XSetRegion (expose->pDisplay, pFrame->getGC (), pFrame->region);

			// add processus of static first to set the region to max after a total draw and destroy it the first time...
#endif
			pFrame->draw ();

#if TEST_REGION
			rect.x      = 0;
			rect.y      = 0;
			rect.width  = pFrame->getWidth ();
			rect.height = pFrame->getHeight ();
			XUnionRectWithRegion (&rect, pFrame->region, pFrame->region);
			XSetRegion (expose->pDisplay, pFrame->getGC (), pFrame->region);
			XDestroyRegion (pFrame->region);
			first = true;
#endif
		}
	}
}

//-----------------------------------------------------------------------------
void _eventHandler (Widget w, XtPointer clientData, XEvent *event, char *p)
{
	switch (event->type)
	{
	case EnterNotify:
		break;

	case LeaveNotify:
		XCrossingEvent *xevent = (XCrossingEvent*)event;
		
		CFrame* pFrame = (CFrame*)clientData;
		if (pFrame && pFrame->getEditView ())
		{
			if (xevent->x < 0 || xevent->x >= pFrame->getWidth () ||
					xevent->y < 0 || xevent->y >= pFrame->getHeight ())
			{
				// if button pressed => don't defocus
				if (xevent->state & (Button1Mask|Button2Mask|Button3Mask))
					break;
				pFrame->getEditView ()->looseFocus ();
				pFrame->setEditView (0);
			}
		}
		break;
	}
}

//-----------------------------------------------------------------------------
long getIndexColor8Bit (CColor color, Display *pDisplay, Colormap colormap)
{
	long i;

	// search in pre-loaded color
	for (i = 0; i < CDrawContext::nbNewColor; i++)
	{
		if ((paletteNewColor[i].red   == color.red)   &&
				(paletteNewColor[i].green == color.green) &&
				(paletteNewColor[i].blue  == color.blue))
			return paletteNewColor[i].alpha;
	}
	
	// Allocate new color cell
	XColor xcolor;
	int red   = color.red   << 8;
	int green = color.green << 8;
	int blue  = color.blue  << 8;
	xcolor.red   = red;
	xcolor.green = green;
	xcolor.blue  = blue;	
	if (XAllocColor (pDisplay, colormap, &xcolor))
	{
		// store this new color
		if (CDrawContext::nbNewColor < 255) 
		{
			paletteNewColor[CDrawContext::nbNewColor].red    = color.red;
			paletteNewColor[CDrawContext::nbNewColor].green  = color.green;
			paletteNewColor[CDrawContext::nbNewColor].blue   = color.blue;
			paletteNewColor[CDrawContext::nbNewColor].alpha = xcolor.pixel;
			CDrawContext::nbNewColor++;
		}
		return xcolor.pixel;
	}
		
	// take the nearest color
	int diff;
	int min = 3 * 65536;
	int index = 0;

	XColor xcolors[256];
	for (i = 0; i < 256; i++)
		xcolors[i].pixel = i;

	XQueryColors (pDisplay, colormap, xcolors, 256);

	for (i = 0; i < 256; i++)
	{
		diff = fabs (xcolors[i].red - red) + fabs (xcolors[i].green - green) + fabs (xcolors[i].blue - blue);
		if (diff < min)
		{
			min = diff;
			index = i;
		}
	}

	// store this new color
	if (CDrawContext::nbNewColor < 255)
	{
		paletteNewColor[CDrawContext::nbNewColor].red    = color.red;
		paletteNewColor[CDrawContext::nbNewColor].green  = color.green;
		paletteNewColor[CDrawContext::nbNewColor].blue   = color.blue;
		paletteNewColor[CDrawContext::nbNewColor].alpha = index;
		CDrawContext::nbNewColor++;
	}
	return (index);
}

//-----------------------------------------------------------------------------
bool xpmGetValues (char **ppDataXpm, long *pWidth, long *pHeight, long *pNcolor, long *pCpp)
{
	// get the size of the pixmap
	sscanf (ppDataXpm[0], "%d %d %d %d", pWidth, pHeight, pNcolor, pCpp);
	
	return true;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
#elif BEOS
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
PlugView::PlugView (BRect frame, CFrame* cframe)
	: BView (frame, NULL, B_FOLLOW_ALL, B_WILL_DRAW), cframe (cframe)
{
	SetViewColor (B_TRANSPARENT_COLOR);
}

//-----------------------------------------------------------------------------
void PlugView::Draw (BRect updateRect)
{
	cframe->draw ();
}

//-----------------------------------------------------------------------------
void PlugView::MouseDown (BPoint where)
{
	BMessage*	m = Window ()->CurrentMessage ();
	int32 buttons;
	m->FindInt32 ("buttons", &buttons);
	
	if (buttons & B_SECONDARY_MOUSE_BUTTON && !Window ()->IsFront () && !Window ()->IsFloating ())
	{
		Window ()->Activate (true);
		return;
	}

	CDrawContext context (cframe, this, NULL);
	CPoint here (where.x, where.y);
	cframe->mouse (&context, here);
}

//-----------------------------------------------------------------------------
void PlugView::MessageReceived (BMessage *msg)
{
	if (msg->what == B_SIMPLE_DATA)
	{
		int32		countMax = 0;	// max number of references. Possibly not all valid...
		type_code	typeFound;
		msg->GetInfo ("refs", &typeFound, &countMax);
		if (countMax > 0)
		{ 
			entry_ref	item;
			int			nbRealItems = 0;
			char **		ptrItems = new char* [countMax];
			for (int k = 0; k < countMax; k++)
				if (msg->FindRef ("refs", k, &item) == B_OK)
				{
					BPath path (&item);
					if (path.InitCheck () == B_OK)
						ptrItems[nbRealItems++] = strdup (path.Path ());
				}
			BPoint bwhere = msg->DropPoint ();
			ConvertFromScreen (&bwhere);
			CPoint where (bwhere.x, bwhere.y);
			cframe->onDrop ((void**)ptrItems, nbRealItems, kDropFiles, where);
			for (long i = 0; i < nbRealItems; i++)
				free (ptrItems[i]);
			delete []ptrItems;
		}
	}
	else BView::MessageReceived (msg);
}

#endif


//-----------------------------------------------------------------------------
#if WINDOWS
//-----------------------------------------------------------------------------
// Drop Implementation
//-----------------------------------------------------------------------------
class CDropTarget : public IDropTarget
{	
public:
	CDropTarget (VSTGUI_CFrame* pFrame);
	virtual ~CDropTarget ();

	// IUnknown
	STDMETHOD (QueryInterface) (REFIID riid, void** object);
	STDMETHOD_ (ULONG, AddRef) (void);
	STDMETHOD_ (ULONG, Release) (void);
   
	// IDropTarget
	STDMETHOD (DragEnter) (IDataObject *dataObject, DWORD keyState, POINTL pt, DWORD *effect);
	STDMETHOD (DragOver) (DWORD keyState, POINTL pt, DWORD *effect);
	STDMETHOD (DragLeave) (void);
	STDMETHOD (Drop) (IDataObject *dataObject, DWORD keyState, POINTL pt, DWORD *effect);
private:
	long refCount;
	bool accept;
	VSTGUI_CFrame* pFrame;
};

//-----------------------------------------------------------------------------
// CDropTarget
//-----------------------------------------------------------------------------
void* createDropTarget (VSTGUI_CFrame* pFrame)
{
	return new CDropTarget (pFrame);
}

//-----------------------------------------------------------------------------
CDropTarget::CDropTarget (VSTGUI_CFrame* pFrame)
: refCount (0), pFrame (pFrame)
{
}

//-----------------------------------------------------------------------------
CDropTarget::~CDropTarget ()
{
}

//-----------------------------------------------------------------------------
STDMETHODIMP CDropTarget::QueryInterface (REFIID riid, void** object)
{
	if (riid == IID_IDropTarget || riid == IID_IUnknown)
	{
		*object = this;
		AddRef ();
      return NOERROR;
	}
	*object = 0;
	return E_NOINTERFACE;
}

//-----------------------------------------------------------------------------
STDMETHODIMP_(ULONG) CDropTarget::AddRef (void)
{
	return ++refCount;
}

//-----------------------------------------------------------------------------
STDMETHODIMP_(ULONG) CDropTarget::Release (void)
{
	refCount--;
	if (refCount <= 0)
	{
		delete this;
		return 0;
	}
	return refCount;
}

//-----------------------------------------------------------------------------
STDMETHODIMP CDropTarget::DragEnter (IDataObject *dataObject, DWORD keyState, POINTL pt, DWORD *effect)
{
	accept = false;
	if (dataObject)
	{
		FORMATETC formatTEXTDrop = {CF_TEXT, 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
		if (S_OK == dataObject->QueryGetData (&formatTEXTDrop))
		{
			accept = true;
			return DragOver (keyState, pt, effect);
		}

		FORMATETC formatHDrop = {CF_HDROP, 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
		if (S_OK == dataObject->QueryGetData (&formatHDrop))
		{
			accept = true;
			return DragOver (keyState, pt, effect);
		}
	}

	*effect = DROPEFFECT_NONE;
	return S_OK;
}

//-----------------------------------------------------------------------------
STDMETHODIMP CDropTarget::DragOver (DWORD keyState, POINTL pt, DWORD *effect)
{
	if (accept)
	{
		if (keyState & MK_CONTROL)
			*effect = DROPEFFECT_COPY;
		else
			*effect = DROPEFFECT_MOVE;
	}
	else
		*effect = DROPEFFECT_NONE;
	return S_OK;
}

//-----------------------------------------------------------------------------
STDMETHODIMP CDropTarget::DragLeave (void)
{
	return S_OK;
}

//-----------------------------------------------------------------------------
STDMETHODIMP CDropTarget::Drop (IDataObject *dataObject, DWORD keyState, POINTL pt, DWORD *effect)
{
	if (pFrame)
	{
		void* hDrop = 0;
		STGMEDIUM medium;
		FORMATETC formatTEXTDrop = {CF_TEXT,  0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
		FORMATETC formatHDrop    = {CF_HDROP, 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};

		long type = 0; // 0 = file, 1 = text

		HRESULT hr = dataObject->GetData (&formatTEXTDrop, &medium);
		if (hr != S_OK)
			hr = dataObject->GetData (&formatHDrop, &medium);
		else
			type = 1;
		if (hr == S_OK)
			hDrop = medium.hGlobal;

		if (hDrop)
		{
			switch (type)
			{
			//---File----------------------
			case 0:
				{
					long nbOfItems = (long)DragQueryFile ((HDROP)hDrop, 0xFFFFFFFFL, 0, 0);
					char fileDropped[1024];

					if (nbOfItems > 0) 
					{
						char **ptrItems = new char* [nbOfItems];
						long itemIndex = 0;
						long nbRealItems = 0;
						while (itemIndex < nbOfItems)
						{
							if (DragQueryFile ((HDROP)hDrop, itemIndex, fileDropped, sizeof (fileDropped))) 
							{
								// resolve link
								checkResolveLink (fileDropped, fileDropped);

								ptrItems[nbRealItems] = new char [sizeof (fileDropped)];
								strcpy ((char*)ptrItems[nbRealItems], fileDropped);
								nbRealItems++;
							}

							itemIndex++;
						}
						VSTGUI_CPoint where;
						pFrame->getCurrentLocation (where);
						pFrame->onDrop ((void**)ptrItems, nbOfItems, VSTGUI_kDropFiles, where);
						for (long i = 0; i < nbRealItems; i++)
							delete []ptrItems[i];
						delete []ptrItems;
					}
				} break;

			//---TEXT----------------------------
			case 1:
				{
					void* data = GlobalLock (medium.hGlobal);
					long dataSize = (long)GlobalSize (medium.hGlobal);
					if (data && dataSize)
					{
						VSTGUI_CPoint where;
						pFrame->getCurrentLocation (where);
						pFrame->onDrop ((void**)&data, dataSize, VSTGUI_kDropText, where);
					}

					GlobalUnlock (medium.hGlobal);
					if (medium.pUnkForRelease)
						medium.pUnkForRelease->Release ();
					else
						GlobalFree (medium.hGlobal);
				} break;
			}
		}
	}
	
	DragLeave ();
	return S_OK;
}

//-----------------------------------------------------------------------------
bool checkResolveLink (const char* nativePath, char* resolved)
{
	char* ext = strrchr (nativePath, '.');
	if (ext && stricmp (ext, ".lnk") == NULL)
	{
		IShellLink* psl;
		IPersistFile* ppf;
		WIN32_FIND_DATA wfd;
		HRESULT hres;
		WORD wsz[2048];
		
		// Get a pointer to the IShellLink interface.
		hres = CoCreateInstance (CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER,
			IID_IShellLink, (void**)&psl);
		if (SUCCEEDED (hres))
		{
			// Get a pointer to the IPersistFile interface.
			hres = psl->QueryInterface (IID_IPersistFile, (void**)&ppf);
			if (SUCCEEDED (hres))
			{
				// Ensure string is Unicode.
				MultiByteToWideChar (CP_ACP, 0, nativePath, -1, (LPWSTR)wsz, 2048);
				// Load the shell link.
				hres = ppf->Load ((LPWSTR)wsz, STGM_READ);
				if (SUCCEEDED (hres))
				{					
					hres = psl->Resolve (0, MAKELONG (SLR_ANY_MATCH | SLR_NO_UI, 500));
					if (SUCCEEDED (hres))
					{
						// Get the path to the link target.
						hres = psl->GetPath (resolved, 2048, &wfd, SLGP_SHORTPATH);
					}
				}
				// Release pointer to IPersistFile interface.
				ppf->Release ();
			}
			// Release pointer to IShellLink interface.
			psl->Release ();
		}
		return SUCCEEDED(hres);
	}
	return false;	
}

#elif MAC
#if MAC_OLD_DRAG
//-----------------------------------------------------------------------------
// Drop Implementation
//-----------------------------------------------------------------------------
#if !MACX
#include "Drag.h"
#endif

pascal short drag_receiver (WindowPtr w, void* ref, DragReference drag);

static DragReceiveHandlerUPP drh;

static bool gEventDragWorks = false;
//-------------------------------------------------------------------------------------------
void install_drop (CFrame *frame)
{
	drh = NewDragReceiveHandlerUPP (drag_receiver);
#if CARBON
	InstallReceiveHandler (drh, (WindowRef)(frame->getSystemWindow ()), (void*)frame);
#else
	InstallReceiveHandler (drh, (GrafPort*)(frame->getSystemWindow ()), (void*)frame);
#endif
}

//-------------------------------------------------------------------------------------------
void remove_drop (CFrame *frame)
{
#if CARBON
	RemoveReceiveHandler (drh, (WindowRef)(frame->getSystemWindow ()));
#else
	RemoveReceiveHandler (drh, (GrafPort*)(frame->getSystemWindow ()));
#endif
}

//-------------------------------------------------------------------------------------------
// Drop has happened in one of our's windows.
// The data is either of our own type (flavour type stCA), or comes from
// another app. The only data from outside that is currently accepted are
// HFS-files
//-------------------------------------------------------------------------------------------
pascal short drag_receiver (WindowPtr w, void* ref, DragReference drag)
{
	#if QUARTZ
	if (gEventDragWorks)
		return noErr;
	#endif

	unsigned short i, items;
	ItemReference item;
	long          size;
	HFSFlavor     hfs;
	void*         pack;

	// get num of items
	CountDragItems (drag, &items);
	if (items <= 0)
		return cantGetFlavorErr;
			
	char **ptrItems = new char* [items];
	long nbFileItems = 0;
	CFrame *pFrame = (CFrame*)ref;
	char* string = 0;
		
	// for each items
	for (i = 1; i <= items; i++)
	{
		pack = NULL;
	
		GetDragItemReferenceNumber (drag, i, &item);

		//---try file--------------------------
		if (GetFlavorDataSize (drag, item, flavorTypeHFS, &size) == noErr)
		{ 
			GetFlavorData (drag, item, flavorTypeHFS, &hfs, &size, 0L);
			
			ptrItems[nbFileItems] = new char [sizeof (FSSpec)];
			memcpy (ptrItems[nbFileItems], &hfs.fileSpec, sizeof (FSSpec));
			nbFileItems++;
		}
		
		//---try Text-------------------------
		else if (GetFlavorDataSize (drag, item, 'TEXT', &size) == noErr)
		{
			string = new char [size + 2];
			if (string)
			{
				GetFlavorData (drag, item, 'TEXT', string, &size, 0);
				string[size] = 0;
			}
			break;
		}
		
		//---try XML text----------------------
		else if (GetFlavorDataSize (drag, item, 'XML ', &size) == noErr)
		{
			string = new char [size + 2];
			if (string)
			{
				GetFlavorData (drag, item, 'XML ', string, &size, 0);
				string[size] = 0;
			}
			break;
		}
	} // end for eac items
	
	// call the frame
	if (nbFileItems)
	{
		VSTGUI_CPoint where;
		pFrame->getCurrentLocation (where);
		pFrame->onDrop ((void**)ptrItems, nbFileItems, VSTGUI_kDropFiles, where);
		for (long i = 0; i < nbFileItems; i++)
				delete []ptrItems[i];
		delete []ptrItems;
		return noErr;
	}
	if (string)
	{
		VSTGUI_CPoint where;
		pFrame->getCurrentLocation (where);
		pFrame->onDrop ((void**)&string, size, VSTGUI_kDropText, where);
				
		delete []string;
	}
	
	
	delete []ptrItems;
	return cantGetFlavorErr;
}
#endif // MAC_OLD_DRAG

#if QUARTZ
#define defControlStringMask	CFSTR ("net.sourceforge.vstgui.%d")

bool CFrame::registerWithToolbox ()
{
	CFStringRef defControlString = CFStringCreateWithFormat (NULL, NULL, defControlStringMask, this);

	controlSpec.defType = kControlDefObjectClass;
	controlSpec.u.classRef = NULL;

	EventTypeSpec eventTypes[] = {	{kEventClassControl, kEventControlDraw},
									{kEventClassControl, kEventControlHitTest},
									{kEventClassControl, kEventControlClick},
									{kEventClassControl, kEventControlTrack},
									{kEventClassControl, kEventControlContextualMenuClick},
									//{kEventClassKeyboard, kEventRawKeyDown},
									//{kEventClassKeyboard, kEventRawKeyRepeat},
									{kEventClassMouse, kEventMouseWheelMoved},
									{kEventClassControl, kEventControlDragEnter},
									{kEventClassControl, kEventControlDragWithin},
									{kEventClassControl, kEventControlDragLeave},
									{kEventClassControl, kEventControlDragReceive},
									{kEventClassControl, kEventControlInitialize},
									//{kEventClassControl, kEventControlSetFocusPart},
									//{kEventClassControl, kEventControlGetFocusPart}
								};

	ToolboxObjectClassRef controlClass = NULL;

	OSStatus status = RegisterToolboxObjectClass (	defControlString,
													NULL,
													GetEventTypeCount (eventTypes),
													eventTypes,
													CFrame::carbonEventHandler,
													this,
													&controlClass);
	if (status == noErr)
		controlSpec.u.classRef = controlClass;

	CFRelease (defControlString);

	return (controlSpec.u.classRef != NULL);
}

//---------------------------------------------------------------------------------------
pascal OSStatus CFrame::carbonEventHandler (EventHandlerCallRef inHandlerCallRef, EventRef inEvent, void *inUserData)
{
	OSStatus result = eventNotHandledErr;
	CFrame* frame = (CFrame*)inUserData;
	EventClass eventClass = GetEventClass (inEvent);
	EventKind eventKind = GetEventKind (inEvent);
	WindowRef window = (WindowRef)frame->getSystemWindow ();

	// WARNING :
	// I've not implemented the old style resource file handling.
	// Use the CFBundleCopyResourceURL... functions to get your resources.

	// with quartz we only set the port because of the old style getMouseLocation call in CDrawContext
	// if this lib changes its internal event handling, we don't need it anymore !
//	GrafPtr	savedPort;
//	bool portChanged = QDSwapPort (GetWindowPort (window), &savedPort);

	switch (eventClass)
	{
		case kEventClassControl:
		{
			switch (eventKind)
			{
				case kEventControlInitialize:
				{
					UInt32 controlFeatures = kControlSupportsDragAndDrop | kControlSupportsFocus | kControlHandlesTracking | kControlSupportsEmbedding;
					SetEventParameter (inEvent, kEventParamControlFeatures, typeUInt32, sizeof (UInt32), &controlFeatures);
					result = noErr;
					break;
				}
				case kEventControlDraw:
				{
					CGContextRef cgcontext = 0;
					OSStatus result = GetEventParameter (inEvent, kEventParamCGContextRef, typeCGContextRef, NULL, sizeof (cgcontext), NULL, &cgcontext);
					CDrawContext context (frame, (result == noErr) ? cgcontext : NULL, window);
					RgnHandle dirtyRegion;
					if (GetEventParameter (inEvent, kEventParamRgnHandle, typeQDRgnHandle, NULL, sizeof (RgnHandle), NULL, &dirtyRegion) == noErr)
					{
						Rect bounds;
						GetRegionBounds (dirtyRegion, &bounds);
						CRect updateRect;
						Rect2CRect (bounds, updateRect);
						WindowAttributes windowAttributes;
						GetWindowAttributes (window, &windowAttributes);
						if (!(windowAttributes & kWindowCompositingAttribute))
							updateRect.offset (-context.offsetScreen.x, -context.offsetScreen.y);
						frame->drawRect (&context, updateRect);
					}
					else
						frame->draw (&context);
					result = noErr;
					break;
				}
				case kEventControlHitTest:
				{
					ControlPartCode code = kControlContentMetaPart;
					SetEventParameter (inEvent, kEventParamControlPart, typeControlPartCode, sizeof (ControlPartCode), &code);
					result = noErr;
					break;
				}
				case kEventControlClick:
				{
					EventMouseButton buttonState;
					GetEventParameter (inEvent, kEventParamMouseButton, typeMouseButton, NULL, sizeof (EventMouseButton), NULL, &buttonState);
					if (buttonState == kEventMouseButtonPrimary)
					{
						result = CallNextEventHandler (inHandlerCallRef, inEvent);
						break;
					}
				}
				case kEventControlTrack:
				case kEventControlContextualMenuClick:
				{
					long buttons = 0;
					EventMouseButton buttonState;
					HIPoint hipoint;
					UInt32 modifiers;
					GetEventParameter (inEvent, kEventParamMouseLocation, typeHIPoint, NULL, sizeof (HIPoint), NULL, &hipoint);
					if (eventKind == kEventControlContextualMenuClick)
						buttons = kRButton;
					else if (eventKind == kEventControlTrack)
					{
						buttons = kLButton;
						GetEventParameter (inEvent, kEventParamKeyModifiers, typeUInt32, NULL, sizeof (UInt32), NULL, &modifiers);
						if (modifiers & cmdKey)
							buttons |= kControl;
						if (modifiers & shiftKey)
							buttons |= kShift;
						if (modifiers & optionKey)
							buttons |= kAlt;
						if (modifiers & controlKey)
							buttons |= kApple;
					}
					else
					{
						GetEventParameter (inEvent, kEventParamKeyModifiers, typeUInt32, NULL, sizeof (UInt32), NULL, &modifiers);
						GetEventParameter (inEvent, kEventParamMouseButton, typeMouseButton, NULL, sizeof (EventMouseButton), NULL, &buttonState);
						if (buttonState == kEventMouseButtonPrimary)
							buttons |= kLButton;
						if (buttonState == kEventMouseButtonSecondary)
							buttons |= kRButton;
						if (buttonState == kEventMouseButtonTertiary)
							buttons |= kMButton;
						if (modifiers & cmdKey)
							buttons |= kControl;
						if (modifiers & shiftKey)
							buttons |= kShift;
						if (modifiers & optionKey)
							buttons |= kAlt;
						if (modifiers & controlKey)
							buttons |= kApple;
					}
					//SetUserFocusWindow (window);
					//AdvanceKeyboardFocus (window);
					//SetKeyboardFocus (window, frame->controlRef, kControlFocusNextPart);
					Point point = {hipoint.y, hipoint.x};
					if (eventKind == kEventControlClick)
						QDGlobalToLocalPoint (GetWindowPort (window), &point);
					CDrawContext context (frame, NULL, window);
					CPoint p (point.h, point.v);
					WindowAttributes windowAttributes;
					GetWindowAttributes (window, &windowAttributes);
					if (!(eventKind == kEventControlContextualMenuClick && windowAttributes & kWindowCompositingAttribute))
						p.offset (-context.offsetScreen.x, -context.offsetScreen.y);
					frame->mouse (&context, p, buttons);
					result = noErr;
					break;
				}
				case kEventControlGetFocusPart:
				{
					ControlPartCode code = frame->hasFocus ? kControlContentMetaPart : kControlFocusNoPart;
					SetEventParameter (inEvent, kEventParamControlPart, typeControlPartCode, sizeof (ControlPartCode), &code);
					result = noErr;
					break;
				}
				case kEventControlSetFocusPart:
				{
					ControlPartCode code;
					GetEventParameter (inEvent, kEventParamControlPart, typeControlPartCode, NULL, sizeof (ControlPartCode), NULL, &code);
					if (code == kControlFocusNoPart)
						frame->hasFocus = false;
					else
						frame->hasFocus = true;
					result = noErr;
					break;
				}
				case kEventControlDragEnter:
				{
					#if MAC_OLD_DRAG
					gEventDragWorks = true;
					#endif
					DragRef dragRef;
					if (GetEventParameter (inEvent, kEventParamDragRef, typeDragRef, NULL, sizeof (DragRef), NULL, &dragRef) == noErr)
					{
						UInt16 numItems;
						CountDragItems (dragRef, &numItems);
						if (numItems > 0)
						{
							long size;
							for (UInt16 i = 1; i <= numItems; i++)
							{
								DragItemRef itemRef;
								if (GetDragItemReferenceNumber (dragRef, i, &itemRef) == noErr)
								{
									if (GetFlavorDataSize (dragRef, itemRef, flavorTypeHFS, &size) == noErr)
										result = noErr;
									if (GetFlavorDataSize (dragRef, itemRef, 'TEXT', &size) == noErr)
										result = noErr;
									if (GetFlavorDataSize (dragRef, itemRef, 'XML ', &size) == noErr)
										result = noErr;
									if (result == noErr)
										break;
								}
							}
						}
						if (result == noErr)
						{
							Boolean accept = true;
							SetEventParameter (inEvent, 'cldg' /*kEventParamControlWouldAcceptDrop*/, typeBoolean, sizeof (Boolean), &accept);
							SetThemeCursor (kThemeCopyArrowCursor);
						}
					}
					break;
				}
				case kEventControlDragWithin:
				{
					result = noErr;
					break;
				}
				case kEventControlDragLeave:
				{
					SetThemeCursor (kThemeArrowCursor);
					result = noErr;
					break;
				}
				case kEventControlDragReceive:
				{
					DragRef dragRef;
					if (GetEventParameter (inEvent, kEventParamDragRef, typeDragRef, NULL, sizeof (DragRef), NULL, &dragRef) == noErr)
					{
						UInt16 numItems;
						CountDragItems (dragRef, &numItems);
						if (numItems > 0)
						{
							VSTGUI_CPoint where (-1, -1);
							char **ptrItems = new char* [numItems];
							long nbFileItems = 0;
							char* string = 0;
							long size;
							HFSFlavor hfs;
							for (UInt16 i = 1; i <= numItems; i++)
							{
								DragItemRef itemRef;
								if (GetDragItemReferenceNumber (dragRef, i, &itemRef) == noErr)
								{
									//---try file--------------------------
									if (GetFlavorDataSize (dragRef, itemRef, flavorTypeHFS, &size) == noErr)
									{ 
										GetFlavorData (dragRef, itemRef, flavorTypeHFS, &hfs, &size, 0L);
										
										ptrItems[nbFileItems] = new char [sizeof (FSSpec)];
										memcpy (ptrItems[nbFileItems], &hfs.fileSpec, sizeof (FSSpec));
										nbFileItems++;
									}
									
									//---try Text-------------------------
									else if (GetFlavorDataSize (dragRef, itemRef, 'TEXT', &size) == noErr)
									{
										string = new char [size + 2];
										if (string)
										{
											GetFlavorData (dragRef, itemRef, 'TEXT', string, &size, 0);
											string[size] = 0;
										}
										break;
									}
									
									//---try XML text----------------------
									else if (GetFlavorDataSize (dragRef, itemRef, 'XML ', &size) == noErr)
									{
										string = new char [size + 2];
										if (string)
										{
											GetFlavorData (dragRef, itemRef, 'XML ', string, &size, 0);
											string[size] = 0;
										}
										break;
									}
								} // end for eac items
							}
							// call the frame
							frame->getCurrentLocation (where);
							if (nbFileItems)
							{
								frame->onDrop ((void**)ptrItems, nbFileItems, VSTGUI_kDropFiles, where);
								for (long i = 0; i < nbFileItems; i++)
										delete []ptrItems[i];
								delete []ptrItems;
								return noErr;
							}
							if (string)
							{
								frame->onDrop ((void**)&string, size, VSTGUI_kDropText, where);
										
								delete []string;
							}
						}
					}
					break;
				}
			}
			break;
		}
		case kEventClassMouse:
		{
			switch (eventKind)
			{
				case kEventMouseWheelMoved:
				{
					HIPoint hipoint;
					SInt32 wheelDelta;
					EventMouseWheelAxis wheelAxis;
					WindowRef windowRef;
					GetEventParameter (inEvent, kEventParamWindowRef, typeWindowRef, NULL, sizeof (WindowRef), NULL, &windowRef);
					GetEventParameter (inEvent, kEventParamMouseLocation, typeHIPoint, NULL, sizeof (HIPoint), NULL, &hipoint);
					GetEventParameter (inEvent, kEventParamMouseWheelAxis, typeMouseWheelAxis, NULL, sizeof (EventMouseWheelAxis), NULL, &wheelAxis);
					GetEventParameter (inEvent, kEventParamMouseWheelDelta, typeLongInteger, NULL, sizeof (SInt32), NULL, &wheelDelta);
					Point point = {hipoint.y, hipoint.x};
					QDGlobalToLocalPoint (GetWindowPort (window), &point);
					CDrawContext context (frame, NULL, window);
					CPoint p (point.h, point.v);
					p.offset (-context.offsetScreen.x, -context.offsetScreen.y);
					frame->onWheel (&context, p, wheelDelta);					
					result = noErr;
					break;
				}
			}
			break;
		}
		case kEventClassKeyboard:
		{
			switch (eventKind)
			{
				case kEventRawKeyDown:
				case kEventRawKeyRepeat:
				{
					// todo: make this work
					/*
					char character = 0;
					UInt32 keyCode = 0;
					UInt32 modifiers = 0;
					GetEventParameter (inEvent, kEventParamKeyMacCharCodes, typeChar, NULL, sizeof (char), NULL, &character);
					GetEventParameter (inEvent, kEventParamKeyCode, typeChar, NULL, sizeof (UInt32), NULL, &keyCode);
					GetEventParameter (inEvent, kEventParamKeyModifiers, typeChar, NULL, sizeof (UInt32), NULL, &modifiers);
					VstKeyCode vstKeyCode;
					vstKeyCode.character = character;
					vstKeyCode.virt = 0; // we need a lookup table here !!!
					vstKeyCode.modifier = 0;
					if (modifiers & cmdKey)
						vstKeyCode.modifier |= MODIFIER_CONTROL;
					if (modifiers & shiftKey)
						vstKeyCode.modifier |= MODIFIER_SHIFT;
					if (modifiers & optionKey)
						vstKeyCode.modifier |= MODIFIER_ALTERNATE;
					if (modifiers & controlKey)
						vstKeyCode.modifier |= MODIFIER_COMMAND;
					if (frame->onKeyDown (vstKeyCode) != -1)
						result = noErr;
					*/
					break;
				}
			}
			break;
		}
	}
//	if (portChanged)
//		QDSwapPort (savedPort, NULL);
	return result;
}
#endif

#endif

