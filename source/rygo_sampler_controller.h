#pragma once

#include "public.sdk/source/vst/vsteditcontroller.h"
#include "vstgui/plugin-bindings/vst3editor.h"
#include "vstgui/lib/controls/icontrollistener.h"
#include "vstgui/lib/controls/cparamdisplay.h"
#include <map>

namespace Steinberg::Vst::RygoSampler {

class Controller : public Steinberg::Vst::EditControllerEx1,
                   public VSTGUI::VST3EditorDelegate,
                   public VSTGUI::IControlListener
{
public:
	Controller () = default;

	static FUnknown* createInstance (void*)
	{
		return (IEditController*)new Controller ();
	}

	tresult PLUGIN_API initialize (FUnknown* context) SMTG_OVERRIDE;

	IPlugView* PLUGIN_API createView (const char* name) SMTG_OVERRIDE;

	// VST3EditorDelegate — intercept each view to set up show-while-editing
	VSTGUI::CView* verifyView (VSTGUI::CView* view, const VSTGUI::UIAttributes&,
	                           const VSTGUI::IUIDescription*, VSTGUI::VST3Editor*) override;
	void willClose (VSTGUI::VST3Editor*) override;

	// IControlListener (registered as sub-listener on each knob)
	void valueChanged     (VSTGUI::CControl*) override {}
	void controlBeginEdit (VSTGUI::CControl* control) override;
	void controlEndEdit   (VSTGUI::CControl* control) override;

private:
	std::map<int32_t, VSTGUI::CParamDisplay*> paramDisplays_;
};

} // namespace Steinberg::Vst::RygoSampler
