//-----------------------------------------------------------------------------
// VST Plug-Ins SDK
// VSTGUI: Graphical User Interface Framework not only for VST plugins : 
//
// Version 3.6
//
//-----------------------------------------------------------------------------
// VSTGUI LICENSE
// (c) 2009, Steinberg Media Technologies, All Rights Reserved
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

#ifndef __vst3editor__
#define __vst3editor__

#include "public.sdk/source/vst/vstguieditor.h"
#include "pluginterfaces/vst/ivstplugview.h"
#include "uidescription.h"
#include "../ctooltipsupport.h"
#include <string>
#include <map>

#ifdef verify
#undef verify
#endif

BEGIN_NAMESPACE_VSTGUI
class ParameterChangeListener;
class VST3Editor;

//-----------------------------------------------------------------------------
class IVST3CustomViewCreator
{
public:
	virtual ~IVST3CustomViewCreator () {}
	
	virtual CView* createCustomView (const char* name, const UIAttributes& attributes, IUIDescription* description) = 0;
	virtual bool findParameter (const CPoint& pos, Steinberg::Vst::ParamID& paramID, VST3Editor* editor) { return false; }
};

//-----------------------------------------------------------------------------
class VST3Editor : public Steinberg::Vst::VSTGUIEditor, public Steinberg::Vst::IParameterFinder, public IController
{
public:
	VST3Editor (void* controller, const char* viewName, const char* xmlFile, bool debugMode = false);
	VST3Editor (UIDescription* desc, void* controller, const char* viewName, const char* xmlFile = 0, bool debugMode = false);

	bool verify ();
	bool exchangeView (const char* newViewName);
	void enableTooltips (bool state);

//-----------------------------------------------------------------------------
	DELEGATE_REFCOUNT(Steinberg::Vst::VSTGUIEditor)
	Steinberg::tresult PLUGIN_API queryInterface (const ::Steinberg::TUID iid, void** obj);
protected:
	~VST3Editor ();
	void init ();
	ParameterChangeListener* getParameterChangeListener (long tag);
	void recreateView ();

	#if VSTGUI_LIVE_EDITING
	void runNewTemplateDialog (const char* baseViewName);
	void runTemplateSettingsDialog ();
	#endif // VSTGUI_LIVE_EDITING
	
	bool PLUGIN_API open (void* parent);
	void PLUGIN_API close ();

	void beginEdit (long index);
	void endEdit (long index);

	CView* createView (const UIAttributes& attributes, IUIDescription* description);
	CView* verifyView (CView* view, const UIAttributes& attributes, IUIDescription* description);
	CMessageResult notify (CBaseObject* sender, const char* message);

	Steinberg::tresult PLUGIN_API onSize (Steinberg::ViewRect* newSize);
	Steinberg::tresult PLUGIN_API canResize ();
	Steinberg::tresult PLUGIN_API checkSizeConstraint (Steinberg::ViewRect* rect);

	Steinberg::tresult PLUGIN_API onWheel (float distance);
	Steinberg::tresult PLUGIN_API onKeyDown (Steinberg::char16 key, Steinberg::int16 keyMsg, Steinberg::int16 modifiers);
	Steinberg::tresult PLUGIN_API onKeyUp (Steinberg::char16 key, Steinberg::int16 keyMsg, Steinberg::int16 modifiers);

	// IParameterFinder
	Steinberg::tresult PLUGIN_API findParameter (Steinberg::int32 xPos, Steinberg::int32 yPos, Steinberg::Vst::ParamID& resultTag);

	// CControlListener
	virtual void valueChanged (CControl* pControl);
	virtual void controlBeginEdit (CControl* pControl);
	virtual void controlEndEdit (CControl* pControl);

	UIDescription* description;
	CTooltipSupport* tooltipSupport;
	std::map<long, ParameterChangeListener*> paramChangeListeners;
	std::string viewName;
	std::string xmlFile;
	bool debugMode;
	bool tooltipsEnabled;
	bool doCreateView;
	
	CPoint minSize;
	CPoint maxSize;
};

END_NAMESPACE_VSTGUI

#endif
