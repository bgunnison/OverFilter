// Copyright (c) 2026 Brian R. Gunnison
// MIT License
#pragma once

#include <algorithm>
#include <cmath>

namespace overfilter::dsp {

class TwoPoleResonator {
public:
    static constexpr double kDefaultSampleRate = 44100.0;
    static constexpr double kDefaultFrequency = 440.0;
    static constexpr double kDefaultQ = 8.0;
    static constexpr double kMinFrequency = 1.0;
    static constexpr double kMaxNyquistRatio = 0.49;
    static constexpr double kMinQ = 0.1;
    static constexpr double kPi = 3.14159265358979323846;

    void reset() {
        x1_ = 0.0;
        x2_ = 0.0;
        y1_ = 0.0;
        y2_ = 0.0;
    }

    void setSampleRate(double sampleRate) {
        sampleRate_ = sampleRate > 0.0 ? sampleRate : kDefaultSampleRate;
        updateCoefficients();
    }

    void setFrequency(double frequency) {
        frequency_ = std::max(frequency, kMinFrequency);
        updateCoefficients();
    }

    void setQ(double q) {
        q_ = std::max(q, kMinQ);
        updateCoefficients();
    }

    void setFeedback(double feedback) {
        feedback_ = std::clamp(feedback, 0.0, 1.0);
    }

    double process(double x) {
        if (!std::isfinite(x1_) || !std::isfinite(x2_) || !std::isfinite(y1_) || !std::isfinite(y2_)) {
            reset();
        }

        const double y = b0_ * x + b1_ * x1_ + b2_ * x2_ - a1_ * y1_ - a2_ * y2_;
        if (!std::isfinite(y)) {
            reset();
            return 0.0;
        }

        x2_ = x1_;
        x1_ = x;
        y2_ = y1_;
        y1_ = y;
        constrainState();
        return y1_;
    }

private:
    void constrainState() {
        static constexpr double kStateLimit = 4.0;
        const double peak = std::max(std::abs(y1_), std::abs(y2_));
        if (peak > kStateLimit) {
            const double scale = kStateLimit / peak;
            y1_ *= scale;
            y2_ *= scale;
        }
    }

    void updateCoefficients() {
        double frequency = frequency_;
        if (!std::isfinite(frequency) || frequency <= 0.0) {
            frequency = kMinFrequency;
        }
        const double maxFrequency = kMaxNyquistRatio * sampleRate_;
        frequency = std::clamp(frequency, kMinFrequency, maxFrequency);
        const double omega = 2.0 * kPi * frequency / sampleRate_;
        const double sinOmega = std::sin(omega);
        const double alpha = sinOmega / (2.0 * std::max(q_, kMinQ));
        const double a0 = 1.0 + alpha;
        if (!std::isfinite(a0) || a0 == 0.0) {
            b0_ = b1_ = b2_ = a1_ = a2_ = 0.0;
            return;
        }

        b0_ = alpha / a0;
        b1_ = 0.0;
        b2_ = -alpha / a0;
        a1_ = -2.0 * std::cos(omega) / a0;
        a2_ = (1.0 - alpha) / a0;
    }

    double sampleRate_{kDefaultSampleRate};
    double frequency_{kDefaultFrequency};
    double q_{kDefaultQ};
    double feedback_{0.0};
    double b0_{0.0};
    double b1_{0.0};
    double b2_{0.0};
    double a1_{0.0};
    double a2_{0.0};
    double x1_{0.0};
    double x2_{0.0};
    double y1_{0.0};
    double y2_{0.0};
};

} // namespace overfilter::dsp
