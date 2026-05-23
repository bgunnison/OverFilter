// Copyright (c) 2026 Brian R. Gunnison
// MIT License
#pragma once

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

namespace overfilter {

constexpr int kMaxFilters = 10;
constexpr int kPerFilterParams = 9;

enum FilterParamSlot {
    kSlotMode = 0,
    kSlotTune,
    kSlotGain,
    kSlotQ,
    kSlotFeedback,
    kSlotPolarity,
    kSlotSpread,
    kSlotBypass,
    kSlotFeedbackEnable
};

enum ActiveParamSlot {
    kActiveMode = 0,
    kActiveTune,
    kActiveGain,
    kActiveQ,
    kActiveFeedback,
    kActivePolarity,
    kActiveSpread
};

constexpr int kActiveParamCount = 7;

enum class TuningMode {
    FixedHz = 0,
    Note = 1
};

constexpr double kMinFrequencyHz = 20.0;
constexpr double kMaxFrequencyHz = 20000.0;
constexpr double kMinGainDb = -24.0;
constexpr double kMaxGainDb = 24.0;
constexpr double kMinQ = 0.4;
constexpr double kMaxQ = 50.0;
constexpr double kMaxSpreadMs = 30.0;
constexpr double kPi = 3.14159265358979323846;
constexpr int kAbletonOctaveOffset = 2;

inline TuningMode normalizedToMode(double normalized) {
    return normalized >= 0.5 ? TuningMode::Note : TuningMode::FixedHz;
}

inline double modeToNormalized(TuningMode mode) {
    return mode == TuningMode::Note ? 1.0 : 0.0;
}

inline double clamp01(double value) {
    return std::clamp(value, 0.0, 1.0);
}

inline double normalizedToFixedFrequency(double normalized) {
    const double n = clamp01(normalized);
    const double minLog = std::log(kMinFrequencyHz);
    const double maxLog = std::log(kMaxFrequencyHz);
    return std::exp(minLog + n * (maxLog - minLog));
}

inline double fixedFrequencyToNormalized(double frequencyHz) {
    const double hz = std::clamp(frequencyHz, kMinFrequencyHz, kMaxFrequencyHz);
    const double minLog = std::log(kMinFrequencyHz);
    const double maxLog = std::log(kMaxFrequencyHz);
    return clamp01((std::log(hz) - minLog) / (maxLog - minLog));
}

inline int normalizedToMidiNote(double normalized) {
    return std::clamp(static_cast<int>(std::round(clamp01(normalized) * 127.0)), 0, 127);
}

inline double midiNoteToNormalized(int midiNote) {
    return static_cast<double>(std::clamp(midiNote, 0, 127)) / 127.0;
}

inline double midiNoteToFrequency(int midiNote) {
    return 440.0 * std::pow(2.0, (static_cast<double>(std::clamp(midiNote, 0, 127)) - 69.0) / 12.0);
}

inline int frequencyToNearestMidiNote(double frequencyHz) {
    const double hz = std::clamp(frequencyHz, kMinFrequencyHz, kMaxFrequencyHz);
    const double midi = 69.0 + 12.0 * std::log2(hz / 440.0);
    return std::clamp(static_cast<int>(std::round(midi)), 0, 127);
}

inline double normalizedToFrequency(double modeNormalized, double tuneNormalized) {
    if (normalizedToMode(modeNormalized) == TuningMode::Note) {
        return midiNoteToFrequency(normalizedToMidiNote(tuneNormalized));
    }
    return normalizedToFixedFrequency(tuneNormalized);
}

inline double normalizedToGainDb(double normalized) {
    return kMinGainDb + clamp01(normalized) * (kMaxGainDb - kMinGainDb);
}

inline double gainDbToNormalized(double gainDb) {
    return clamp01((std::clamp(gainDb, kMinGainDb, kMaxGainDb) - kMinGainDb) / (kMaxGainDb - kMinGainDb));
}

inline double normalizedToQ(double normalized) {
    const double n = clamp01(normalized);
    const double minLog = std::log(kMinQ);
    const double maxLog = std::log(kMaxQ);
    return std::exp(minLog + n * (maxLog - minLog));
}

inline double qToNormalized(double q) {
    const double qClamped = std::clamp(q, kMinQ, kMaxQ);
    const double minLog = std::log(kMinQ);
    const double maxLog = std::log(kMaxQ);
    return clamp01((std::log(qClamped) - minLog) / (maxLog - minLog));
}

inline double normalizedToFeedback(double normalized) {
    return clamp01(normalized);
}

inline double normalizedToSpreadMs(double normalized) {
    return clamp01(normalized) * kMaxSpreadMs;
}

inline bool normalizedToBool(double normalized) {
    return normalized >= 0.5;
}

inline double boolToNormalized(bool value) {
    return value ? 1.0 : 0.0;
}

inline const char* noteNameForMidi(int midiNote) {
    static constexpr const char* kNames[] = {
        "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
    };
    return kNames[static_cast<size_t>(std::clamp(midiNote, 0, 127) % 12)];
}

inline int octaveForMidi(int midiNote) {
    return (std::clamp(midiNote, 0, 127) / 12) - kAbletonOctaveOffset;
}

inline std::string noteStringForMidi(int midiNote) {
    char text[16]{};
    std::snprintf(text, sizeof(text), "%s%d", noteNameForMidi(midiNote), octaveForMidi(midiNote));
    return text;
}

inline bool parseNoteString(const char* text, int& midiNote) {
    if (!text) return false;
    std::string s(text);
    s.erase(std::remove_if(s.begin(), s.end(), [](unsigned char c) { return std::isspace(c) != 0; }), s.end());
    if (s.empty()) return false;
    for (auto& ch : s) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));

    int noteIndex = -1;
    size_t pos = 1;
    switch (s[0]) {
        case 'C': noteIndex = 0; break;
        case 'D': noteIndex = 2; break;
        case 'E': noteIndex = 4; break;
        case 'F': noteIndex = 5; break;
        case 'G': noteIndex = 7; break;
        case 'A': noteIndex = 9; break;
        case 'B': noteIndex = 11; break;
        default: return false;
    }

    if (pos < s.size() && (s[pos] == '#' || s[pos] == 'B')) {
        noteIndex += (s[pos] == '#') ? 1 : -1;
        pos += 1;
    }
    noteIndex = (noteIndex + 12) % 12;

    int octave = 3;
    if (pos < s.size()) {
        try {
            octave = std::stoi(s.substr(pos));
        } catch (...) {
            return false;
        }
    }

    midiNote = std::clamp((octave + kAbletonOctaveOffset) * 12 + noteIndex, 0, 127);
    return true;
}

