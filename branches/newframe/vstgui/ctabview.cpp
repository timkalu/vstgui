//-----------------------------------------------------------------------------
// VST Plug-Ins SDK
// VSTGUI: Graphical User Interface Framework not only for VST plugins : 
//
// CTabView written 2004 by Arne Scheffler
//
//-----------------------------------------------------------------------------
// VSTGUI LICENSE
// � 2004, Steinberg Media Technologies, All Rights Reserved
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

#ifndef __ctabview__
#include "ctabview.h"
#endif

BEGIN_NAMESPACE_VSTGUI

//-----------------------------------------------------------------------------
class CTabButton : public COnOffButton
//-----------------------------------------------------------------------------
{
public:
	CTabButton (const CRect &size, CControlListener *listener, long tag, CBitmap *background, const char* inName)
	: COnOffButton (size, listener, tag, background)
	{
		if (inName)
		{
			name = (char*)malloc (strlen (inName) + 1);
			strcpy (name, inName);
		}
		activeTextColor = kBlackCColor;
		inactiveTextColor = kWhiteCColor;
		textFont = kSystemFont;
	}

	virtual ~CTabButton ()
	{
		if (name)
			free (name);
	}	

	virtual void draw (CDrawContext *pContext)
	{
		COnOffButton::draw (pContext);
		if (name)
		{
			pContext->setFont (textFont);
			pContext->setFontColor (value ? activeTextColor : inactiveTextColor);
			pContext->drawString (name, size, false);
		}
	}

	void mouse (CDrawContext *pContext, CPoint &where, long button)
	{
		if (!bMouseEnabled)
			return;

	 	if (button == -1) button = pContext->getMouseButtons ();
		if (!(button & kLButton))
			return;

		value = ((long)value) ? 0.f : 1.f;
		
		if (listener)
			listener->valueChanged (pContext, this);
	}

	virtual void onDragEnter (CDrawContext* context, CDragContainer* drag, const CPoint& where)
	{
		if (value == 0.f)
		{
			value = 1.f;
			if (listener)
				listener->valueChanged (context, this);
		}
	}

	void setTextFont (const CFont& font) { textFont = font; }
	void setActiveTextColor (const CColor& color) { activeTextColor = color; }
	void setInactiveTextColor (const CColor& color) { inactiveTextColor = color; }

	CLASS_METHODS (CTabButton, COnOffButton)
protected:
	char* name;
	CFont textFont;
	CColor activeTextColor;
	CColor inactiveTextColor;
};

//-----------------------------------------------------------------------------
class CTabChildView : public CReferenceCounter
//-----------------------------------------------------------------------------
{
public:
	CTabChildView (CView* view)
	: view (view)
	, previous (0)
	, next (0)
	, button (0)
	{
		view->remember ();
	}

	virtual ~CTabChildView ()
	{
		view->forget ();
	}

	CView* view;
	CTabChildView* previous;
	CTabChildView* next;
	CTabButton* button;
};

//-----------------------------------------------------------------------------
CTabView::CTabView (const CRect& size, CFrame* parent, CBitmap* tabBitmap, CBitmap* background, long tabPosition, long style)
: CViewContainer (size, parent, background)
, tabBitmap (tabBitmap)
, tabPosition (tabPosition)
, style (style)
, firstChild (0)
, lastChild (0)
, currentChild (0)
, numberOfChilds (0)
{
	if (tabBitmap)
		tabBitmap->remember ();
	setMode (kOnlyDirtyUpdate);
}

//-----------------------------------------------------------------------------
CTabView::~CTabView ()
{
	removeAllTabs ();
	if (tabBitmap)
		tabBitmap->forget ();
}

