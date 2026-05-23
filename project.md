# OverFilter VST3 Product Summary

OverFilter is a VST3 audio effect for Ableton Live based on the existing `C:\projects\ableplugs\beat` plugin structure and UI pattern. The plugin is a ten-band stereo resonant filterbank where each filter can be selected and edited individually.

The goal is to make a software Resonant Filterbank concept: tuned resonant bands, per-band feedback, stereo spread, and self-oscillating spectral tone shaping. 

## Core Idea

OverFilter lets users tune each resonant filter either by direct frequency or by musical note.

This allows users to build harmonic resonator layouts manually. For example, a drum loop can be passed through filters tuned to `A2`, `C3`, `E3`, and `G3`, making the loop ring in an A minor shape.

Switching tuning modes preserves the audible target: changing from `Note` to `Fixed Hz` converts the selected note into its exact frequency, and changing from `Fixed Hz` to `Note` snaps the tune value to the nearest note and displays the note name.

## V1 Scope

- Ten selectable stereo resonant filters.
- Each filter has its own settings.
- Each filter can use one of two tuning modes:
  - `Fixed Hz`: center frequency is edited directly in Hz.
  - `Note`: center frequency is selected as note name plus octave, such as `C2`, `F#3`, or `A4`.
  - Note names follow Ableton Live's octave convention, where MIDI note 60 is `C3`.

## Per-Filter Controls

Each numbered filter button selects one filter and exposes its controls:

- `Mode`: `Fixed Hz` or `Note`.
- `Tune`: frequency in Hz for fixed mode, or note plus octave for note mode.
- `Gain`: center-detent band amount. Center is flat, positive values add the selected bandpass region, and negative values subtract it for band-reject/notch-like behavior. Tuning a filter to a note does not change the input by itself until `Gain` moves away from center or feedback is engaged.
- `Q`: controls bandwidth and resonant sharpness for both boost and reject behavior. Feedback can emphasize resonance, but it should not replace the Q setting.
- `Feedback`: amount of this filter sent into the feedback path.
- `Spread`: stereo offset for the selected filter.

Global controls:

- `Wet/Dry`: blends from dry input to the full OverFilter processed output. It is global and lives in the header area under the title.

Per-filter toggles:

- `B`: if selected bypasses the filter.
- `F`: includes the filter in the feedback network.

## Phase And Summing

The hardware reference runs one common input summing node into ten parallel bandpass filters. Selected feedback paths route band outputs back into that input node. OverFilter follows that structure: each active filter listens to the same input-plus-feedback drive, then selected `F` filters contribute their band output to the shared feedback return.

Each filter is a resonant bandpass stage, so its phase is frequency-dependent. Around the filter center frequency the band output is most coherent with itself and will build strongly; below and above the center frequency it shifts in opposite directions. When ten filters are summed in parallel, overlapping bands can reinforce or partially cancel each other depending on their tuning, Q, gain, and stereo spread.

This matters most when several filters are tuned close together or when feedback is active. The shared feedback path sums the feedback-enabled filters into one positive return, so filters with compatible phase relationships can build together, while filters that return out of phase can suppress each other or make resonance feel less obvious.

As on the hardware, self-oscillation is created by a feedback loop around the selected bandpass filters. High feedback narrows the effective resonance and routes the selected bands back into the common drive node, so one enabled band can ring at its displayed note or fixed Hz value, and multiple enabled bands can interact through the shared feedback path.

The global `Wet/Dry` control also changes the perceived phase behavior. Lower wet settings reduce the strength of the resonant contribution, so phase-related peaks and dips are less obvious. Higher wet settings make the parallel filter sum and shared feedback behavior more audible.

## UI Direction

The UI should follow the foundation established by the Beat VST3:

- White title bar.
- Bold blue main panel.
- Black rectangular controls.
- Numbered selectable slots.
- Selected slot outlined in red.
- A centered plugin title and `Select` button.
- A PinchFX-style animated logo in the upper left with transparent background.
- A widened editor with the filter select buttons evenly dispersed horizontally.
- A persistent global `Wet/Dry` slider in the header to the right of the logo.
- The per-filter sliders end-aligned with filter select button `10`.
- A two-button exclusive mode selector labeled `Fixed Hz` and `Note`.

For OverFilter, the numbered slots represent filters `1-10`. Selecting a filter changes the visible control panel to that filter's settings.

Suggested title text:

- Header: `OverFilter/1-Filter`
- Center title: `OverFilter`

## Example Filter Layout

Example manual harmonic setup:

- `1`: Fixed Hz, `61 Hz`
- `2`: Fixed Hz, `115 Hz`
- `3`: Note, `A2`
- `4`: Note, `C3`
- `5`: Note, `E3`
- `6`: Note, `G3`
- `7`: Note, `A3`
- `8`: Note, `C4`
- `9`: Fixed Hz, `5.2 kHz`
- `10`: Fixed Hz, `11 kHz`

This creates a mix of stable low-frequency filterbank behavior and manually tuned harmonic resonance.

## Product Positioning

OverFilter is not a normal EQ or a generic filter plugin. It is a playable resonant filterbank for shaping audio into tuned spectral feedback. It is useful for drums, bass, synths, vocals, pads, returns, noise, and experimental sound design.
