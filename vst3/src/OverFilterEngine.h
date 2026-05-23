// Copyright (c) 2026 Brian R. Gunnison
// MIT License
#pragma once

#include "OverFilterParams.h"
#include "dsp/FractionalDelay.h"
#include "dsp/PeakLimiter.h"
#include "dsp/TwoPoleResonator.h"

#include <array>
#include <cstdint>

namespace overfilter {

struct FilterRuntimeParams {
    double frequencyHz{440.0};
    double gainDb{0.0};
    double q{7.2};
    double feedback{0.0};
    double spreadMs{0.0};
    bool positivePolarity{true};
    bool bypass{false};
    bool feedbackEnabled{false};
};

class OverFilterEngine {
public:
    void prepare(double sampleRate, int32_t maxBlockSize);
    void reset();
    void setFilterParams(int filterIndex, const FilterRuntimeParams& params);
    void setWetDry(double wetDry);

    template <typename SampleType>
    void processBlock(SampleType** inputs, SampleType** outputs, int32_t numChannels, int32_t numSamples) {
        if (!inputs || !outputs || numChannels <= 0 || numSamples <= 0) return;
        for (int32_t sample = 0; sample < numSamples; ++sample) {
            const double inL = static_cast<double>(inputs[0][sample]);
            const double inR = numChannels > 1 ? static_cast<double>(inputs[1][sample]) : inL;
            double outL = 0.0;
            double outR = 0.0;
            processSample(inL, inR, outL, outR);
            outputs[0][sample] = static_cast<SampleType>(outL);
            if (numChannels > 1) {
                outputs[1][sample] = static_cast<SampleType>(outR);
            }
        }
    }

private:
    void processSample(double inL, double inR, double& outL, double& outR);
    static double softClip(double x);
    double feedbackSeed(double maxFeedback);

    double sampleRate_{44100.0};
    std::array<FilterRuntimeParams, kMaxFilters> params_{};
    std::array<std::array<dsp::TwoPoleResonator, 2>, kMaxFilters> resonators_{};
    std::array<dsp::FractionalDelay, kMaxFilters> rightSpreadDelay_{};
    dsp::PeakLimiter limiterL_{};
    dsp::PeakLimiter limiterR_{};
    double feedbackL_{0.0};
    double feedbackR_{0.0};
    double wetDryTarget_{1.0};
    double wetDryCurrent_{1.0};
    double wetDrySmoothingCoeff_{1.0};
    uint32_t noiseState_{0x12345678u};
};

} // namespace overfilter
