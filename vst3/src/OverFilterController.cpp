// Copyright (c) 2026 Brian R. Gunnison
// MIT License
#include "OverFilterController.h"

#include "base/source/fstreamer.h"
#include "pluginterfaces/base/ustring.h"
#include "vstgui/lib/controls/cautoanimation.h"
#include "vstgui/lib/controls/cbuttons.h"
#include "vstgui/uidescription/delegationcontroller.h"
#include "vstgui/plugin-bindings/vst3editor.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace overfilter {
using namespace Steinberg;
using namespace Steinberg::Vst;

namespace {

constexpr int32_t kFilterSelectButtonTagBase = 301;
constexpr int32_t kFilterSelectButtonTagLast = kFilterSelectButtonTagBase + kMaxFilters - 1;

bool isFilterSelectButtonTag(int32_t tag) {
    return tag >= kFilterSelectButtonTagBase && tag <= kFilterSelectButtonTagLast;
}

int filterIndexForButtonTag(int32_t tag) {
    return isFilterSelectButtonTag(tag) ? static_cast<int>(tag - kFilterSelectButtonTagBase) : -1;
}

ParamValue filterSelectNormalizedForIndex(int filterIndex) {
    if (kMaxFilters <= 1) return 0.0;
    return static_cast<ParamValue>(std::clamp(filterIndex, 0, kMaxFilters - 1)) /
           static_cast<ParamValue>(kMaxFilters - 1);
}

int filterIndexFromSelectNormalized(ParamValue normalized) {
    const int value = static_cast<int>(std::round(clamp01(normalized) * (kMaxFilters - 1)));
    return std::clamp(value, 0, kMaxFilters - 1);
}

class FilterSelectorController final : public VSTGUI::DelegationController {
public:
    FilterSelectorController(VSTGUI::IController* baseController, OverFilterController& controller)
        : DelegationController(baseController), controller_(controller) {}

    ~FilterSelectorController() override {
        for (auto* button : buttons_) {
            if (!button) continue;
            if (button->getListener() == this) {
                button->setListener(nullptr);
            }
            button->forget();
        }
    }

    VSTGUI::CView* verifyView(VSTGUI::CView* view, const VSTGUI::UIAttributes& attributes,
                              const VSTGUI::IUIDescription* description) override {
        if (auto* button = dynamic_cast<VSTGUI::CTextButton*>(view)) {
            const int filterIndex = filterIndexForButtonTag(button->getTag());
            if (filterIndex >= 0 && buttons_[static_cast<size_t>(filterIndex)] == nullptr) {
                button->setListener(this);
                button->remember();
                buttons_[static_cast<size_t>(filterIndex)] = button;
                syncButtonState(*button, filterIndex == selectedFilterIndex());
            }
        }
        return DelegationController::verifyView(view, attributes, description);
    }

    void valueChanged(VSTGUI::CControl* control) override {
        const int filterIndex = filterIndexForControl(control);
        if (filterIndex < 0) {
            DelegationController::valueChanged(control);
            return;
        }
        syncButtons(filterIndex);
        const ParamValue normalized = filterSelectNormalizedForIndex(filterIndex);
        controller_.setParamNormalized(kParamFilterSelect, normalized);
        controller_.performEdit(kParamFilterSelect, controller_.getParamNormalized(kParamFilterSelect));
    }

    void controlBeginEdit(VSTGUI::CControl* control) override {
        if (filterIndexForControl(control) < 0) {
            DelegationController::controlBeginEdit(control);
            return;
        }
        controller_.beginEdit(kParamFilterSelect);
    }

    void controlEndEdit(VSTGUI::CControl* control) override {
        if (filterIndexForControl(control) < 0) {
            DelegationController::controlEndEdit(control);
            return;
        }
        controller_.endEdit(kParamFilterSelect);
    }

private:
    int filterIndexForControl(VSTGUI::CControl* control) const {
        return control ? filterIndexForButtonTag(control->getTag()) : -1;
    }

    int selectedFilterIndex() const {
        return filterIndexFromSelectNormalized(controller_.getParamNormalized(kParamFilterSelect));
    }

    void syncButtonState(VSTGUI::CTextButton& button, bool selected) {
        button.setValue(selected ? 1.f : 0.f);
        button.invalid();
    }

