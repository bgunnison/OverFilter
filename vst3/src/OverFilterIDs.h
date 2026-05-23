// Copyright (c) 2026 Brian R. Gunnison
// MIT License
#pragma once

#include "OverFilterParams.h"

#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/vst/vsttypes.h"

namespace overfilter {

#ifdef OVERFILTER_DEBUG_UIDS
// {27A1BEF2-A7F3-4DC7-AE84-15CDC8D48745}
static const Steinberg::FUID kOverFilterProcessorUID(0x27A1BEF2, 0xA7F34DC7, 0xAE8415CD, 0xC8D48745);
// {7329A133-D9CA-48E6-8557-7767FC47C70F}
static const Steinberg::FUID kOverFilterControllerUID(0x7329A133, 0xD9CA48E6, 0x85577767, 0xFC47C70F);
#else
// {4E908FDD-9272-4674-AB72-3C5FC4B8118B}
static const Steinberg::FUID kOverFilterProcessorUID(0x4E908FDD, 0x92724674, 0xAB723C5F, 0xC4B8118B);
// {7C3096D2-A8A1-4BBF-B2C9-248E533F29B2}
static const Steinberg::FUID kOverFilterControllerUID(0x7C3096D2, 0xA8A14BBF, 0xB2C9248E, 0x533F29B2);
#endif

#ifdef OVERFILTER_DEBUG_NAME
constexpr Steinberg::FIDString kOverFilterVst3Name = "Debug OverFilter";
#else
constexpr Steinberg::FIDString kOverFilterVst3Name = "OverFilter";
#endif
constexpr Steinberg::FIDString kOverFilterVst3Vendor = "VirtualRobot";
constexpr Steinberg::FIDString kOverFilterVst3Version = "0.1.0 (" __DATE__ " " __TIME__ ")";
constexpr const char kOverFilterVst3Build[] = __DATE__ " " __TIME__;
constexpr Steinberg::FIDString kOverFilterVst3Url = "https://ableplugs.local/overfilter";
constexpr Steinberg::FIDString kOverFilterVst3Email = "support@ableplugs.local";

enum ParamIDs : Steinberg::Vst::ParamID {
    kParamFilterSelect = 0,
    kParamBaseFilterParams,
};

constexpr int kActiveParamBase = kParamBaseFilterParams + kMaxFilters * kPerFilterParams;
constexpr Steinberg::Vst::ParamID kParamWetDry = static_cast<Steinberg::Vst::ParamID>(kActiveParamBase + kActiveParamCount);

inline Steinberg::Vst::ParamID filterParamId(int filterIndex, int paramSlot) {
    return static_cast<Steinberg::Vst::ParamID>(kParamBaseFilterParams + filterIndex * kPerFilterParams + paramSlot);
}

inline Steinberg::Vst::ParamID activeParamId(int activeSlot) {
    return static_cast<Steinberg::Vst::ParamID>(kActiveParamBase + activeSlot);
}

} // namespace overfilter
