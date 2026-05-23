@echo off
setlocal

set ROOT=%~dp0
set ZIP=%ROOT%OverFilter.vst3.zip
set BUNDLE=%ROOT%build\VST3\Release\OverFilter.vst3

if exist "%ZIP%" del "%ZIP%"
if not exist "%BUNDLE%" (
  echo Release bundle not found: "%BUNDLE%"
  goto :done
)

powershell -NoProfile -ExecutionPolicy Bypass -Command "Compress-Archive -Path '%BUNDLE%' -DestinationPath '%ZIP%' -Force"

:done
pause
endlocal