    void syncButtons(int selectedIndex) {
        for (int filter = 0; filter < kMaxFilters; ++filter) {
            auto* button = buttons_[static_cast<size_t>(filter)];
            if (!button) continue;
            syncButtonState(*button, filter == selectedIndex);
        }
    }

    OverFilterController& controller_;
    std::array<VSTGUI::CTextButton*, kMaxFilters> buttons_{};
};

void toAscii(TChar* src, char* dst, size_t dstSize) {
    UString128 ustr(src);
    ustr.toAscii(dst, static_cast<int32>(dstSize));
}

} // namespace

tresult PLUGIN_API OverFilterController::initialize(FUnknown* context) {
    tresult result = EditControllerEx1::initialize(context);
    if (result != kResultOk) return result;

    buildParamOrder();

    auto addParam = [&](const char* title, ParamID id, ParamValue def, int32 flags, int32 stepCount = 0) {
        UString128 utitle;
        UString128 uunits;
        utitle.fromAscii(title);
        uunits.fromAscii("");
        parameters.addParameter(utitle, uunits, stepCount, def, flags, id);
        paramState_[id] = def;
    };

    addParam("Filter Select", kParamFilterSelect, 0.0, ParameterInfo::kIsHidden, kMaxFilters - 1);
    addParam("Wet/Dry", kParamWetDry, defaultNormalized(kParamWetDry), ParameterInfo::kCanAutomate);

    auto addActive = [&](const char* title, int activeSlot, ParamValue def, int32 stepCount = 0) {
        addParam(title, activeParamId(activeSlot), def, ParameterInfo::kIsHidden, stepCount);
    };
    addActive("Mode", kActiveMode, defaultNormalized(activeParamId(kActiveMode)), 1);
    addActive("Tune", kActiveTune, defaultNormalized(activeParamId(kActiveTune)));
    addActive("Gain", kActiveGain, defaultNormalized(activeParamId(kActiveGain)));
    addActive("Q", kActiveQ, defaultNormalized(activeParamId(kActiveQ)));
    addActive("Feedback", kActiveFeedback, defaultNormalized(activeParamId(kActiveFeedback)));
    addActive("Polarity", kActivePolarity, defaultNormalized(activeParamId(kActivePolarity)), 1);
    addActive("Spread", kActiveSpread, defaultNormalized(activeParamId(kActiveSpread)));

    for (int filter = 0; filter < kMaxFilters; ++filter) {
        const int number = filter + 1;
        auto addFilter = [&](const char* suffix, int slot, int32 stepCount = 0,
                             int32 flags = ParameterInfo::kCanAutomate) {
            char title[64]{};
            std::snprintf(title, sizeof(title), "Filter %d %s", number, suffix);
            addParam(title, filterParamId(filter, slot), defaultNormalized(filterParamId(filter, slot)),
                     flags, stepCount);
        };
        addFilter("Mode", kSlotMode, 1);
        addFilter("Tune", kSlotTune);
        addFilter("Gain", kSlotGain);
        addFilter("Q", kSlotQ);
        addFilter("Feedback", kSlotFeedback);
        addFilter("Polarity", kSlotPolarity, 1, ParameterInfo::kIsHidden);
        addFilter("Spread", kSlotSpread);
        addFilter("Bypass", kSlotBypass, 1);
        addFilter("Feedback Enable", kSlotFeedbackEnable, 1);
    }

    syncActiveParams();
    return kResultOk;
}

IPlugView* PLUGIN_API OverFilterController::createView(FIDString name) {
    if (FIDStringsEqual(name, ViewType::kEditor)) {
        return new VSTGUI::VST3Editor(this, "view", "overfilter.uidesc");
    }
    return EditControllerEx1::createView(name);
}

VSTGUI::CView* OverFilterController::verifyView(VSTGUI::CView* view, const VSTGUI::UIAttributes& attributes,
                                                const VSTGUI::IUIDescription* description,
                                                VSTGUI::VST3Editor* editor) {
    if (auto* animation = dynamic_cast<VSTGUI::CAutoAnimation*>(view)) {
        animation->openWindow();
        animation->setMouseEnabled(false);
    }
    return VSTGUI::VST3EditorDelegate::verifyView(view, attributes, description, editor);
}

