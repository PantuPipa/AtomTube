#pragma once
#define PSAPI_VERSION 1
#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mferror.h>
#include <mftransform.h>
#include <evr.h>
#include <dshow.h>
#include <wrl/client.h>
#include <string>
#include <deque>
#include <cstdarg>
using Microsoft::WRL::ComPtr;
using std::wstring;
wstring fmt(const wchar_t* format, ...);
wstring stamp(bool filename = false);
struct VideoAdapter {
    wstring description=L"N/D";
    bool rgbOutput=false;
};
const VideoAdapter& defaultVideoAdapter();
class Journal {
    wstring header, folder;
    std::deque<wstring> entries;
    HANDLE file = INVALID_HANDLE_VALUE;
    void rotate();
public:
    ~Journal();
    void init(bool automatic = true);
    void add(const wstring& line);
    bool save(const wchar_t* path) const;
};
extern Journal journal;
struct Problem { HRESULT hr; const wchar_t* operation; };
inline void check(HRESULT hr, const wchar_t* operation) {
    if (FAILED(hr)) throw Problem{hr, operation};
}
class Resources {
    ULONGLONG wall = 0, process = 0, total = 0, idle = 0;
public:
    wstring sample();
};
