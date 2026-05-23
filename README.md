# OverFilter

OverFilter is a VST3 stereo resonant filterbank effect for Ableton Live.

It provides ten selectable resonant filters. Each filter can be tuned by direct frequency (`Fixed Hz`) or by musical note (`Note`), then shaped with gain, Q, feedback amount, feedback polarity, stereo spread, bypass, and feedback-enable controls.

## Build

Set `VST3_SDK_ROOT` to the local Steinberg VST3 SDK checkout, then run:

```bat
build.bat
```

The build outputs are created under `build\VST3\Debug` and `build\VST3\Release`.

## Deploy

```bat
deploy.bat
```

This copies:

- `C:\ProgramData\vstplugins\DebugOverFilter.vst3`
- `C:\ProgramData\vstplugins\OverFilter.vst3`

Close Ableton Live before deploying if the installed bundle is locked.