VSTGUI::IController* OverFilterController::createSubController(VSTGUI::UTF8StringPtr name,
                                                               const VSTGUI::IUIDescription* description,
                                                               VSTGUI::VST3Editor* editor) {
    if (name && std::strcmp(name, "OverFilterSelector") == 0) {
        return new FilterSelectorController(editor, *this);
    }
    return VSTGUI::VST3EditorDelegate::createSubController(name, description, editor);
}

void OverFilterController::didOpen(VSTGUI::VST3Editor*) {
    if (autoExposed_) return;
    exposeAutomatableParams();
    autoExposed_ = true;
}

tresult PLUGIN_API OverFilterController::setComponentHandler(IComponentHandler* handler) {
    tresult result = EditControllerEx1::setComponentHandler(handler);
    if (result == kResultOk && handler) {
        if (pendingProcessorSync_) {
            pushAllParamsToProcessor();
        }
        if (!autoExposed_) {
            exposeAutomatableParams();
            autoExposed_ = true;
        }
    }
    return result;
}

void OverFilterController::exposeAutomatableParams() {
    if (!componentHandler) return;
    const int32 count = parameters.getParameterCount();
    for (int32 i = 0; i < count; ++i) {
        auto* param = parameters.getParameterByIndex(i);
        if (!param) continue;
        const auto flags = param->getInfo().flags;
        if (flags & ParameterInfo::kIsHidden) continue;
        if (flags & ParameterInfo::kIsReadOnly) continue;
        const ParamID pid = param->getInfo().id;
        const ParamValue value = getParamNormalized(pid);
        ParamValue nudge = value + 1e-5;
        if (nudge > 1.0) nudge = value - 1e-5;
        if (nudge < 0.0) nudge = value;
        beginEdit(pid);
        performEdit(pid, nudge);
        performEdit(pid, value);
        endEdit(pid);
    }
}

void OverFilterController::buildParamOrder() {
    paramOrder_.clear();
    paramOrder_.push_back(kParamFilterSelect);
    for (int filter = 0; filter < kMaxFilters; ++filter) {
        for (int slot = 0; slot < kPerFilterParams; ++slot) {
            paramOrder_.push_back(filterParamId(filter, slot));
        }
    }
    paramOrder_.push_back(kParamWetDry);
}

ParamValue OverFilterController::defaultNormalized(ParamID pid) const {
    if (pid == kParamFilterSelect) return 0.0;
    if (pid == kParamWetDry) return 1.0;
    if (pid >= kActiveParamBase && pid < kActiveParamBase + kActiveParamCount) {
        const int activeSlot = static_cast<int>(pid - kActiveParamBase);
        return overfilter::defaultNormalized(0, activeSlotToFilterSlot(activeSlot));
    }
    if (pid >= kParamBaseFilterParams && pid < kActiveParamBase) {
        const int rel = static_cast<int>(pid - kParamBaseFilterParams);
        return overfilter::defaultNormalized(rel / kPerFilterParams, rel % kPerFilterParams);
    }
    return 0.0;
}

int OverFilterController::selectedFilterIndex() {
    return filterIndexFromSelectNormalized(getParamNormalized(kParamFilterSelect));
}

int OverFilterController::filterIndexForParam(ParamID pid) {
    if (pid >= kParamBaseFilterParams && pid < kActiveParamBase) {
        return static_cast<int>(pid - kParamBaseFilterParams) / kPerFilterParams;
    }
    if (pid >= kActiveParamBase && pid < kActiveParamBase + kActiveParamCount) {
        return selectedFilterIndex();
    }
    return -1;
}

int OverFilterController::slotForParam(ParamID pid) {
    if (pid >= kParamBaseFilterParams && pid < kActiveParamBase) {
        return static_cast<int>(pid - kParamBaseFilterParams) % kPerFilterParams;
    }
    if (pid >= kActiveParamBase && pid < kActiveParamBase + kActiveParamCount) {
        return activeSlotToFilterSlot(static_cast<int>(pid - kActiveParamBase));
    }
    return -1;
}

bool OverFilterController::isTuneParam(ParamID pid) {
    return slotForParam(pid) == kSlotTune;
}

