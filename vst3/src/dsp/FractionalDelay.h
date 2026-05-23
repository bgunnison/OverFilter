// Copyright (c) 2026 Brian R. Gunnison
// MIT License
#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

namespace overfilter::dsp {

class FractionalDelay {
public:
    void prepare(double sampleRate, double maxDelayMs) {
        sampleRate_ = sampleRate > 0.0 ? sampleRate : 44100.0;
        maxDelayMs_ = std::max(0.0, maxDelayMs);
        smoothingCoeff_ = 1.0 - std::exp(-1.0 / (0.04 * sampleRate_));
        const auto size = static_cast<size_t>(std::ceil(sampleRate_ * maxDelayMs * 0.001)) + 4;
        buffer_.assign(std::max<size_t>(size, 8), 0.0);
        writeIndex_ = 0;
        initialized_ = false;
    }

    void reset() {
        std::fill(buffer_.begin(), buffer_.end(), 0.0);
        writeIndex_ = 0;
        initialized_ = false;
    }

    double process(double input, double delayMs) {
        if (buffer_.empty()) {
            prepare(sampleRate_, 30.0);
        }

        targetDelayMs_ = std::clamp(delayMs, 0.0, maxDelayMs_);
        if (!initialized_) {
            currentDelayMs_ = targetDelayMs_;
            initialized_ = true;
        } else {
            currentDelayMs_ += smoothingCoeff_ * (targetDelayMs_ - currentDelayMs_);
        }

        const double maxDelaySamples = static_cast<double>(buffer_.size() - 3);
        const double delaySamples = std::clamp(currentDelayMs_ * 0.001 * sampleRate_, 0.0, maxDelaySamples);
        if (delaySamples <= 1e-9) {
            buffer_[writeIndex_] = std::isfinite(input) ? input : 0.0;
            writeIndex_ = (writeIndex_ + 1) % buffer_.size();
            return buffer_[(writeIndex_ + buffer_.size() - 1) % buffer_.size()];
        }
        const double readPos = static_cast<double>(writeIndex_) - delaySamples;
        double wrapped = readPos;
        while (wrapped < 0.0) wrapped += static_cast<double>(buffer_.size());

        const auto index0 = static_cast<size_t>(wrapped) % buffer_.size();
        const auto index1 = (index0 + 1) % buffer_.size();
        const double frac = wrapped - std::floor(wrapped);
        const double output = buffer_[index0] + frac * (buffer_[index1] - buffer_[index0]);

        buffer_[writeIndex_] = std::isfinite(input) ? input : 0.0;
        writeIndex_ = (writeIndex_ + 1) % buffer_.size();
        return output;
    }

private:
    double sampleRate_{44100.0};
    double maxDelayMs_{30.0};
    double targetDelayMs_{0.0};
    double currentDelayMs_{0.0};
    double smoothingCoeff_{1.0};
    std::vector<double> buffer_{};
    size_t writeIndex_{0};
    bool initialized_{false};
};

} // namespace overfilter::dsp
