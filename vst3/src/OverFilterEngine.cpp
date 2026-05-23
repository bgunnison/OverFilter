// Copyright (c) 2026 Brian R. Gunnison
// MIT License
#include "OverFilterEngine.h"

#include <algorithm>
#include <cmath>

namespace overfilter {

void OverFilterEngine::prepare(double sampleRate, int32_t) {
    sampleRate_ = sampleRate > 0.0 ? sampleRate : 44100.0;
    wetDrySmoothingCoeff_ = 1.0 - std::exp(-1.0 / (0.02 * sampleRate_));
    limiterL_.setSampleRate(sampleRate_);
    limiterR_.setSampleRate(sampleRate_);
    limiterL_.setThreshold(0.86);
    limiterR_.setThreshold(0.86);
    limiterL_.setReleaseMs(45.0);
    limiterR_.setReleaseMs(45.0);

    for (int filter = 0; filter < kMaxFilters; ++filter) {
        rightSpreadDelay_[static_cast<size_t>(filter)].prepare(sampleRate_, kMaxSpreadMs);
        for (int channel = 0; channel < 2; ++channel) {
            auto& resonator = resonators_[static_cast<size_t>(filter)][static_cast<size_t>(channel)];
            resonator.setSampleRate(sampleRate_);
            resonator.setFrequency(params_[static_cast<size_t>(filter)].frequencyHz);
            resonator.setQ(params_[static_cast<size_t>(filter)].q);
            resonator.setFeedback(params_[static_cast<size_t>(filter)].feedbackEnabled
                ? params_[static_cast<size_t>(filter)].feedback
                : 0.0);
            resonator.reset();
        }
    }
    reset();
}

void OverFilterEngine::reset() {
    feedbackL_ = 0.0;
    feedbackR_ = 0.0;
    wetDryCurrent_ = wetDryTarget_;
    noiseState_ = 0x12345678u;
    limiterL_.reset();
    limiterR_.reset();
    for (int filter = 0; filter < kMaxFilters; ++filter) {
        rightSpreadDelay_[static_cast<size_t>(filter)].reset();
        for (int channel = 0; channel < 2; ++channel) {
            resonators_[static_cast<size_t>(filter)][static_cast<size_t>(channel)].reset();
        }
    }
}

void OverFilterEngine::setFilterParams(int filterIndex, const FilterRuntimeParams& params) {
    if (filterIndex < 0 || filterIndex >= kMaxFilters) return;
    auto& dst = params_[static_cast<size_t>(filterIndex)];
    dst = params;
    dst.frequencyHz = std::clamp(dst.frequencyHz, kMinFrequencyHz, std::max(kMinFrequencyHz, 0.49 * sampleRate_));
    dst.gainDb = std::clamp(dst.gainDb, kMinGainDb, kMaxGainDb);
    dst.q = std::clamp(dst.q, kMinQ, kMaxQ);
    dst.feedback = std::clamp(dst.feedback, 0.0, 1.0);
    dst.spreadMs = std::clamp(dst.spreadMs, 0.0, kMaxSpreadMs);

    const double leftFrequency = dst.frequencyHz;
    const double rightFrequency = dst.frequencyHz;
    const double resonanceNarrowing = dst.feedbackEnabled ? std::pow(dst.feedback, 2.0) : 0.0;
    const double effectiveQ = std::clamp(dst.q * (1.0 + 1.75 * resonanceNarrowing), kMinQ, kMaxQ);
    auto& left = resonators_[static_cast<size_t>(filterIndex)][0];
    auto& right = resonators_[static_cast<size_t>(filterIndex)][1];
    left.setFrequency(leftFrequency);
    left.setQ(effectiveQ);
    left.setFeedback(dst.feedbackEnabled ? dst.feedback : 0.0);
    right.setFrequency(rightFrequency);
    right.setQ(effectiveQ);
    right.setFeedback(dst.feedbackEnabled ? dst.feedback : 0.0);
}

void OverFilterEngine::setWetDry(double wetDry) {
    wetDryTarget_ = std::clamp(wetDry, 0.0, 1.0);
}

double OverFilterEngine::softClip(double x) {
    if (!std::isfinite(x)) return 0.0;
    return std::tanh(std::clamp(x, -8.0, 8.0));
}

double OverFilterEngine::feedbackSeed(double maxFeedback) {
    if (maxFeedback <= 0.65) return 0.0;
    noiseState_ ^= noiseState_ << 13;
    noiseState_ ^= noiseState_ >> 17;
    noiseState_ ^= noiseState_ << 5;
    const double bipolar = (static_cast<double>(noiseState_ & 0x00ffffffu) / 8388607.5) - 1.0;
    const double amount = std::clamp((maxFeedback - 0.65) / 0.35, 0.0, 1.0);
    return bipolar * 1.0e-6 * amount;
}

void OverFilterEngine::processSample(double inL, double inR, double& outL, double& outR) {
    const double inputL = std::isfinite(inL) ? std::clamp(inL, -8.0, 8.0) : 0.0;
    const double inputR = std::isfinite(inR) ? std::clamp(inR, -8.0, 8.0) : 0.0;

    int feedbackFilters = 0;
    double maxFeedback = 0.0;
    for (const auto& p : params_) {
        if (p.bypass || !p.feedbackEnabled) continue;
        ++feedbackFilters;
        maxFeedback = std::max(maxFeedback, p.feedback);
    }

    const double seed = feedbackSeed(maxFeedback);
    const double driveL = inputL + 0.78 * feedbackL_ + seed;
    const double driveR = inputR + 0.78 * feedbackR_ - seed;
    double wetL = 0.0;
    double wetR = 0.0;
    double nextFeedbackL = 0.0;
    double nextFeedbackR = 0.0;
    int wetContributors = 0;

    for (int filter = 0; filter < kMaxFilters; ++filter) {
        const auto& p = params_[static_cast<size_t>(filter)];
        if (p.bypass) continue;

        const double bandL = resonators_[static_cast<size_t>(filter)][0].process(driveL);
        const double rawBandR = resonators_[static_cast<size_t>(filter)][1].process(driveR);
        const double bandR = rightSpreadDelay_[static_cast<size_t>(filter)].process(rawBandR, p.spreadMs);

        const double gainLinear = std::pow(10.0, p.gainDb / 20.0);
        const double bandAmount = std::clamp(p.gainDb / kMaxGainDb, -1.0, 1.0);
        const double addAmount = bandAmount >= 0.0 ? 2.0 * bandAmount : bandAmount;
        const double feedbackWetAmount = p.feedbackEnabled ? 0.45 * p.feedback : 0.0;
        if (std::abs(addAmount) > 1.0e-6 || feedbackWetAmount > 1.0e-6) {
            ++wetContributors;
        }
        wetL += addAmount * bandL;
        wetR += addAmount * bandR;

        if (p.feedbackEnabled) {
            const double feedbackCurve = std::pow(p.feedback, 1.15);
            const double sliderBoost = std::max(0.0, gainLinear - 1.0);
            const double gainInfluence = std::clamp(0.85 + 0.25 * sliderBoost, 0.70, 1.75);
            const double fb = (0.08 + 1.85 * feedbackCurve) * gainInfluence;
            nextFeedbackL += softClip(0.85 * fb * bandL);
            nextFeedbackR += softClip(0.85 * fb * bandR);
            wetL += feedbackWetAmount * bandL;
            wetR += feedbackWetAmount * bandR;
        }
    }

    const double wetNorm = wetContributors > 0 ? 1.0 / std::sqrt(0.5 * static_cast<double>(wetContributors) + 0.5) : 1.0;
    const double feedbackNorm = feedbackFilters > 0 ? 1.0 / std::sqrt(0.25 * static_cast<double>(feedbackFilters) + 0.75) : 1.0;
    feedbackL_ = softClip(nextFeedbackL * feedbackNorm);
    feedbackR_ = softClip(nextFeedbackR * feedbackNorm);

    const double processedL = limiterL_.process(inputL + wetL * wetNorm);
    const double processedR = limiterR_.process(inputR + wetR * wetNorm);
    wetDryCurrent_ += wetDrySmoothingCoeff_ * (wetDryTarget_ - wetDryCurrent_);
    outL = inputL + wetDryCurrent_ * (processedL - inputL);
    outR = inputR + wetDryCurrent_ * (processedR - inputR);
}

} // namespace overfilter
