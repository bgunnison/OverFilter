// Copyright (c) 2026 Brian R. Gunnison
// MIT License
#pragma once

#include "OverFilterEngine.h"
#include "OverFilterIDs.h"

#include "public.sdk/source/vst/vstaudioeffect.h"

#include <array>
#include <unordered_map>
#include <vector>

namespace overfilter {

class OverFilterProcessor : public Steinberg::Vst::AudioEffect {
public:
    OverFilterProcessor();

    static Steinberg::FUnknown* createInstance(void*) {
        return static_cast<Steinberg::Vst::IAudioProcessor*>(new OverFilterProcessor());
    }

    Steinberg::tresult PLUGIN_API initialize(Steinberg::FUnknown* context) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API terminate() SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API setState(Steinberg::IBStream* state) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API getState(Steinberg::IBStream* state) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API setBusArrangements(Steinberg::Vst::SpeakerArrangement* inputs, Steinberg::int32 numIns,
                                                     Steinberg::Vst::SpeakerArrangement* outputs, Steinberg::int32 numOuts) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API process(Steinberg::Vst::ProcessData& data) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API setupProcessing(Steinberg::Vst::ProcessSetup& setup) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API setActive(Steinberg::TBool state) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API canProcessSampleSize(Steinberg::int32 symbolicSampleSize) SMTG_OVERRIDE;

private:
    void buildParamOrder();
    void handleParameterChanges(Steinberg::Vst::ProcessData& data);
    void applyNormalizedParam(Steinberg::Vst::ParamID pid, Steinberg::Vst::ParamValue value);
    void syncFilterConfig(int filterIndex);
    void syncAllFilters();
    Steinberg::Vst::ParamValue defaultNormalized(Steinberg::Vst::ParamID pid) const;

    template <typename SampleType>
    void processAudio(Steinberg::Vst::ProcessData& data, SampleType** inputs, SampleType** outputs,
                      int32_t numChannels, int32_t numSamples);

    std::array<std::array<double, kPerFilterParams>, kMaxFilters> filterState_{};
    std::vector<Steinberg::Vst::ParamID> paramOrder_{};
    std::unordered_map<Steinberg::Vst::ParamID, double> paramState_{};
    OverFilterEngine engine_{};
    double sampleRate_{44100.0};
    int32_t maxBlockSize_{0};
};

} // namespace overfilter
