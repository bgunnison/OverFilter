// Copyright (c) 2026 Brian R. Gunnison
// MIT License
#include "OverFilterProcessor.h"

#include "OverFilterController.h"

#include "base/source/fstreamer.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"

#include <algorithm>

namespace overfilter {
using namespace Steinberg;
using namespace Steinberg::Vst;

namespace {

void writeOutputSilence(ProcessData& data) {
    for (int32 bus = 0; bus < data.numOutputs; ++bus) {
        auto& out = data.outputs[bus];
        out.silenceFlags = 0;
        if (out.channelBuffers32 && data.symbolicSampleSize == kSample32) {
            for (uint32 c = 0; c < out.numChannels; ++c) {
                std::fill_n(out.channelBuffers32[c], data.numSamples, 0.0f);
            }
        }
        if (out.channelBuffers64 && data.symbolicSampleSize == kSample64) {
            for (uint32 c = 0; c < out.numChannels; ++c) {
                std::fill_n(out.channelBuffers64[c], data.numSamples, 0.0);
            }
        }
    }
}

} // namespace

OverFilterProcessor::OverFilterProcessor() {
    setControllerClass(kOverFilterControllerUID);
    setProcessing(true);
    buildParamOrder();
}

tresult PLUGIN_API OverFilterProcessor::initialize(FUnknown* context) {
    tresult result = AudioEffect::initialize(context);
    if (result != kResultOk) return result;

    addAudioInput(STR16("Main In"), SpeakerArr::kStereo);
    addAudioOutput(STR16("Main Out"), SpeakerArr::kStereo);
    return kResultOk;
}

tresult PLUGIN_API OverFilterProcessor::terminate() {
    return AudioEffect::terminate();
}

tresult PLUGIN_API OverFilterProcessor::setBusArrangements(SpeakerArrangement* inputs, int32 numIns,
                                                           SpeakerArrangement* outputs, int32 numOuts) {
    if (numIns != 1 || numOuts != 1) return kResultFalse;
    if (inputs[0] != SpeakerArr::kMono && inputs[0] != SpeakerArr::kStereo) return kResultFalse;
    if (outputs[0] != inputs[0]) return kResultFalse;
    return AudioEffect::setBusArrangements(inputs, numIns, outputs, numOuts);
}

tresult PLUGIN_API OverFilterProcessor::setupProcessing(ProcessSetup& setup) {
    sampleRate_ = setup.sampleRate > 0.0 ? setup.sampleRate : 44100.0;
    maxBlockSize_ = setup.maxSamplesPerBlock;
    engine_.prepare(sampleRate_, maxBlockSize_);
    syncAllFilters();
    return AudioEffect::setupProcessing(setup);
}

tresult PLUGIN_API OverFilterProcessor::setActive(TBool state) {
    if (state) {
        engine_.reset();
    }
    return AudioEffect::setActive(state);
}

tresult PLUGIN_API OverFilterProcessor::canProcessSampleSize(int32 symbolicSampleSize) {
    if (symbolicSampleSize == kSample32 || symbolicSampleSize == kSample64) return kResultOk;
    return kResultFalse;
}

void OverFilterProcessor::buildParamOrder() {
    paramOrder_.clear();
    paramOrder_.push_back(kParamFilterSelect);
    for (int filter = 0; filter < kMaxFilters; ++filter) {
        for (int slot = 0; slot < kPerFilterParams; ++slot) {
            const ParamID pid = filterParamId(filter, slot);
            paramOrder_.push_back(pid);
            const double value = overfilter::defaultNormalized(filter, slot);
            filterState_[static_cast<size_t>(filter)][static_cast<size_t>(slot)] = value;
            paramState_[pid] = value;
        }
    }
    paramState_[kParamFilterSelect] = 0.0;
    paramOrder_.push_back(kParamWetDry);
    paramState_[kParamWetDry] = 1.0;
}

ParamValue OverFilterProcessor::defaultNormalized(ParamID pid) const {
    if (pid == kParamFilterSelect) return 0.0;
    if (pid == kParamWetDry) return 1.0;
    if (pid >= kParamBaseFilterParams && pid < kActiveParamBase) {
        const int rel = static_cast<int>(pid - kParamBaseFilterParams);
        const int filter = rel / kPerFilterParams;
        const int slot = rel % kPerFilterParams;
        return overfilter::defaultNormalized(filter, slot);
    }
    if (pid >= kActiveParamBase && pid < kActiveParamBase + kActiveParamCount) {
        const int filterSlot = activeSlotToFilterSlot(static_cast<int>(pid - kActiveParamBase));
        return overfilter::defaultNormalized(0, filterSlot);
    }
    return 0.0;
}

void OverFilterProcessor::handleParameterChanges(ProcessData& data) {
    if (!data.inputParameterChanges) return;
    const int32 count = data.inputParameterChanges->getParameterCount();
    for (int32 i = 0; i < count; ++i) {
        IParamValueQueue* queue = data.inputParameterChanges->getParameterData(i);
        if (!queue) continue;
        const int32 points = queue->getPointCount();
        if (points <= 0) continue;
        ParamValue value = 0.0;
        int32 sampleOffset = 0;
        queue->getPoint(points - 1, sampleOffset, value);
        applyNormalizedParam(queue->getParameterId(), value);
    }
}

void OverFilterProcessor::applyNormalizedParam(ParamID pid, ParamValue value) {
    paramState_[pid] = value;
    if (pid == kParamFilterSelect) return;
    if (pid == kParamWetDry) {
        engine_.setWetDry(clamp01(value));
        paramState_[pid] = clamp01(value);
        return;
    }
    if (pid >= kActiveParamBase && pid < kActiveParamBase + kActiveParamCount) return;
    if (pid < kParamBaseFilterParams || pid >= kActiveParamBase) return;

    const int rel = static_cast<int>(pid - kParamBaseFilterParams);
    const int filter = rel / kPerFilterParams;
    const int slot = rel % kPerFilterParams;
    if (filter < 0 || filter >= kMaxFilters || slot < 0 || slot >= kPerFilterParams) return;

    auto& state = filterState_[static_cast<size_t>(filter)];
    if (slot == kSlotMode) {
        const TuningMode previousMode = normalizedToMode(state[kSlotMode]);
        const TuningMode nextMode = normalizedToMode(value);
        if (previousMode != nextMode) {
            if (previousMode == TuningMode::Note && nextMode == TuningMode::FixedHz) {
                state[kSlotTune] = fixedFrequencyToNormalized(midiNoteToFrequency(normalizedToMidiNote(state[kSlotTune])));
                paramState_[filterParamId(filter, kSlotTune)] = state[kSlotTune];
            } else if (previousMode == TuningMode::FixedHz && nextMode == TuningMode::Note) {
                state[kSlotTune] = midiNoteToNormalized(frequencyToNearestMidiNote(normalizedToFixedFrequency(state[kSlotTune])));
                paramState_[filterParamId(filter, kSlotTune)] = state[kSlotTune];
            }
        }
    }

    state[static_cast<size_t>(slot)] = clamp01(value);
    syncFilterConfig(filter);
}

void OverFilterProcessor::syncFilterConfig(int filterIndex) {
    if (filterIndex < 0 || filterIndex >= kMaxFilters) return;
    const auto& state = filterState_[static_cast<size_t>(filterIndex)];
    FilterRuntimeParams params{};
    params.frequencyHz = normalizedToFrequency(state[kSlotMode], state[kSlotTune]);
    params.gainDb = normalizedToGainDb(state[kSlotGain]);
    params.q = normalizedToQ(state[kSlotQ]);
    params.feedback = normalizedToFeedback(state[kSlotFeedback]);
    params.positivePolarity = true;
    params.spreadMs = normalizedToSpreadMs(state[kSlotSpread]);
    params.bypass = normalizedToBool(state[kSlotBypass]);
    params.feedbackEnabled = normalizedToBool(state[kSlotFeedbackEnable]);
    engine_.setFilterParams(filterIndex, params);
}

void OverFilterProcessor::syncAllFilters() {
    for (int filter = 0; filter < kMaxFilters; ++filter) {
        syncFilterConfig(filter);
    }
}

tresult PLUGIN_API OverFilterProcessor::process(ProcessData& data) {
    if (data.numOutputs == 0 || !data.outputs) return kResultOk;
    for (int32 bus = 0; bus < data.numOutputs; ++bus) {
        data.outputs[bus].silenceFlags = 0;
    }

    handleParameterChanges(data);
    if (data.numSamples <= 0) return kResultOk;

    auto* mainIn = data.numInputs > 0 ? &data.inputs[0] : nullptr;
    if (!mainIn || mainIn->numChannels == 0) {
        writeOutputSilence(data);
        return kResultOk;
    }

    auto& outBus = data.outputs[0];
    if (outBus.numChannels != mainIn->numChannels) {
        writeOutputSilence(data);
        return kResultOk;
    }

    const int32 numChannels = static_cast<int32>(outBus.numChannels);
    if (data.symbolicSampleSize == kSample32) {
        processAudio<float>(data, mainIn->channelBuffers32, outBus.channelBuffers32, numChannels, data.numSamples);
    } else if (data.symbolicSampleSize == kSample64) {
        processAudio<double>(data, mainIn->channelBuffers64, outBus.channelBuffers64, numChannels, data.numSamples);
    }

    return kResultOk;
}

template <typename SampleType>
void OverFilterProcessor::processAudio(ProcessData&, SampleType** inputs, SampleType** outputs,
                                       int32_t numChannels, int32_t numSamples) {
    engine_.processBlock(inputs, outputs, numChannels, numSamples);
}

tresult PLUGIN_API OverFilterProcessor::setState(IBStream* state) {
    if (!state) return kResultFalse;
    IBStreamer streamer(state, kLittleEndian);
    for (auto pid : paramOrder_) {
        double value = 0.0;
        if (!streamer.readDouble(value)) value = defaultNormalized(pid);
        applyNormalizedParam(pid, value);
    }
    syncAllFilters();
    return kResultOk;
}

tresult PLUGIN_API OverFilterProcessor::getState(IBStream* state) {
    if (!state) return kResultFalse;
    IBStreamer streamer(state, kLittleEndian);
    for (auto pid : paramOrder_) {
        double value = defaultNormalized(pid);
        const auto it = paramState_.find(pid);
        if (it != paramState_.end()) value = it->second;
        streamer.writeDouble(value);
    }
    return kResultOk;
}

} // namespace overfilter
