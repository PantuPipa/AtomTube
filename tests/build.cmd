@echo off
setlocal
cd /d "%~dp0.."
if not "%VSCMD_ARG_TGT_ARCH%"=="x86" exit /b 1
if not exist build mkdir build
pushd build
cl /nologo /O2 /W4 /EHsc /MT /utf-8 /std:c++14 /DUNICODE /D_UNICODE /D_WIN32_WINNT=0x0601 /DWINVER=0x0601 /DNOMINMAX ..\tests\playback.cpp ..\src\player.cpp ..\src\stream.cpp ..\src\youtube.cpp ..\src\diagnostics.cpp /Fe:playback.exe /link /SUBSYSTEM:CONSOLE,6.01 user32.lib gdi32.lib ole32.lib oleaut32.lib mf.lib mfplat.lib mfuuid.lib evr.lib strmiids.lib psapi.lib advapi32.lib shell32.lib d3d9.lib
set result=%ERRORLEVEL%
if not "%result%"=="0" goto done
cl /nologo /O2 /W4 /EHsc /MT /utf-8 /std:c++14 /DUNICODE /D_UNICODE /D_WIN32_WINNT=0x0601 /DWINVER=0x0601 /DNOMINMAX ..\tests\cores.cpp stream.obj diagnostics.obj /Fe:cores.exe /link /SUBSYSTEM:CONSOLE,6.01 user32.lib gdi32.lib ole32.lib psapi.lib advapi32.lib shell32.lib d3d9.lib
set result=%ERRORLEVEL%
if not "%result%"=="0" goto done
cl /nologo /O2 /W4 /EHsc /MT /utf-8 /std:c++14 /DUNICODE /D_UNICODE /D_WIN32_WINNT=0x0601 /DWINVER=0x0601 /DNOMINMAX ..\tests\resolve.cpp youtube.obj diagnostics.obj /Fe:resolve.exe /link /SUBSYSTEM:CONSOLE,6.01 user32.lib gdi32.lib ole32.lib psapi.lib advapi32.lib shell32.lib d3d9.lib
set result=%ERRORLEVEL%
if not "%result%"=="0" goto done
cl /nologo /O2 /W4 /EHsc /MT /utf-8 /std:c++14 /DUNICODE /D_UNICODE /D_WIN32_WINNT=0x0601 /DWINVER=0x0601 /DNOMINMAX ..\tests\window.cpp /Fe:window.exe /link /SUBSYSTEM:CONSOLE,6.01 user32.lib gdi32.lib
set result=%ERRORLEVEL%
if not "%result%"=="0" goto done
cl /nologo /O2 /W4 /EHsc /MT /utf-8 /std:c++14 /DUNICODE /D_UNICODE /D_WIN32_WINNT=0x0601 /DWINVER=0x0601 /DNOMINMAX ..\tests\scale.cpp /Fe:scale.exe /link /SUBSYSTEM:CONSOLE,6.01 user32.lib gdi32.lib
set result=%ERRORLEVEL%
:done
popd
exit /b %result%
