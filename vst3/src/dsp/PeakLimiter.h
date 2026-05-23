// Copyright (c) 2026 Brian R. Gunnison
// MIT License
#pragma once

#include <algorithm>
#include <cmath>

namespace overfilter::dsp {

class PeakLimiter {
public:
    void setSampleRate(double sampleRate) {
        sampleRate_ = sampleRate > 0.0 ? sampleRate : 44100.0;
        updateCoefficient();
    }

    void reset() {
        peak_ = 0.0;
    }

    void setThreshold(double threshold) {
        threshold_ = std::max(0.01, threshold);
    }

    void setReleaseMs(double releaseMs) {
        releaseMs_ = std::max(1.0, releaseMs);
        updateCoefficient();
    }

    double process(double x) {
        if (!std::isfinite(x)) {
            reset();
            return 0.0;
        }

        const double safe = std::clamp(x, -32.0, 32.0);
        peak_ = std::max(std::abs(safe), releaseCoeff_ * peak_);
        peak_ = std::min(peak_, 32.0);
        const double gain = std::min(1.0, threshold_ / (peak_ + 1e-9));
        const double y = safe * gain;
        if (!std::isfinite(y)) {
            reset();
            return 0.0;
        }
        return y;
    }

private:
    void updateCoefficient() {
        releaseCoeff_ = std::exp(-1.0 / ((releaseMs_ * 0.001) * sampleRate_));
    }

    double sampleRate_{44100.0};
    double releaseMs_{45.0};
    double releaseCoeff_{0.0};
    double threshold_{0.86};
    double peak_{0.0};
};

} // namespace overfilter::dsp