TuningMode OverFilterController::modeForParam(ParamID pid) {
    const int filter = filterIndexForParam(pid);
    if (filter < 0) return TuningMode::FixedHz;
    const ParamID modePid = (pid >= kActiveParamBase && pid < kActiveParamBase + kActiveParamCount)
        ? activeParamId(kActiveMode)
        : filterParamId(filter, kSlotMode);
    return normalizedToMode(getParamNormalized(modePid));
}

void OverFilterController::updateTuneForModeChange(ParamID modePid, TuningMode previousMode, TuningMode nextMode) {
    if (previousMode == nextMode) return;
    const int filter = filterIndexForParam(modePid);
    if (filter < 0 || filter >= kMaxFilters) return;

    const ParamID perFilterTuneId = filterParamId(filter, kSlotTune);
    const ParamValue currentTune = getParamNormalized(perFilterTuneId);
    ParamValue convertedTune = currentTune;
    if (previousMode == TuningMode::Note && nextMode == TuningMode::FixedHz) {
        convertedTune = fixedFrequencyToNormalized(midiNoteToFrequency(normalizedToMidiNote(currentTune)));
    } else if (previousMode == TuningMode::FixedHz && nextMode == TuningMode::Note) {
        convertedTune = midiNoteToNormalized(frequencyToNearestMidiNote(normalizedToFixedFrequency(currentTune)));
    }
    convertedTune = clamp01(convertedTune);

    const bool selected = filter == selectedFilterIndex();
    const ParamID activeTuneId = activeParamId(kActiveTune);
    syncingActive_ = true;
    EditControllerEx1::setParamNormalized(perFilterTuneId, convertedTune);
    paramState_[perFilterTuneId] = convertedTune;
    if (selected) {
        EditControllerEx1::setParamNormalized(activeTuneId, convertedTune);
        paramState_[activeTuneId] = convertedTune;
    }
    if (componentHandler) {
        componentHandler->beginEdit(perFilterTuneId);
        componentHandler->performEdit(perFilterTuneId, convertedTune);
        componentHandler->endEdit(perFilterTuneId);
        if (selected) {
            componentHandler->beginEdit(activeTuneId);
            componentHandler->performEdit(activeTuneId, convertedTune);
            componentHandler->endEdit(activeTuneId);
        }
        componentHandler->restartComponent(Vst::kParamValuesChanged);
    }
    syncingActive_ = false;
}

void OverFilterController::syncActiveParams() {
    if (syncingActive_) return;
    syncingActive_ = true;
    const int filter = selectedFilterIndex();
    for (int activeSlot = 0; activeSlot < kActiveParamCount; ++activeSlot) {
        const ParamID src = filterParamId(filter, activeSlotToFilterSlot(activeSlot));
        const ParamID dst = activeParamId(activeSlot);
        const ParamValue value = getParamNormalized(src);
        EditControllerEx1::setParamNormalized(dst, value);
        paramState_[dst] = value;
    }
    syncingActive_ = false;
    if (componentHandler) {
        componentHandler->restartComponent(Vst::kParamValuesChanged);
    }
}

void OverFilterController::pushAllParamsToProcessor() {
    if (!componentHandler || pushingToProcessor_) return;
    pushingToProcessor_ = true;
    pendingProcessorSync_ = false;
    for (auto pid : paramOrder_) {
        if (pid >= kActiveParamBase) continue;
        const ParamValue value = getParamNormalized(pid);
        componentHandler->beginEdit(pid);
        componentHandler->performEdit(pid, value);
        componentHandler->endEdit(pid);
    }
    pushingToProcessor_ = false;
}

