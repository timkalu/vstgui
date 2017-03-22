#include "mandelbrotwindow.h"
#include "mandelbrot.h"
#include "mandelbrotview.h"
#include "modelbinding.h"
#include "touchbarsupport.h"
#include "vstgui/lib/cbitmap.h"
#include "vstgui/lib/ccolor.h"
#include "vstgui/lib/cframe.h"
#include "vstgui/lib/cfileselector.h"
#include "vstgui/lib/iscalefactorchangedlistener.h"
#include "vstgui/lib/iviewlistener.h"
#include "vstgui/standalone/include/helpers/uidesc/customization.h"
#include "vstgui/standalone/include/helpers/value.h"
#include "vstgui/standalone/include/helpers/valuelistener.h"
#include "vstgui/standalone/include/helpers/windowcontroller.h"
#include "vstgui/standalone/include/iasync.h"
#include "vstgui/standalone/include/iapplication.h"
#include "vstgui/standalone/include/iuidescwindow.h"
#include "vstgui/uidescription/delegationcontroller.h"
#include "vstgui/uidescription/iuidescription.h"
#include "vstgui/uidescription/uiattributes.h"
#include "vstgui/uidescription/cstream.h"
#include <atomic>
#include <cassert>
#include <thread>

//------------------------------------------------------------------------
namespace Mandelbrot {

using namespace VSTGUI;
using namespace VSTGUI::Standalone;
using namespace VSTGUI::Standalone::Application;

//------------------------------------------------------------------------
inline CColor calculateColor (uint32_t iteration, double maxIterationInv)
{
	CColor color;
	const auto t = static_cast<double> (iteration) * maxIterationInv;
	color.red = static_cast<uint8_t> (9. * (1. - t) * t * t * t * 255.);
	color.green = static_cast<uint8_t> (15. * (1. - t) * (1. - t) * t * t * 255.);
	color.blue = static_cast<uint8_t> (8.5 * (1. - t) * (1. - t) * (1. - t) * t * 255.);
	return color;
}

//------------------------------------------------------------------------
inline std::function<uint32_t (CColor)> getColorToInt32 (IPlatformBitmapPixelAccess::PixelFormat f)
{
	switch (f)
	{
		case IPlatformBitmapPixelAccess::kARGB:
		{
			return [] (CColor color) {
				return (color.red << 8) | (color.green << 16) | (color.blue << 24) | (color.alpha);
			};
			break;
		}
		case IPlatformBitmapPixelAccess::kABGR:
		{
			return [] (CColor color) {
				return (color.blue << 8) | (color.green << 16) | (color.red << 24) | (color.alpha);
			};
			break;
		}
		case IPlatformBitmapPixelAccess::kRGBA:
		{
			return [] (CColor color) {
				return (color.red) | (color.green << 8) | (color.blue << 16) | (color.alpha << 24);
			};
			break;
		}
		case IPlatformBitmapPixelAccess::kBGRA:
		{
			return [] (CColor color) {
				return (color.blue) | (color.green << 8) | (color.red << 16) | (color.alpha << 24);
			};
			break;
		}
	}
}

//------------------------------------------------------------------------
template <typename ReadyCallback>
inline void calculateMandelbrotBitmap (Model model, SharedPointer<CBitmap> bitmap, CPoint size,
                                       uint32_t id, const std::atomic<uint32_t>& taskID,
                                       ReadyCallback readyCallback)
{
	if (auto pa = owned (CBitmapPixelAccess::create (bitmap)))
	{
		const auto numLinesPerTask =
		    static_cast<uint32_t> (size.y / (std::thread::hardware_concurrency () * 8));

		const auto maxIterationInv = 1. / model.getIterations ();

		auto pixelAccess = shared (pa->getPlatformBitmapPixelAccess ());
		auto colorToInt32 = getColorToInt32 (pixelAccess->getPixelFormat ());
		auto counter = std::make_shared<uint32_t> (0);
		for (auto y = 0u; y < static_cast<uint32_t> (size.y); y += numLinesPerTask)
		{
			++(*counter);
			auto task = [=, &taskID] () {
				for (auto i = 0u; i < numLinesPerTask; ++i)
				{
					if (y + i >= size.y || taskID != id)
						break;
					auto pixelPtr = reinterpret_cast<uint32_t*> (
					    pixelAccess->getAddress () + (y + i) * pixelAccess->getBytesPerRow ());
					calculateLine (y + i, size, model, [&] (auto x, auto iteration) {
						auto color = calculateColor (iteration, maxIterationInv);
						*pixelPtr = colorToInt32 (color);
						pixelPtr++;
					});
				}
				Async::perform (Async::Context::Main, [readyCallback, counter, bitmap, id] () {
					if (--(*counter) == 0)
					{
						readyCallback (id, bitmap);
					}
				});
			};
			Async::perform (Async::Context::Background, std::move (task));
		}
	}
}

//------------------------------------------------------------------------
struct ViewController : DelegationController,
                        IViewListenerAdapter,
                        IModelChangeListener,
                        IScaleFactorChangedListener,
                        AtomicReferenceCounted
{
	ViewController (IController* parent, Model::Ptr model, ValuePtr progressValue)
	: DelegationController (parent), model (model), progressValue (progressValue)
	{
		model->registerListener (this);
	}
	~ViewController () noexcept override { model->unregisterListener (this); }

	CView* createView (const UIAttributes& attributes, const IUIDescription* description) override
	{
		if (auto name = attributes.getAttributeValue (IUIDescription::kCustomViewName))
		{
			if (*name == "MandelbrotView")
			{
				mandelbrotView = new View ([&] (auto box) {
					auto min =
					    pixelToPoint (model->getMax (), model->getMin (),
					                  mandelbrotView->getViewSize ().getSize (), box.getTopLeft ());
					auto max = pixelToPoint (model->getMax (), model->getMin (),
					                         mandelbrotView->getViewSize ().getSize (),
					                         box.getBottomRight ());
					model->setMinMax (min, max);
				});
				mandelbrotView->registerViewListener (this);
				return mandelbrotView;
			}
		}
		return controller->createView (attributes, description);
	}

	void viewSizeChanged (CView* view, const CRect& oldSize) override { updateMandelbrot (); }
	void viewAttached (CView* view) override
	{
		if (auto frame = view->getFrame ())
		{
			frame->registerScaleFactorChangedListeneer (this);
			scaleFactor = frame->getScaleFactor ();
			updateMandelbrot ();
		}
	}
	void viewRemoved (CView* view) override
	{
		if (auto frame = view->getFrame ())
		{
			frame->unregisterScaleFactorChangedListeneer (this);
		}
	}
	void viewWillDelete (CView* view) override
	{
		assert (mandelbrotView == view);
		++taskID; // cancel background calculation
		mandelbrotView->unregisterViewListener (this);
		mandelbrotView = nullptr;
	}

	void onScaleFactorChanged (CFrame* frame, double newScaleFactor) override
	{
		if (scaleFactor != newScaleFactor)
		{
			scaleFactor = newScaleFactor;
			updateMandelbrot ();
		}
	}

	void modelChanged (const Model& model) override { updateMandelbrot (); }

	void updateMandelbrot ()
	{
		CPoint size = mandelbrotView->getViewSize ().getSize ();
		size.x *= scaleFactor;
		size.y *= scaleFactor;
		size.makeIntegral ();
		if (size.x == 0 || size.y == 0)
			return;
		Value::performSingleEdit (*progressValue, 1.);
		auto bitmap = makeOwned<CBitmap> (size.x, size.y);
		bitmap->getPlatformBitmap ()->setScaleFactor (scaleFactor);
		auto id = ++taskID;
		auto This = shared (this);
		calculateMandelbrotBitmap (*model.get (), bitmap, size, id, taskID,
		                           [This] (uint32_t id, SharedPointer<CBitmap> bitmap) {
			                           if (id == This->taskID && This->mandelbrotView)
			                           {
				                           This->mandelbrotView->setBackground (bitmap);
				                           Value::performSingleEdit (*This->progressValue, 0.);
			                           }
			                       });
	}

	void saveBitmap (OutputStream& stream)
	{
		if (auto bitmap = mandelbrotView->getBackground ())
		{
			if (auto platformBitmap = bitmap->getPlatformBitmap ())
			{
				auto bytes = IPlatformBitmap::createMemoryPNGRepresentation (platformBitmap);
				if (!bytes.empty ())
				{
					stream.writeRaw (bytes.data (), bytes.size ());
				}
			}
		}
	}

	Model::Ptr model;
	ValuePtr progressValue;
	CView* mandelbrotView {nullptr};
	double scaleFactor {1.};
	std::atomic<uint32_t> taskID {0};
};

static const Command saveCommand {"File", "Save Bitmap"};

//------------------------------------------------------------------------
struct WindowCustomization : public UIDesc::Customization, public WindowControllerAdapter, public ICommandHandler
{
	static std::shared_ptr<WindowCustomization> make (const ValuePtr& maxIterations)
	{
		auto obj = std::make_shared<WindowCustomization>();
		obj->maxIterations = maxIterations;
		return obj;
	}