inline double defaultNormalized(int filterIndex, int slot) {
    static constexpr std::array<double, kMaxFilters> kFixedHz = {
        61.0, 115.0, 220.0, 261.625565, 329.627557,
        391.995436, 440.0, 523.251131, 5200.0, 11000.0
    };
    static constexpr std::array<int, kMaxFilters> kNotes = {
        36, 43, 57, 60, 64, 67, 69, 72, 84, 96
    };

    filterIndex = std::clamp(filterIndex, 0, kMaxFilters - 1);
    switch (slot) {
        case kSlotMode:
            return boolToNormalized(filterIndex >= 2 && filterIndex <= 7);
        case kSlotTune:
            if (filterIndex >= 2 && filterIndex <= 7) {
                return midiNoteToNormalized(kNotes[static_cast<size_t>(filterIndex)]);
            }
            return fixedFrequencyToNormalized(kFixedHz[static_cast<size_t>(filterIndex)]);
        case kSlotGain:
            return gainDbToNormalized(0.0);
        case kSlotQ:
            return qToNormalized(7.2);
        case kSlotFeedback:
            return 0.42;
        case kSlotPolarity:
            return boolToNormalized(true);
        case kSlotSpread:
            return 18.0 / kMaxSpreadMs;
        case kSlotBypass:
            return 0.0;
        case kSlotFeedbackEnable:
            return boolToNormalized(filterIndex == 0 || (filterIndex >= 2 && filterIndex <= 5));
        default:
            return 0.0;
    }
}

inline int activeSlotToFilterSlot(int activeSlot) {
    switch (activeSlot) {
        case kActiveMode: return kSlotMode;
        case kActiveTune: return kSlotTune;
        case kActiveGain: return kSlotGain;
        case kActiveQ: return kSlotQ;
        case kActiveFeedback: return kSlotFeedback;
        case kActivePolarity: return kSlotPolarity;
        case kActiveSpread: return kSlotSpread;
        default: break;
    }
    return kSlotTune;
}

} // namespace overfilter