tresult PLUGIN_API OverFilterController::setParamNormalized(ParamID pid, ParamValue value) {
    if (pendingProcessorSync_ && componentHandler) {
        pushAllParamsToProcessor();
    }
    const bool modeChange = !syncingActive_ && slotForParam(pid) == kSlotMode;
    const TuningMode previousMode = modeChange ? modeForParam(pid) : TuningMode::FixedHz;
    value = clamp01(value);
    const TuningMode nextMode = modeChange ? normalizedToMode(value) : TuningMode::FixedHz;
    paramState_[pid] = value;
    tresult result = EditControllerEx1::setParamNormalized(pid, value);

    if (pid == kParamFilterSelect) {
        syncActiveParams();
        return result;
    }

    if (pid >= kActiveParamBase && pid < kActiveParamBase + kActiveParamCount) {
        if (!syncingActive_) {
            const int activeSlot = static_cast<int>(pid - kActiveParamBase);
            const int filterSlot = activeSlotToFilterSlot(activeSlot);
            const ParamID perFilterId = filterParamId(selectedFilterIndex(), filterSlot);
            syncingActive_ = true;
            EditControllerEx1::setParamNormalized(perFilterId, value);
            paramState_[perFilterId] = value;
            if (componentHandler) {
                componentHandler->beginEdit(perFilterId);
                componentHandler->performEdit(perFilterId, value);
                componentHandler->endEdit(perFilterId);
                componentHandler->restartComponent(Vst::kParamValuesChanged);
            }
            syncingActive_ = false;
        }
        if (modeChange) {
            updateTuneForModeChange(pid, previousMode, nextMode);
        }
        return result;
    }

    if (pid >= kParamBaseFilterParams && pid < kActiveParamBase) {
        const int filter = filterIndexForParam(pid);
        const int slot = slotForParam(pid);
        if (!syncingActive_ && filter == selectedFilterIndex()) {
            for (int activeSlot = 0; activeSlot < kActiveParamCount; ++activeSlot) {
                if (activeSlotToFilterSlot(activeSlot) != slot) continue;
                const ParamID activeId = activeParamId(activeSlot);
                syncingActive_ = true;
                EditControllerEx1::setParamNormalized(activeId, value);
                paramState_[activeId] = value;
                if (componentHandler) {
                    componentHandler->beginEdit(activeId);
                    componentHandler->performEdit(activeId, value);
                    componentHandler->endEdit(activeId);
                    componentHandler->restartComponent(Vst::kParamValuesChanged);
                }
                syncingActive_ = false;
                break;
            }
        }
        if (modeChange) {
            updateTuneForModeChange(pid, previousMode, nextMode);
        }
    }

    return result;
}

tresult PLUGIN_API OverFilterController::setComponentState(IBStream* state) {
    if (!state) return kResultFalse;
    IBStreamer streamer(state, kLittleEndian);
    for (auto pid : paramOrder_) {
        double value = 0.0;
        if (!streamer.readDouble(value)) value = defaultNormalized(pid);
        EditControllerEx1::setParamNormalized(pid, clamp01(value));
        paramState_[pid] = clamp01(value);
    }
    syncActiveParams();
    if (componentHandler) {
        pushAllParamsToProcessor();
    } else {
        pendingProcessorSync_ = true;
    }
    return kResultOk;
}

tresult PLUGIN_API OverFilterController::setState(IBStream* state) {
    return setComponentState(state);
}

