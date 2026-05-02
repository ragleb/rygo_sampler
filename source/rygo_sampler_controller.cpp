#include "rygo_sampler_controller.h"
#include "rygo_sampler_ids.h"
#include "public.sdk/source/vst/vstparameters.h"
#include "pluginterfaces/base/ustring.h"
#include "base/source/fstring.h"
#include "pluginterfaces/base/funknownimpl.h"

namespace Steinberg::Vst::RygoSampler {

tresult PLUGIN_API Controller::initialize (FUnknown* context)
{
	if (EditControllerEx1::initialize (context) != kResultOk)
		return kResultFalse;

	// Parameters use plain range 0-100 so the user can type natural percentage
	// values in any host's generic UI. The processor receives normalised 0-1
	// via the standard VST3 normalisation mapping.
	auto attackParam =
	    new RangeParameter (USTRING ("Attack"), kParamAttack, USTRING ("%"), 0., 100., 0.);
	parameters.addParameter (attackParam);

	auto decayParam =
	    new RangeParameter (USTRING ("Decay"), kParamDecay, USTRING ("%"), 0., 100., 100.);
	parameters.addParameter (decayParam);

	auto releaseParam =
	    new RangeParameter (USTRING ("Release"), kParamRelease, USTRING ("%"), 0., 100., 0.);
	parameters.addParameter (releaseParam);

	auto cutoffParam =
	    new RangeParameter (USTRING ("Filter Cutoff"), kParamCutoff, USTRING ("%"), 0., 100., 100.);
	parameters.addParameter (cutoffParam);

	auto resonanceParam =
	    new RangeParameter (USTRING ("Filter Resonance"), kParamResonance, USTRING ("%"), 0., 100., 0.);
	parameters.addParameter (resonanceParam);

	auto reverbWetParam =
	    new RangeParameter (USTRING ("Reverb Wet"), kParamReverbWet, USTRING ("%"), 0., 100., 0.);
	parameters.addParameter (reverbWetParam);

	auto reverbSizeParam =
	    new RangeParameter (USTRING ("Reverb Size"), kParamReverbSize, USTRING ("%"), 0., 100., 0.);
	parameters.addParameter (reverbSizeParam);

	auto gainParam =
	    new RangeParameter (USTRING ("Gain"), kParamGain, USTRING ("dB"), -6., 6., 0.);
	parameters.addParameter (gainParam);

	auto* playbackModeParam =
	    new StringListParameter (USTRING ("Playback Mode"), kParamPlaybackMode);
	playbackModeParam->appendString (USTRING ("Gate"));
	playbackModeParam->appendString (USTRING ("One Shot"));
	playbackModeParam->setNormalized (0.);
	parameters.addParameter (playbackModeParam);

	return kResultOk;
}

IPlugView* PLUGIN_API Controller::createView (const char* name)
{
	if (Steinberg::ConstString (name) == Steinberg::Vst::ViewType::kEditor)
		return new VSTGUI::VST3Editor (this, "editor", "rygo_editor.uidesc");
	return nullptr;
}

VSTGUI::CView* Controller::verifyView (VSTGUI::CView* view,
                                        const VSTGUI::UIAttributes&,
                                        const VSTGUI::IUIDescription*,
                                        VSTGUI::VST3Editor*)
{
	if (auto* display = dynamic_cast<VSTGUI::CParamDisplay*> (view))
	{
		auto tag = display->getTag ();
		if (tag >= 0)
		{
			display->setAlphaValue (0.f);
			paramDisplays_[tag] = display;
		}
	}
	else if (auto* ctrl = dynamic_cast<VSTGUI::CControl*> (view))
	{
		if (ctrl->getTag () >= 0)
			ctrl->registerControlListener (this);
	}
	return view;
}

void Controller::willClose (VSTGUI::VST3Editor*)
{
	paramDisplays_.clear ();
}

void Controller::controlBeginEdit (VSTGUI::CControl* control)
{
	auto it = paramDisplays_.find (control->getTag ());
	if (it != paramDisplays_.end ())
	{
		it->second->setAlphaValue (1.f);
		it->second->invalid ();
	}
}

void Controller::controlEndEdit (VSTGUI::CControl* control)
{
	auto it = paramDisplays_.find (control->getTag ());
	if (it != paramDisplays_.end ())
	{
		it->second->setAlphaValue (0.f);
		it->second->invalid ();
	}
}

} // namespace Steinberg::Vst::RygoSampler
