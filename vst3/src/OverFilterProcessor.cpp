// Copyright (c) 2026 Brian R. Gunnison
// MIT License
#include "OverFilterProcessor.h"

#include "OverFilterController.h"

#include "base/source/fstreamer.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"

#include <algorithm>
#include <cmath>
#ifdef OVERFILTER_DEBUG_NAME
#include <fstream>
#endif

namespace overfilter {
using namespace Steinberg;
using namespace Steinberg::Vst;

namespace {

constexpr int32 kNoMidiPitch = -1;
constexpr int32 kNoMidiNoteId = -1;
constexpr int16 kNoMidiChannel = -1;

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
    releaseAllMidiFilters();
}

tresult PLUGIN_API OverFilterProcessor::initialize(FUnknown* context) {
    tresult result = AudioEffect::initialize(context);
    if (result != kResultOk) return result;

    addAudioInput(STR16("Main In"), SpeakerArr::kStereo);
    addAudioOutput(STR16("Main Out"), SpeakerArr::kStereo);
    addEventInput(STR16("MIDI In"), 1);
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
        releaseAllMidiFilters();
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
    paramOrder_.push_back(kParamGlobalBypass);
    paramState_[kParamGlobalBypass] = 0.0;
    paramOrder_.push_back(kParamGlobalMute);
    paramState_[kParamGlobalMute] = 0.0;
}

ParamValue OverFilterProcessor::defaultNormalized(ParamID pid) const {
    if (pid == kParamFilterSelect) return 0.0;
    if (pid == kParamWetDry) return 1.0;
    if (pid == kParamGlobalBypass || pid == kParamGlobalMute) return 0.0;
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
    if (pid == kParamGlobalBypass) {
        const bool enabled = normalizedToBool(value);
        engine_.setGlobalBypass(enabled);
        paramState_[pid] = boolToNormalized(enabled);
        return;
    }
    if (pid == kParamGlobalMute) {
        const bool enabled = normalizedToBool(value);
        engine_.setOutputMute(enabled);
        paramState_[pid] = boolToNormalized(enabled);
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
            const auto isMidiMode = [](TuningMode mode) {
                return mode == TuningMode::MidiNote || mode == TuningMode::MidiChord;
            };
            if (isMidiMode(previousMode) || isMidiMode(nextMode)) {
                releaseMidiFilter(filter);
            }
            if (previousMode != TuningMode::FixedHz && nextMode == TuningMode::FixedHz) {
                state[kSlotTune] = fixedFrequencyToNormalized(midiNoteToFrequency(normalizedToMidiNote(state[kSlotTune])));
                paramState_[filterParamId(filter, kSlotTune)] = state[kSlotTune];
            } else if (previousMode == TuningMode::FixedHz && nextMode != TuningMode::FixedHz) {
                state[kSlotTune] = midiNoteToNormalized(frequencyToNearestMidiNote(normalizedToFixedFrequency(state[kSlotTune])));
                paramState_[filterParamId(filter, kSlotTune)] = state[kSlotTune];
            }
        }
    }

    state[static_cast<size_t>(slot)] = clamp01(value);
    syncFilterConfig(filter);
}

bool OverFilterProcessor::isFilterInMidiNoteMode(int filterIndex) const {
    if (filterIndex < 0 || filterIndex >= kMaxFilters) return false;
    const auto& state = filterState_[static_cast<size_t>(filterIndex)];
    return normalizedToMode(state[kSlotMode]) == TuningMode::MidiNote;
}

bool OverFilterProcessor::isFilterInMidiChordMode(int filterIndex) const {
    if (filterIndex < 0 || filterIndex >= kMaxFilters) return false;
    const auto& state = filterState_[static_cast<size_t>(filterIndex)];
    return normalizedToMode(state[kSlotMode]) == TuningMode::MidiChord;
}

bool OverFilterProcessor::isFilterInAnyMidiMode(int filterIndex) const {
    return isFilterInMidiNoteMode(filterIndex) || isFilterInMidiChordMode(filterIndex);
}