//-----------------------------------------------------------------------------
bool CTabView::addTab (CView* view, const char* name)
{
	if (!view)
		return false;

	CRect ts (0, 0, tabBitmap->getWidth (), tabBitmap->getHeight () / 2);
	switch (tabPosition)
	{
		case kPositionTop:
			ts.offset (tabBitmap->getWidth () * numberOfChilds, 0); break;
		case kPositionBottom:
			ts.offset (tabBitmap->getWidth () * numberOfChilds, size.getHeight () - tabBitmap->getHeight () / 2); break;
		case kPositionLeft:
			ts.offset (0, tabBitmap->getHeight () / 2 * numberOfChilds); break;
		case kPositionRight:
			ts.offset (size.getWidth () - tabBitmap->getWidth (), tabBitmap->getHeight () / 2 * numberOfChilds); break;
	}
	CTabButton* b = new CTabButton (ts, this, numberOfChilds, tabBitmap, name);
	b->setTransparency (true);
	addView (b);
	CTabChildView* v = new CTabChildView (view);
	v->button = b;
	if (lastChild)
	{
		lastChild->next = v;
		v->previous = lastChild;
		lastChild = v;
	}
	else
	{
		firstChild = lastChild = v;
		setCurrentChild (v);
	}
	numberOfChilds++;
	return true;
}

//-----------------------------------------------------------------------------
bool CTabView::removeTab (CView* view)
{
	if (!view)
		return false;
	
	CTabChildView* v = firstChild;
	while (v)
	{
		if (v->view == view)
		{
			if (v->previous)
				v->previous->next = v->next;
			if (v->next)
				v->next->previous = v->previous;
			if (v == currentChild)
				setCurrentChild (v->previous ? v->previous : v->next);
			removeView (v->button);
			v->forget ();
			numberOfChilds--;
			return true;
		}
		v = v->next;
	}
	return false;
}

//-----------------------------------------------------------------------------
bool CTabView::removeAllTabs ()
{
	setCurrentChild (0);
	CTabChildView* v = firstChild;
	while (v)
	{
		CTabChildView* next = v->next;
		removeView (v->button);
		v->forget ();
		v = next;
	}
	firstChild = 0;
	lastChild = 0;
	numberOfChilds = 0;
	return true;
}

//-----------------------------------------------------------------------------
bool CTabView::selectTab (long index)
{
	if (index > numberOfChilds)
		return false;
	CTabChildView* v = firstChild;
	long i = 0;
	while (v)
	{
		if (index == i)
			break;
		v = v->next;
		i++;
	}
	if (v)
	{
		setCurrentChild (v);
		return true;
	}
	return false;
}

//-----------------------------------------------------------------------------
void CTabView::setCurrentChild (CTabChildView* childView)
{
	if (childView == currentChild)
	{
		if (currentChild->button)
			currentChild->button->setValue (1.f);
		return;
	}
	if (currentChild)
	{
		if (currentChild->button)
			currentChild->button->setValue (0.f);
		removeView (currentChild->view, false);
	}
	currentChild = childView;
	if (currentChild)
	{
		addView (currentChild->view);
		if (currentChild->button)
			currentChild->button->setValue (1.f);
		currentChild->view->setDirty ();
	}
	else
		setDirty ();
}

//-----------------------------------------------------------------------------
void CTabView::valueChanged (CDrawContext *pContext, CControl *pControl)
{
	if (pControl->isTypeOf ("CTabButton"))
		selectTab (pControl->getTag ());
}

//-----------------------------------------------------------------------------
CRect& CTabView::getTabViewSize (CRect& rect) const
{
	rect = size;
	rect.offset (-size.left, -size.top);
	switch (tabPosition)
	{
		case kPositionTop:
			rect.top += tabBitmap->getHeight () / 2; break;
		case kPositionBottom:
			rect.bottom -= tabBitmap->getHeight () / 2; break;
		case kPositionLeft:
			rect.left += tabBitmap->getWidth (); break;
		case kPositionRight:
			rect.right -= tabBitmap->getWidth (); break;
	}
	return rect;
}


END_NAMESPACE_VSTGUI
