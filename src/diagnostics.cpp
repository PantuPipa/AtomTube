#include "common.h"
#include <shlobj.h>
#include <psapi.h>
#include <d3d9.h>
#include <vector>
Journal journal;
wstring fmt(const wchar_t* format, ...) {
    va_list args;
    va_start(args, format);
    int n = _vscwprintf(format, args);
    va_end(args);
    if (n < 0) return L"N/D";
    std::vector<wchar_t> out(n + 1);
    va_start(args, format);
    vswprintf_s(out.data(), out.size(), format, args);
    va_end(args);
    return out.data();
}
wstring stamp(bool filename) {
    SYSTEMTIME t; GetLocalTime(&t);
    return fmt(filename ? L"%04u-%02u-%02u_%02u-%02u-%02u" : L"%04u-%02u-%02u %02u:%02u:%02u.%03u",
        t.wYear,t.wMonth,t.wDay,t.wHour,t.wMinute,t.wSecond,t.wMilliseconds);
}
static bool writeText(HANDLE h, const wstring& s) {
    DWORD done = 0, bytes = static_cast<DWORD>(s.size()*sizeof(wchar_t));
    return WriteFile(h,s.data(),bytes,&done,nullptr) && done == bytes;
}
Journal::~Journal() { if (file != INVALID_HANDLE_VALUE) CloseHandle(file); }
void Journal::rotate() {
    if (file != INVALID_HANDLE_VALUE) { CloseHandle(file); file = INVALID_HANDLE_VALUE; }
    wstring name = folder + L"\\ultima.txt";
    MoveFileExW(name.c_str(), (folder+L"\\anterior.txt").c_str(), MOVEFILE_REPLACE_EXISTING);
    file = CreateFileW(name.c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    if (file != INVALID_HANDLE_VALUE) writeText(file,L"\uFEFF" + header);
}
static VideoAdapter detectVideoAdapter() {
    VideoAdapter adapter;
    ComPtr<IDirect3D9> d3d; d3d.Attach(Direct3DCreate9(D3D_SDK_VERSION));
    D3DADAPTER_IDENTIFIER9 gpu={};
    if (d3d && SUCCEEDED(d3d->GetAdapterIdentifier(D3DADAPTER_DEFAULT,0,&gpu))) {
        adapter.description=fmt(L"%hs | PCI %04X:%04X | driver %u.%u.%u.%u",gpu.Description,
            gpu.VendorId,gpu.DeviceId,HIWORD(gpu.DriverVersion.HighPart),LOWORD(gpu.DriverVersion.HighPart),
            HIWORD(gpu.DriverVersion.LowPart),LOWORD(gpu.DriverVersion.LowPart));
        // Intel Cedar Trail display functions, including GMA 3600 and 3650.
        adapter.rgbOutput=gpu.VendorId==0x8086 && ((gpu.DeviceId>=0x0be0 && gpu.DeviceId<=0x0bef) ||
            strstr(gpu.Description,"Graphics Media Accelerator 3600")!=nullptr);
    }
    return adapter;
}
const VideoAdapter& defaultVideoAdapter() {
    static const VideoAdapter adapter=detectVideoAdapter();
    return adapter;
}
void Journal::init(bool automatic) {
    SYSTEM_INFO sys; GetNativeSystemInfo(&sys);
    OSVERSIONINFOEXW os = {}; os.dwOSVersionInfoSize=sizeof(os);
    using RtlVersion = LONG(WINAPI*)(OSVERSIONINFOW*);
    auto version = reinterpret_cast<RtlVersion>(GetProcAddress(GetModuleHandleW(L"ntdll"),"RtlGetVersion"));
    wstring windows=L"N/D";
    if (version && version(reinterpret_cast<OSVERSIONINFOW*>(&os))==0)
        windows=fmt(L"%lu.%lu build %lu SP%u",os.dwMajorVersion,os.dwMinorVersion,os.dwBuildNumber,os.wServicePackMajor);
    wchar_t cpu[256] = L"N/D"; DWORD bytes = sizeof(cpu);
    RegGetValueW(HKEY_LOCAL_MACHINE,L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
                 L"ProcessorNameString",RRF_RT_REG_SZ,nullptr,cpu,&bytes);
    MEMORYSTATUSEX mem = {}; mem.dwLength=sizeof(mem);
    wstring ram=GlobalMemoryStatusEx(&mem) ? fmt(L"%llu MiB",mem.ullTotalPhys>>20) : L"N/D";
    header=fmt(L"AtomTube 0.9.2 | diagnóstico de sessão | aplicativo x86\r\nWindows %s | arquitetura SO %s\r\nCPU %s | %lu lógicos | RAM %s\r\nAdaptador D3D9 padrão %s\r\n",
        windows.c_str(),sys.wProcessorArchitecture==PROCESSOR_ARCHITECTURE_INTEL ? L"x86" : sys.wProcessorArchitecture==PROCESSOR_ARCHITECTURE_AMD64 ? L"x64" : L"outra",
        cpu,sys.dwNumberOfProcessors,ram.c_str(),defaultVideoAdapter().description.c_str());
    wchar_t dir[MAX_PATH];
    if (automatic && SUCCEEDED(SHGetFolderPathW(nullptr,CSIDL_LOCAL_APPDATA,nullptr,SHGFP_TYPE_CURRENT,dir))) {
        folder=wstring(dir)+L"\\AtomTube";
        CreateDirectoryW(folder.c_str(),nullptr);
        rotate();
    }
    add(automatic && file==INVALID_HANDLE_VALUE ? L"Log automático indisponível. Exportação manual disponível." : L"Sessão iniciada.");
}
void Journal::add(const wstring& line) {
    wstring s=L"["+stamp()+L"] "+line+L"\r\n";
    if (entries.size()>=2048) entries.pop_front();
    entries.push_back(s);
    if (file != INVALID_HANDLE_VALUE) {
        if (GetFileSize(file,nullptr)>1024*1024) rotate();
        if (file != INVALID_HANDLE_VALUE) writeText(file,s);
    }
}
bool Journal::save(const wchar_t* path) const {
    HANDLE h=CreateFileW(path,GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    if (h==INVALID_HANDLE_VALUE) return false;
    bool ok=writeText(h,L"\uFEFF"+header);
    for (const auto& s:entries) if (ok) ok=writeText(h,s);
    ok = ok && FlushFileBuffers(h);
    return CloseHandle(h) && ok;
}
static ULONGLONG ticks(FILETIME f) {
    return (static_cast<ULONGLONG>(f.dwHighDateTime)<<32) | f.dwLowDateTime;
}
wstring Resources::sample() {
    FILETIME creation,exit,kernel,user,i,k,u;
    ULONGLONG now=GetTickCount64();
    SYSTEM_INFO sys; GetSystemInfo(&sys);
    wstring cpu=L"N/D", system=L"N/D",ram=L"N/D";
    if (GetProcessTimes(GetCurrentProcess(),&creation,&exit,&kernel,&user) && GetSystemTimes(&i,&k,&u)) {
        ULONGLONG p=ticks(kernel)+ticks(user), t=ticks(k)+ticks(u), id=ticks(i);
        if (wall && now>wall && t>total) {
            cpu=fmt(L"%.1f%%",100.0*(p-process)/((now-wall)*10000.0*sys.dwNumberOfProcessors));
            system=fmt(L"%.1f%%",100.0*(1.0-double(id-idle)/double(t-total)));
        }
        wall=now; process=p; total=t; idle=id;
    }
    PROCESS_MEMORY_COUNTERS pm = {}; pm.cb=sizeof(pm);
    if (GetProcessMemoryInfo(GetCurrentProcess(),&pm,sizeof(pm))) ram=fmt(L"%.1f MiB",pm.WorkingSetSize/1048576.0);
    return L"CPU app "+cpu+L" | CPU sistema "+system+L" | RAM app "+ram;
}