void OverFilterProcessor::releaseMidiFilter(int filterIndex) {
    if (filterIndex < 0 || filterIndex >= kMaxFilters) return;
    midiHeldPitch_[static_cast<size_t>(filterIndex)] = kNoMidiPitch;
    midiHeldNoteId_[static_cast<size_t>(filterIndex)] = kNoMidiNoteId;
    midiHeldChannel_[static_cast<size_t>(filterIndex)] = kNoMidiChannel;
}

void OverFilterProcessor::releaseAllMidiFilters() {
    midiHeldPitch_.fill(kNoMidiPitch);
    midiHeldNoteId_.fill(kNoMidiNoteId);
    midiHeldChannel_.fill(kNoMidiChannel);
    midiNextFilter_ = 0;
}

int OverFilterProcessor::findNextFreeMidiChordFilter() const {
    for (int offset = 0; offset < kMaxFilters; ++offset) {
        const int filter = (midiNextFilter_ + offset) % kMaxFilters;
        if (!isFilterInMidiChordMode(filter)) continue;
        if (midiHeldPitch_[static_cast<size_t>(filter)] == kNoMidiPitch) return filter;
    }
    return -1;
}

int OverFilterProcessor::findMidiChordFilterForNote(int32 pitch, int32 noteId, int16 channel) const {
    if (noteId >= 0) {
        for (int filter = 0; filter < kMaxFilters; ++filter) {
            if (!isFilterInMidiChordMode(filter)) continue;
            if (midiHeldNoteId_[static_cast<size_t>(filter)] == noteId &&
                midiHeldChannel_[static_cast<size_t>(filter)] == channel) {
                return filter;
            }
        }
    }

    for (int filter = 0; filter < kMaxFilters; ++filter) {
        if (!isFilterInMidiChordMode(filter)) continue;
        if (midiHeldPitch_[static_cast<size_t>(filter)] == pitch &&
            midiHeldChannel_[static_cast<size_t>(filter)] == channel) {
            return filter;
        }
    }

    if (noteId >= 0) {
        for (int filter = 0; filter < kMaxFilters; ++filter) {
            if (!isFilterInMidiChordMode(filter)) continue;
            if (midiHeldNoteId_[static_cast<size_t>(filter)] == noteId) {
                return filter;
            }
        }
    }

    for (int filter = 0; filter < kMaxFilters; ++filter) {
        if (!isFilterInMidiChordMode(filter)) continue;
        if (midiHeldPitch_[static_cast<size_t>(filter)] == pitch) {
            return filter;
        }
    }

    return -1;
}

int OverFilterProcessor::findMidiChordFilterForNoteOff(int32 pitch, int32 noteId, int16 channel) const {
    return findMidiChordFilterForNote(pitch, noteId, channel);
}

void OverFilterProcessor::emitParameterChange(IParameterChanges* outputChanges, ParamID pid,
                                              ParamValue value, int32 sampleOffset) {
    if (!outputChanges) return;
    int32 queueIndex = 0;
    IParamValueQueue* queue = outputChanges->addParameterData(pid, queueIndex);
    if (!queue) return;
    int32 pointIndex = 0;
    queue->addPoint(sampleOffset, value, pointIndex);
}

int OverFilterProcessor::selectedFilterIndex() const {
    const auto it = paramState_.find(kParamFilterSelect);
    const double value = it == paramState_.end() ? 0.0 : it->second;
    return std::clamp(static_cast<int>(std::round(clamp01(value) * (kMaxFilters - 1))), 0, kMaxFilters - 1);
}

void OverFilterProcessor::emitTuneChangeForFilter(int filterIndex, ParamValue value, int32 sampleOffset,
                                                  IParameterChanges* outputChanges) {
    emitParameterChange(outputChanges, filterParamId(filterIndex, kSlotTune), value, sampleOffset);
    if (filterIndex == selectedFilterIndex()) {
        emitParameterChange(outputChanges, activeParamId(kActiveTune), value, sampleOffset);
    }
}

