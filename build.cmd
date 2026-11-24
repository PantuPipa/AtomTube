@echo off
setlocal
cd /d "%~dp0"
if not "%VSCMD_ARG_TGT_ARCH%"=="x86" (
  echo Execute em um x86 Native Tools Command Prompt do Visual Studio 2019 com Windows SDK.
  exit /b 1
)
if not exist build mkdir build
pushd build
rc /nologo /I . /fo atomtube.res ..\src\atomtube.rc
if errorlevel 1 (popd & exit /b 1)
cl /nologo /O2 /W4 /EHsc /MT /utf-8 /std:c++14 /DUNICODE /D_UNICODE /D_WIN32_WINNT=0x0601 /DWINVER=0x0601 /DNOMINMAX ..\src\*.cpp atomtube.res /Fe:AtomTube.exe /link /SUBSYSTEM:WINDOWS,6.01 /MACHINE:X86 /MANIFESTDEPENDENCY:"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='x86' publicKeyToken='6595b64144ccf1df' language='*'" user32.lib gdi32.lib comdlg32.lib ole32.lib oleaut32.lib mf.lib mfplat.lib mfuuid.lib evr.lib strmiids.lib psapi.lib advapi32.lib shell32.lib d3d9.lib winhttp.lib windowscodecs.lib
set result=%ERRORLEVEL%
popd
exit /b %result%
