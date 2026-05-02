#include "rygo_sampler_controller.h"
#include "rygo_sampler_ids.h"
#include "rygo_sampler_processor.h"
#include "version.h"

#include "public.sdk/source/main/pluginfactory.h"

namespace Steinberg::Vst::RygoSampler {

static const char* const kCompanyName = "ZHRI Records";
static const char* const kCompanyWeb = "https://example.com";
static const char* const kCompanyEmail = "info@example.com";

static const char* const kPluginName = "Rygo Sampler";
static const char* const kPluginCategory = "Instrument|Sampler";
static const char* const kControllerName = "Rygo Sampler Controller";

} // namespace Steinberg::Vst::RygoSampler

using namespace Steinberg::Vst::RygoSampler;

BEGIN_FACTORY_DEF (kCompanyName, kCompanyWeb, kCompanyEmail)

	DEF_CLASS2 (INLINE_UID_FROM_FUID (ProcessorUID),
	            PClassInfo::kManyInstances,
	            kVstAudioEffectClass,
	            kPluginName,
	            Vst::kDistributable,
	            kPluginCategory,
	            FULL_VERSION_STR,
	            kVstVersionString,
	            Processor::createInstance)

    DEF_CLASS2 (INLINE_UID_FROM_FUID (ControllerUID),
                PClassInfo::kManyInstances,
                kVstComponentControllerClass,
                kControllerName,
                0,
                "",
	            FULL_VERSION_STR,
	            kVstVersionString,
	            Controller::createInstance)

END_FACTORY
