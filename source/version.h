#pragma once

#include "pluginterfaces/base/fplatform.h"

// Version 1.0.1.0
#define MAJOR_VERSION_INT 1
#define SUB_VERSION_INT 0
#define RELEASE_NUMBER_INT 1
#define BUILD_NUMBER_INT 0

#define MAJOR_VERSION_STR "1"
#define SUB_VERSION_STR "0"
#define RELEASE_NUMBER_STR "1"
#define BUILD_NUMBER_STR "0"

#define FULL_VERSION_STR MAJOR_VERSION_STR "." SUB_VERSION_STR "." RELEASE_NUMBER_STR "." BUILD_NUMBER_STR

#define stringOriginalFilename "rygo_sampler.vst3"
#if SMTG_PLATFORM_64
#define stringFileDescription "Rygo Sampler VST3 (64Bit)"
#else
#define stringFileDescription "Rygo Sampler VST3"
#endif
#define stringCompanyName "ZHRI Records\0"
#define stringCompanyWeb "https://example.com/\0"
#define stringCompanyEmail "info@example.com\0"
#define stringLegalCopyright "\251 2026 ZHRI Records\0"
#define stringLegalTrademarks "VST is a trademark of Steinberg Media Technologies GmbH\0"
