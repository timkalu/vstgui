//-----------------------------------------------------------------------------
// VST Plug-Ins SDK
// VSTGUI: Graphical User Interface Framework for VST plugins
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

#include "../../unittests.h"
#include "../../../../uidescription/uiviewfactory.h"
#include "../../../../uidescription/uiattributes.h"
#include "../../../../uidescription/detail/uiviewcreatorattributes.h"
#include "../../../../lib/cviewcontainer.h"
#include "../../../../lib/cstring.h"
#include "helpers.h"

namespace VSTGUI {
using namespace UIViewCreator;

namespace {

constexpr IdStringPtr kColorName = "MyColor";

class MyUIDescription : public UIDescriptionAdapter
{
public:
	bool getColor (UTF8StringPtr name, CColor& color) const override
	{
		if (UTF8StringView(name) == kColorName)
		{
			color = this->color;
			return true;
		}
		return false;
	}

	UTF8StringPtr lookupColorName (const CColor& color) const override
	{
		if (this->color == color)
			return kColorName;
		return nullptr;
	}

	CColor color {20, 30, 50, 255};
};


} // anonymous

TESTCASE(CViewContainerCreatorTest,

	TEST(backgroundColor,
		MyUIDescription uidesc;
		testAttribute<CViewContainer>(kCViewContainer, kAttrBackgroundColor, kColorName, &uidesc, [&] (CViewContainer* v) {
			return v->getBackgroundColor () == uidesc.color;
		});
		testAttribute<CViewContainer>(kCViewContainer, kAttrBackgroundColor, kColorName, &uidesc, [&] (CViewContainer* v) {
			return v->getBackgroundColor () == uidesc.color;
		}, true);
	);
	
	TEST(backgroundColorDrawStyle,
		MyUIDescription uidesc;
		testAttribute<CViewContainer>(kCViewContainer, kAttrBackgroundColorDrawStyle, "stroked", &uidesc, [] (CViewContainer* v) {
			return v->getBackgroundColorDrawStyle () == kDrawStroked;
		});
		testAttribute<CViewContainer>(kCViewContainer, kAttrBackgroundColorDrawStyle, "filled", &uidesc, [] (CViewContainer* v) {
			return v->getBackgroundColorDrawStyle () == kDrawFilled;
		});
		testAttribute<CViewContainer>(kCViewContainer, kAttrBackgroundColorDrawStyle, "filled and stroked", &uidesc, [] (CViewContainer* v) {
			return v->getBackgroundColorDrawStyle () == kDrawFilledAndStroked;
		});
	);

	TEST(backgroundColorDrawStyleValues,
		MyUIDescription uidesc;
		testPossibleValues (kCViewContainer, kAttrBackgroundColorDrawStyle, &uidesc, {"stroked", "filled", "filled and stroked"});
	);
);

} // VSTGUI