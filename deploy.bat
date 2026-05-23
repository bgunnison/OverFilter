@echo off
setlocal

set ROOT=%~dp0
set DEST=C:\ProgramData\vstplugins

if not exist "%DEST%" mkdir "%DEST%"

if exist "%DEST%\DebugOverFilter.vst3" rmdir /s /q "%DEST%\DebugOverFilter.vst3"
if exist "%DEST%\OverFilter.vst3" rmdir /s /q "%DEST%\OverFilter.vst3"

xcopy /e /i /y "%ROOT%build\VST3\Debug\OverFilter.vst3" "%DEST%\DebugOverFilter.vst3"
if errorlevel 1 goto :done

xcopy /e /i /y "%ROOT%build\VST3\Release\OverFilter.vst3" "%DEST%\OverFilter.vst3"

:done
pause
endlocal
