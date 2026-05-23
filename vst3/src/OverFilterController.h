// Copyright (c) 2026 Brian R. Gunnison
// MIT License
#pragma once

#include "OverFilterIDs.h"

#include "public.sdk/source/vst/vsteditcontroller.h"
#include "vstgui/plugin-bindings/vst3editor.h"

#include <unordered_map>
#include <vector>

namespace overfilter {

class OverFilterController : public Steinberg::Vst::EditControllerEx1, public VSTGUI::VST3EditorDelegate {
public:
    OverFilterController() = default;
    static Steinberg::FUnknown* createInstance(void*) {
        return static_cast<Steinberg::Vst::IEditController*>(new OverFilterController());
    }

    Steinberg::tresult PLUGIN_API initialize(Steinberg::FUnknown* context) SMTG_OVERRIDE;
    Steinberg::IPlugView* PLUGIN_API createView(Steinberg::FIDString name) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API setComponentState(Steinberg::IBStream* state) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API getState(Steinberg::IBStream* state) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API setState(Steinberg::IBStream* state) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API setParamNormalized(Steinberg::Vst::ParamID pid, Steinberg::Vst::ParamValue value) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API getParamStringByValue(Steinberg::Vst::ParamID pid,
                                                        Steinberg::Vst::ParamValue valueNormalized,
                                                        Steinberg::Vst::String128 string) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API getParamValueByString(Steinberg::Vst::ParamID pid,
                                                        Steinberg::Vst::TChar* string,
                                                        Steinberg::Vst::ParamValue& valueNormalized) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API setComponentHandler(Steinberg::Vst::IComponentHandler* handler) SMTG_OVERRIDE;
    VSTGUI::CView* verifyView(VSTGUI::CView* view, const VSTGUI::UIAttributes& attributes,
                              const VSTGUI::IUIDescription* description,
                              VSTGUI::VST3Editor* editor) SMTG_OVERRIDE;
    VSTGUI::IController* createSubController(VSTGUI::UTF8StringPtr name, const VSTGUI::IUIDescription* description,
                                             VSTGUI::VST3Editor* editor) SMTG_OVERRIDE;
    void didOpen(VSTGUI::VST3Editor* editor) SMTG_OVERRIDE;

private:
    void buildParamOrder();
    Steinberg::Vst::ParamValue defaultNormalized(Steinberg::Vst::ParamID pid) const;
    int selectedFilterIndex();
    void syncActiveParams();
    void pushAllParamsToProcessor();
    void exposeAutomatableParams();
    void updateTuneForModeChange(Steinberg::Vst::ParamID modePid, TuningMode previousMode, TuningMode nextMode);
    TuningMode modeForParam(Steinberg::Vst::ParamID pid);
    bool isTuneParam(Steinberg::Vst::ParamID pid);
    int filterIndexForParam(Steinberg::Vst::ParamID pid);
    int slotForParam(Steinberg::Vst::ParamID pid);

    std::vector<Steinberg::Vst::ParamID> paramOrder_{};
    std::unordered_map<Steinberg::Vst::ParamID, Steinberg::Vst::ParamValue> paramState_{};
    bool syncingActive_{false};
    bool pendingProcessorSync_{false};
    bool pushingToProcessor_{false};
    bool autoExposed_{false};
};

} // namespace overfilter
