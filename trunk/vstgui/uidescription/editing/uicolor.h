//-----------------------------------------------------------------------------
// VST Plug-Ins SDK
// VSTGUI: Graphical User Interface Framework not only for VST plugins
//
// Version 4.2
//
//-----------------------------------------------------------------------------
// VSTGUI LICENSE
// (c) 2013, Steinberg Media Technologies, All Rights Reserved
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
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
// IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
// INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
// BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
// OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE  OF THIS SOFTWARE, EVEN IF ADVISED
// OF THE POSSIBILITY OF SUCH DAMAGE.
//-----------------------------------------------------------------------------

#ifndef __uicolor__
#define __uicolor__

#include "../../lib/vstguibase.h"

#if VSTGUI_LIVE_EDITING

#include "../../lib/ccolor.h"
#include "../../lib/idependency.h"

namespace VSTGUI {

//----------------------------------------------------------------------------------------------------
class UIColor : protected CColor, public CBaseObject, public IDependency
{
public:
	UIColor () : CColor (kTransparentCColor), hue (0), saturation (0), lightness (0) {}

	UIColor& operator= (const CColor& c);
	const CColor& base () const { return *this; }

	double getRed () const { return r; }
	double getGreen () const { return g; }
	double getBlue () const { return b; }
	double getAlpha () const { return alpha; }
	
	double getHue () const { return hue; }
	double getSaturation () const { return saturation; }
	double getLightness () const { return lightness; }
	
	void setHue (double h);
	void setSaturation (double s);
	void setLightness (double l);

	void setRed (double nr);
	void setGreen (double ng);
	void setBlue (double nb);
	void setAlpha (double na);

	void beginEdit ();
	void endEdit ();
	
	static IdStringPtr kMsgChanged;
	static IdStringPtr kMsgEditChange;
	static IdStringPtr kMsgBeginEditing;
	static IdStringPtr kMsgEndEditing;
private:
	enum HSLUpdateDirection
	{
		kFrom,
		kTo
	};

	void updateHSL (HSLUpdateDirection direction);
	
	double hue, saturation, lightness;
	double r, g, b;
};

} // namespace

#endif // VSTGUI_LIVE_EDITING

#endif // __uicolor__
