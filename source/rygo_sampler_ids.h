#pragma once

#include "pluginterfaces/vst/vsttypes.h"

namespace Steinberg::Vst::RygoSampler {

enum ParameterID : ParamID
{
	kParamAttack       = 100,
	kParamDecay        = 101,
	// kParamSustain was 102 — removed
	kParamRelease      = 103,
	kParamCutoff       = 104,
	kParamResonance    = 105,
	kParamReverbWet    = 106,
	kParamReverbSize   = 107,
	kParamPlaybackMode = 108,
	kParamGain         = 109
};

static constexpr Steinberg::FIDString kParamPlaybackModeNames[] = {
	"Gate",
	"One Shot",
	nullptr
};

extern const Steinberg::FUID ProcessorUID;
extern const Steinberg::FUID ControllerUID;

} // namespace Steinberg::Vst::RygoSampler
