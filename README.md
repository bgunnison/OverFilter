# OverFilter

OverFilter is a VST3 stereo resonant filterbank effect for Ableton Live.

It provides ten selectable resonant filters. Each filter can be tuned by direct frequency (`Fixed Hz`) or by musical note (`Note`), then shaped with gain, Q, feedback amount, stereo spread, bypass, and feedback-enable controls.

## Usage

 `Wet/Dry` blends the dry input with the complete OverFilter output.  

The numbered buttons select filters `1` through `10`. The selected filter is outlined in red, and the controls below the selector edit that filter. The small readout above each filter's `B` and `F` buttons shows its current tuning.

Each filter has two small toggle buttons:

- `B`: bypasses that filter.
- `F`: includes that filter in the shared feedback network.

The selected filter controls are:

- `Mode`: chooses `Fixed Hz` or `Note`. These are exclusive buttons. Switching from `Note` to `Fixed Hz` converts the current note to its exact frequency. Switching from `Fixed Hz` to `Note` snaps the current frequency to the nearest note.
- `Tune`: sets the center frequency. In `Fixed Hz` mode it displays Hz or kHz. In `Note` mode it displays note names using Ableton's octave convention, where MIDI note 60 is `C3`. 
- `Gain`: center-detent band amount. Center is flat, positive values add the selected bandpass region, and negative values subtract it for band-reject/notch-like behavior.
- `Q`: controls bandwidth and resonant sharpness. Higher values make the band narrower and more resonant.
- `Feedback`: sets how much of this filter is sent into the feedback path when `F` is enabled.
- `Spread`: adds stereo offset for the selected filter by delaying the right-side band signal.

For harmonic resonator patches, set several filters to `Note`, choose notes from a chord, raise `Gain` or `Feedback`, and enable `F` only on the filters you want feeding the shared resonance loop.

## Build

Set `VST3_SDK_ROOT` to the local Steinberg VST3 SDK checkout, then run:

```bat
build.bat
```