tresult PLUGIN_API OverFilterController::getState(IBStream* state) {
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

tresult PLUGIN_API OverFilterController::getParamStringByValue(ParamID pid, ParamValue valueNormalized, String128 string) {
    char text[64]{};
    const int slot = slotForParam(pid);

    if (pid == kParamFilterSelect) {
        std::snprintf(text, sizeof(text), "%d", filterIndexFromSelectNormalized(valueNormalized) + 1);
    } else if (pid == kParamWetDry) {
        std::snprintf(text, sizeof(text), "%.0f%%", 100.0 * clamp01(valueNormalized));
    } else if (slot == kSlotMode) {
        std::snprintf(text, sizeof(text), "%s", normalizedToMode(valueNormalized) == TuningMode::Note ? "Note" : "Fixed Hz");
    } else if (slot == kSlotTune) {
        if (modeForParam(pid) == TuningMode::Note) {
            const auto note = noteStringForMidi(normalizedToMidiNote(valueNormalized));
            std::snprintf(text, sizeof(text), "%s", note.c_str());
        } else {
            const double hz = normalizedToFixedFrequency(valueNormalized);
            if (hz >= 1000.0) {
                std::snprintf(text, sizeof(text), "%.2f kHz", hz / 1000.0);
            } else {
                std::snprintf(text, sizeof(text), "%.0f Hz", hz);
            }
        }
    } else if (slot == kSlotGain) {
        std::snprintf(text, sizeof(text), "%+.1f dB", normalizedToGainDb(valueNormalized));
    } else if (slot == kSlotQ) {
        std::snprintf(text, sizeof(text), "%.1f", normalizedToQ(valueNormalized));
    } else if (slot == kSlotFeedback) {
        std::snprintf(text, sizeof(text), "%.0f%%", 100.0 * normalizedToFeedback(valueNormalized));
    } else if (slot == kSlotPolarity) {
        std::snprintf(text, sizeof(text), "%s", normalizedToBool(valueNormalized) ? "+" : "-");
    } else if (slot == kSlotSpread) {
        std::snprintf(text, sizeof(text), "%.1f ms", normalizedToSpreadMs(valueNormalized));
    } else if (slot == kSlotBypass) {
        std::snprintf(text, sizeof(text), "%s", normalizedToBool(valueNormalized) ? "Bypass" : "On");
    } else if (slot == kSlotFeedbackEnable) {
        std::snprintf(text, sizeof(text), "%s", normalizedToBool(valueNormalized) ? "Feedback" : "Off");
    } else {
        return EditControllerEx1::getParamStringByValue(pid, valueNormalized, string);
    }

    UString(string, str16BufferSize(String128)).fromAscii(text);
    return kResultOk;
}

tresult PLUGIN_API OverFilterController::getParamValueByString(ParamID pid, TChar* string, ParamValue& valueNormalized) {
    char ascii[64]{};
    toAscii(string, ascii, sizeof(ascii));
    std::string s(ascii);
    for (auto& ch : s) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));

    const int slot = slotForParam(pid);
    if (pid == kParamWetDry) {
        char* end = nullptr;
        const double parsed = std::strtod(ascii, &end);
        if (end != ascii) {
            valueNormalized = clamp01(parsed > 1.0 ? parsed / 100.0 : parsed);
            return kResultOk;
        }
    }
    if (slot == kSlotMode) {
        valueNormalized = s.find("NOTE") != std::string::npos ? 1.0 : 0.0;
        return kResultOk;
    }
    if (slot == kSlotTune) {
        int midi = 0;
        if (parseNoteString(ascii, midi)) {
            valueNormalized = midiNoteToNormalized(midi);
            return kResultOk;
        }
        char* end = nullptr;
        double parsed = std::strtod(ascii, &end);
        if (end != ascii) {
            if (s.find("KHZ") != std::string::npos) parsed *= 1000.0;
            valueNormalized = fixedFrequencyToNormalized(parsed);
            return kResultOk;
        }
    }
    if (slot == kSlotGain) {
        char* end = nullptr;
        const double parsed = std::strtod(ascii, &end);
        if (end != ascii) {
            valueNormalized = gainDbToNormalized(parsed);
            return kResultOk;
        }
    }
    if (slot == kSlotQ) {
        char* end = nullptr;
        const double parsed = std::strtod(ascii, &end);
        if (end != ascii) {
            valueNormalized = qToNormalized(parsed);
            return kResultOk;
        }
    }
    if (slot == kSlotFeedback) {
        char* end = nullptr;
        const double parsed = std::strtod(ascii, &end);
        if (end != ascii) {
            valueNormalized = clamp01(parsed > 1.0 ? parsed / 100.0 : parsed);
            return kResultOk;
        }
    }
    if (slot == kSlotPolarity) {
        valueNormalized = s.find("-") != std::string::npos ? 0.0 : 1.0;
        return kResultOk;
    }
    if (slot == kSlotSpread) {
        char* end = nullptr;
        const double parsed = std::strtod(ascii, &end);
        if (end != ascii) {
            valueNormalized = clamp01(parsed / kMaxSpreadMs);
            return kResultOk;
        }
    }
    if (slot == kSlotBypass) {
        valueNormalized = (s.find("BYPASS") != std::string::npos || s == "1") ? 1.0 : 0.0;
        return kResultOk;
    }
    if (slot == kSlotFeedbackEnable) {
        valueNormalized = (s.find("ON") != std::string::npos || s.find("FEED") != std::string::npos || s == "1") ? 1.0 : 0.0;
        return kResultOk;
    }

    return EditControllerEx1::getParamValueByString(pid, string, valueNormalized);
}

} // namespace overfilter