	void onSetContentView (IWindow& window, const SharedPointer<CFrame>& contentView) override
	{
		frame = contentView;
		if (!contentView)
			return;
		if (auto touchBarExt = dynamic_cast<IPlatformFrameTouchBarExtension*> (contentView->getPlatformFrame ()))
		{
			installTouchbarSupport (touchBarExt, maxIterations);
		}
	}

	bool canHandleCommand (const Command& command) override
	{
		if (frame && command == saveCommand)
			return true;
		return false;
	}
	bool handleCommand (const Command& command) override
	{
		if (frame && command == saveCommand)
		{
			if (auto fs = owned (CNewFileSelector::create (frame, CNewFileSelector::kSelectSaveFile)))
			{
				fs->addFileExtension ({"PNG File", "png", "image/png"});
				fs->run ([frame = shared (frame)] (CNewFileSelector* fs) {
					if (fs->getNumSelectedFiles () == 0)
						return;
					if (auto controller = findViewController<ViewController> (frame))
					{
						auto path = fs->getSelectedFile (0);
						assert (path != nullptr);
						CFileStream stream;
						if (!stream.open (path, CFileStream::kWriteMode | CFileStream::kBinaryMode | CFileStream::kTruncateMode))
							return;
						controller->saveBitmap (stream);
					}
				});
				return true;
			}
		}
		return false;
	}
	
	ValuePtr maxIterations;
	CFrame* frame {nullptr};
};


//------------------------------------------------------------------------
VSTGUI::Standalone::WindowPtr makeMandelbrotWindow ()
{
	IApplication::instance ().registerCommand (saveCommand, 0);

	auto model = std::make_shared<Model> ();
	auto modelBinding = ModelBinding::make (model);
	auto customization = WindowCustomization::make (modelBinding->getMaxIterationsValue ());

	customization->addCreateViewControllerFunc (
	    "mandelbrotviewcontroller", [=] (const auto& name, auto parent, const auto uiDesc) {
		    return new ViewController (parent, model, modelBinding->getProgressValue ());
		});

	UIDesc::Config config;
	config.uiDescFileName = "Window.uidesc";
	config.viewName = "Window";
	config.modelBinding = modelBinding;
	config.customization = customization;
	config.windowConfig.title = "Mandelbrot";
	config.windowConfig.autoSaveFrameName = "Mandelbrot";
	config.windowConfig.style.border ().close ().size ().centered ();
	return UIDesc::makeWindow (config);
}

//------------------------------------------------------------------------
} // Mandelbrot
