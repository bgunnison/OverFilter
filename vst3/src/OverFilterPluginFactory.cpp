// Copyright (c) 2026 Brian R. Gunnison
// MIT License
#include "OverFilterController.h"
#include "OverFilterIDs.h"
#include "OverFilterProcessor.h"

#include "public.sdk/source/main/pluginfactory.h"

#ifdef OVERFILTER_DEBUG_NAME
#define stringPluginName "Debug OverFilter"
#else
#define stringPluginName "OverFilter"
#endif

using namespace overfilter;
using namespace Steinberg;
using namespace Steinberg::Vst;

BEGIN_FACTORY_DEF(kOverFilterVst3Vendor, kOverFilterVst3Url, kOverFilterVst3Email)

    DEF_CLASS2(INLINE_UID_FROM_FUID(kOverFilterProcessorUID),
               PClassInfo::kManyInstances,
               kVstAudioEffectClass,
               stringPluginName,
               Vst::kDistributable,
               PlugType::kFx,
               kOverFilterVst3Version,
               kVstVersionString,
               OverFilterProcessor::createInstance)

    DEF_CLASS2(INLINE_UID_FROM_FUID(kOverFilterControllerUID),
               PClassInfo::kManyInstances,
               kVstComponentControllerClass,
               stringPluginName " Controller",
               0,
               "",
               kOverFilterVst3Version,
               kVstVersionString,
               OverFilterController::createInstance)

END_FACTORY