void OverFilterProcessor::debugLogMidiEvent(const char* label, int32 pitch, int32 noteId,
                                            int16 channel, int assignedFilter) const {
#ifdef OVERFILTER_DEBUG_NAME
    std::ofstream log("C:\\projects\\ableplugs\\OverFilter\\overfilter_midi_debug.log", std::ios::app);
    log << label
        << " pitch=" << pitch
        << " noteId=" << noteId
        << " channel=" << channel
        << " assigned=" << assignedFilter
        << " next=" << midiNextFilter_;
    for (int filter = 0; filter < kMaxFilters; ++filter) {
        if (!isFilterInAnyMidiMode(filter)) continue;
        const TuningMode mode = normalizedToMode(filterState_[static_cast<size_t>(filter)][kSlotMode]);
        log << " f" << (filter + 1)
            << ":mode=" << (mode == TuningMode::MidiChord ? "chord" : "note")
            << ":held=" << midiHeldPitch_[static_cast<size_t>(filter)]
            << "/id=" << midiHeldNoteId_[static_cast<size_t>(filter)]
            << "/ch=" << midiHeldChannel_[static_cast<size_t>(filter)]
            << "/note=" << normalizedToMidiNote(filterState_[static_cast<size_t>(filter)][kSlotTune]);
    }
    log << '\n';
#else
    (void)label;
    (void)pitch;
    (void)noteId;
    (void)channel;
    (void)assignedFilter;
#endif
}

void OverFilterProcessor::handleNoteOn(int32 pitch, int32 noteId, int16 channel, int32 sampleOffset,
                                       IParameterChanges* outputChanges) {
    bool retunedAny = false;
    int assignedChordFilter = -1;
    const int safePitch = std::clamp(pitch, 0, 127);
    const double normalizedNote = midiNoteToNormalized(safePitch);

    for (int filter = 0; filter < kMaxFilters; ++filter) {
        if (!isFilterInMidiNoteMode(filter)) continue;

        auto& state = filterState_[static_cast<size_t>(filter)];
        state[kSlotTune] = normalizedNote;
        const ParamID tunePid = filterParamId(filter, kSlotTune);
        paramState_[tunePid] = normalizedNote;
        syncFilterConfig(filter);
        emitTuneChangeForFilter(filter, normalizedNote, sampleOffset, outputChanges);
        retunedAny = true;
    }

    const bool alreadyCaptured = findMidiChordFilterForNote(safePitch, noteId, channel) >= 0;
    if (!alreadyCaptured) {
        assignedChordFilter = findNextFreeMidiChordFilter();
        if (assignedChordFilter >= 0) {
            auto& state = filterState_[static_cast<size_t>(assignedChordFilter)];
            state[kSlotTune] = normalizedNote;
            const ParamID tunePid = filterParamId(assignedChordFilter, kSlotTune);
            paramState_[tunePid] = normalizedNote;
            midiHeldPitch_[static_cast<size_t>(assignedChordFilter)] = safePitch;
            midiHeldNoteId_[static_cast<size_t>(assignedChordFilter)] = noteId;
            midiHeldChannel_[static_cast<size_t>(assignedChordFilter)] = channel;
            midiNextFilter_ = (assignedChordFilter + 1) % kMaxFilters;
            syncFilterConfig(assignedChordFilter);
            emitTuneChangeForFilter(assignedChordFilter, normalizedNote, sampleOffset, outputChanges);
            retunedAny = true;
        }
    }

    if (!retunedAny) {
        debugLogMidiEvent("note_on_no_midi_filter", pitch, noteId, channel, -1);
        return;
    }

    debugLogMidiEvent(alreadyCaptured ? "note_on_chord_already_held" : "note_on", pitch, noteId, channel,
                      assignedChordFilter);
}

void OverFilterProcessor::handleNoteOff(int32 pitch, int32 noteId, int16 channel) {
    const int filter = findMidiChordFilterForNoteOff(std::clamp(pitch, 0, 127), noteId, channel);
    if (filter >= 0) {
        releaseMidiFilter(filter);
        midiNextFilter_ = filter;
    }
    debugLogMidiEvent("note_off", pitch, noteId, channel, filter);
}

void OverFilterProcessor::handleEvent(const Event& event, IParameterChanges* outputChanges) {
    switch (event.type) {
        case Event::kNoteOnEvent:
            if (event.noteOn.velocity <= 0.0f) {
                handleNoteOff(event.noteOn.pitch, event.noteOn.noteId, event.noteOn.channel);
            } else {
                handleNoteOn(event.noteOn.pitch, event.noteOn.noteId, event.noteOn.channel,
                             event.sampleOffset, outputChanges);
            }
            break;
        case Event::kNoteOffEvent:
            handleNoteOff(event.noteOff.pitch, event.noteOff.noteId, event.noteOff.channel);
            break;
        default:
            break;
    }
}

void OverFilterProcessor::handleAllEvents(IEventList* events, IParameterChanges* outputChanges) {
    if (!events) return;
    const int32 eventCount = events->getEventCount();
    Event event{};
    for (int32 i = 0; i < eventCount; ++i) {
        if (events->getEvent(i, event) == kResultOk) {
            handleEvent(event, outputChanges);
        }
    }
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
    if (data.numOutputs == 0 || !data.outputs) {
        handleAllEvents(data.inputEvents, data.outputParameterChanges);
        return kResultOk;
    }
    for (int32 bus = 0; bus < data.numOutputs; ++bus) {
        data.outputs[bus].silenceFlags = 0;
    }

    handleParameterChanges(data);
    if (data.numSamples <= 0) {
        handleAllEvents(data.inputEvents, data.outputParameterChanges);
        return kResultOk;
    }

    auto* mainIn = data.numInputs > 0 ? &data.inputs[0] : nullptr;
    if (!mainIn || mainIn->numChannels == 0) {
        handleAllEvents(data.inputEvents, data.outputParameterChanges);
        writeOutputSilence(data);
        return kResultOk;
    }

    auto& outBus = data.outputs[0];
    if (outBus.numChannels != mainIn->numChannels) {
        handleAllEvents(data.inputEvents, data.outputParameterChanges);
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
void OverFilterProcessor::processAudio(ProcessData& data, SampleType** inputs, SampleType** outputs,
                                       int32_t numChannels, int32_t numSamples) {
    const int32 eventCount = data.inputEvents ? data.inputEvents->getEventCount() : 0;
    int32 nextEvent = 0;
    int32 sampleIndex = 0;
    Event event{};
    std::array<SampleType*, 2> inputChunk{};
    std::array<SampleType*, 2> outputChunk{};

    auto processRange = [&](int32 start, int32 count) {
        if (count <= 0) return;
        for (int32 channel = 0; channel < numChannels; ++channel) {
            inputChunk[static_cast<size_t>(channel)] = inputs[channel] + start;
            outputChunk[static_cast<size_t>(channel)] = outputs[channel] + start;
        }
        engine_.processBlock(inputChunk.data(), outputChunk.data(), numChannels, count);
    };

    while (nextEvent < eventCount) {
        if (data.inputEvents->getEvent(nextEvent, event) != kResultOk) {
            ++nextEvent;
            continue;
        }
        const int32 offset = std::clamp(event.sampleOffset, sampleIndex, numSamples);
        processRange(sampleIndex, offset - sampleIndex);
        sampleIndex = offset;
        handleEvent(event, data.outputParameterChanges);
        ++nextEvent;
    }

    processRange(sampleIndex, numSamples - sampleIndex);
}

tresult PLUGIN_API OverFilterProcessor::setState(IBStream* state) {
    if (!state) return kResultFalse;
    IBStreamer streamer(state, kLittleEndian);
    releaseAllMidiFilters();
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
