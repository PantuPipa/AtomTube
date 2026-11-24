#include "stream.h"
#include <cstring>

// libVLC 3.x public C ABI (the Win32 distribution uses cdecl).
#define VLC(name, signature) symbol<signature>("libvlc_" #name)
void* Stream::lock(void* opaque,void** planes) {
    auto self=static_cast<Stream*>(opaque);
    AcquireSRWLockExclusive(&self->frameLock);
    planes[0]=self->frame;
    return self;
}
void Stream::unlock(void* opaque,void*,void* const*) {
    auto self=static_cast<Stream*>(opaque);
    self->frameReady=true;
    ReleaseSRWLockExclusive(&self->frameLock);
}
void Stream::display(void* opaque,void*) {
    auto self=static_cast<Stream*>(opaque);
    if (self->limitPresentation) {
        LARGE_INTEGER now; QueryPerformanceCounter(&now);
        if (self->nextPresentation.QuadPart && now.QuadPart<self->nextPresentation.QuadPart) return;
        if (!self->nextPresentation.QuadPart) self->nextPresentation=now;
        const LONGLONG period=self->clockFrequency.QuadPart/self->targetFps;
        self->nextPresentation.QuadPart+=period*(1+(now.QuadPart-self->nextPresentation.QuadPart)/period);
    }
    InterlockedIncrement(&self->presentedFrames);
    InvalidateRect(self->output,nullptr,FALSE);
}
intptr_t Stream::read(void* opaque,unsigned char* bytes,size_t length) {
    auto self=static_cast<Stream*>(opaque);
    DWORD got=0;
    if (ReadFile(self->pipe,bytes,length,&got,nullptr)) return got;
    if (GetLastError()==ERROR_BROKEN_PIPE) return 0;
    return -1;
}
void Stream::releasePresenterSurfaces() {
    if (presenterVideo) { presenterVideo->Release(); presenterVideo=nullptr; }
    if (presenterUpload) { presenterUpload->Release(); presenterUpload=nullptr; }
}
void Stream::releasePresenter() {
    releasePresenterSurfaces();
    if (presenterDevice) { presenterDevice->Release(); presenterDevice=nullptr; }
    if (presenterD3d) { presenterD3d->Release(); presenterD3d=nullptr; }
    ZeroMemory(&presenterParameters,sizeof(presenterParameters));
}
bool Stream::createPresenter() {
    releasePresenter();
    presenterD3d=Direct3DCreate9(D3D_SDK_VERSION);
    if (!presenterD3d) return false;
    D3DCAPS9 caps={};
    if (FAILED(presenterD3d->GetDeviceCaps(D3DADAPTER_DEFAULT,D3DDEVTYPE_HAL,&caps)) ||
        !(caps.StretchRectFilterCaps&D3DPTFILTERCAPS_MINFLINEAR) ||
        !(caps.StretchRectFilterCaps&D3DPTFILTERCAPS_MAGFLINEAR)) {
        releasePresenter(); return false;
    }
    presenterParameters.Windowed=TRUE;
    presenterParameters.SwapEffect=D3DSWAPEFFECT_DISCARD;
    presenterParameters.hDeviceWindow=output;
    presenterParameters.BackBufferFormat=D3DFMT_UNKNOWN;
    presenterParameters.PresentationInterval=D3DPRESENT_INTERVAL_IMMEDIATE;
    HRESULT hr=presenterD3d->CreateDevice(D3DADAPTER_DEFAULT,D3DDEVTYPE_HAL,output,
        D3DCREATE_SOFTWARE_VERTEXPROCESSING|D3DCREATE_FPU_PRESERVE,&presenterParameters,&presenterDevice);
    if (FAILED(hr)) { releasePresenter(); return false; }
    hr=presenterDevice->CreateOffscreenPlainSurface(frameWidth,frameHeight,D3DFMT_X8R8G8B8,
        D3DPOOL_SYSTEMMEM,&presenterUpload,nullptr);
    if (SUCCEEDED(hr)) hr=presenterDevice->CreateOffscreenPlainSurface(frameWidth,frameHeight,D3DFMT_X8R8G8B8,
        D3DPOOL_DEFAULT,&presenterVideo,nullptr);
    if (FAILED(hr)) { releasePresenter(); return false; }
    IDirect3DSurface9* back=nullptr;
    D3DSURFACE_DESC desc={};
    if (SUCCEEDED(presenterDevice->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&back))) {
        if (SUCCEEDED(back->GetDesc(&desc)))
            journal.add(fmt(L"Apresentador D3D9 | backbuffer %u × %u",desc.Width,desc.Height));
        back->Release();
    }
    return true;
}
bool Stream::resetPresenter() {
    if (!presenterDevice) return false;
    releasePresenterSurfaces();
    presenterParameters.BackBufferWidth=0;
    presenterParameters.BackBufferHeight=0;
    presenterParameters.BackBufferFormat=D3DFMT_UNKNOWN;
    if (FAILED(presenterDevice->Reset(&presenterParameters))) return false;
    HRESULT hr=presenterDevice->CreateOffscreenPlainSurface(frameWidth,frameHeight,D3DFMT_X8R8G8B8,
        D3DPOOL_SYSTEMMEM,&presenterUpload,nullptr);
    if (SUCCEEDED(hr)) hr=presenterDevice->CreateOffscreenPlainSurface(frameWidth,frameHeight,D3DFMT_X8R8G8B8,
        D3DPOOL_DEFAULT,&presenterVideo,nullptr);
    return SUCCEEDED(hr);
}
bool Stream::repaintPresenter() {
    if (!presenterDevice || !presenterUpload || !presenterVideo) return false;
    HRESULT state=presenterDevice->TestCooperativeLevel();
    if (state==D3DERR_DEVICENOTRESET) {
        if (!resetPresenter()) return false;
    } else if (FAILED(state)) return false;
    D3DLOCKED_RECT target={};
    if (FAILED(presenterUpload->LockRect(&target,nullptr,0))) return false;
    AcquireSRWLockShared(&frameLock);
    bool ready=frame && frameReady;
    if (ready) {
        size_t rowBytes=static_cast<size_t>(frameWidth)*4;
        for (unsigned y=0;y<frameHeight;++y)
            memcpy(static_cast<unsigned char*>(target.pBits)+static_cast<size_t>(target.Pitch)*y,
                frame+static_cast<size_t>(framePitch)*y,rowBytes);
    }
    ReleaseSRWLockShared(&frameLock);
    presenterUpload->UnlockRect();
    if (!ready || FAILED(presenterDevice->UpdateSurface(presenterUpload,nullptr,presenterVideo,nullptr))) return false;
    IDirect3DSurface9* back=nullptr;
    if (FAILED(presenterDevice->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&back))) return false;
    RECT client; GetClientRect(output,&client);
    int cw=client.right, ch=client.bottom;
    int dw=cw, dh=static_cast<int>((static_cast<ULONGLONG>(cw)*frameHeight)/frameWidth);
    if (dh>ch) { dh=ch; dw=static_cast<int>((static_cast<ULONGLONG>(ch)*frameWidth)/frameHeight); }
    RECT source={0,0,static_cast<LONG>(frameWidth),static_cast<LONG>(frameHeight)};
    RECT destination={(cw-dw)/2,(ch-dh)/2,(cw+dw)/2,(ch+dh)/2};
    HRESULT hr=presenterDevice->ColorFill(back,nullptr,D3DCOLOR_XRGB(0,0,0));
    if (SUCCEEDED(hr)) hr=presenterDevice->StretchRect(presenterVideo,&source,back,&destination,
        smoothEdges ? D3DTEXF_LINEAR : D3DTEXF_POINT);
    back->Release();
    if (FAILED(hr)) return false;
    return SUCCEEDED(presenterDevice->Present(nullptr,nullptr,nullptr,nullptr));
}
void Stream::connect(const wstring& exe,const std::string& video,const std::string& audio,LONGLONG offset) {
    // Resolver output is an ASCII HTTPS URL. It is passed to CreateProcess, never to a shell.
    auto input=[offset](const std::string& url) {
        if (url.find_first_of("\"\r\n\\")!=std::string::npos) throw Problem{E_INVALIDARG,L"Endereço de mídia inválido"};
        wstring options=L" -readrate 1 -rw_timeout 15000000 -reconnect 1 -reconnect_on_network_error 1"
            L" -reconnect_on_http_error 429,5xx -reconnect_delay_max 3";
        if (url.compare(0,8,"https://")==0) options+=L" -tls_verify 1";
        if (offset>0) options+=fmt(L" -ss %lld.%03lld",offset/1000,offset%1000);
        return options+L" -i \""+wstring(url.begin(),url.end())+L"\"";
    };
    wstring cmd=L"\""+exe+L"\" -nostdin -hide_banner -loglevel error"+input(video);
    if (!audio.empty()) cmd+=input(audio);
    cmd+=audio.empty() ? L" -map 0:v:0 -map 0:a:0" : L" -map 0:v:0 -map 1:a:0";
    cmd+=L" -c copy -muxdelay 0 -muxpreload 0 -flush_packets 1 -f mpegts pipe:1";
    SECURITY_ATTRIBUTES sa={sizeof(sa),nullptr,TRUE};
    HANDLE writer=nullptr;
    if (!CreatePipe(&pipe,&writer,&sa,65536)) throw Problem{HRESULT_FROM_WIN32(GetLastError()),L"Criar canal do streaming"};
    HANDLE nul=CreateFileW(L"NUL",GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,&sa,OPEN_EXISTING,0,nullptr);
    PROCESS_INFORMATION pi={};
    STARTUPINFOW si={}; si.cb=sizeof(si); si.dwFlags=STARTF_USESTDHANDLES;
    si.hStdOutput=writer; si.hStdInput=nul; si.hStdError=nul;
    job=CreateJobObjectW(nullptr,nullptr);
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits={};
    limits.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    bool ok=nul!=INVALID_HANDLE_VALUE && job && SetHandleInformation(pipe,HANDLE_FLAG_INHERIT,0) &&
        SetInformationJobObject(job,JobObjectExtendedLimitInformation,&limits,sizeof(limits)) &&
        CreateProcessW(exe.c_str(),&cmd[0],nullptr,nullptr,TRUE,CREATE_NO_WINDOW|CREATE_SUSPENDED,nullptr,nullptr,&si,&pi) &&
        AssignProcessToJobObject(job,pi.hProcess) && ResumeThread(pi.hThread)!=DWORD(-1);
    DWORD error=GetLastError();
    CloseHandle(writer); if (nul!=INVALID_HANDLE_VALUE) CloseHandle(nul);
    if (pi.hThread) CloseHandle(pi.hThread);
    transfer=pi.hProcess;
    if (!ok) {
        if (transfer) TerminateProcess(transfer,1);
        throw Problem{HRESULT_FROM_WIN32(error),L"Iniciar transferência das faixas"};
    }
}
void Stream::open(HWND window,const std::string& video,const std::string& audio,bool cpu,bool rgbOutput,
                  unsigned width,unsigned height,double sourceFps,bool smooth,LONGLONG offset,PlaybackOptions settings,const wchar_t* localPath) {
    close();
    wchar_t module[MAX_PATH];
    DWORD n=GetModuleFileNameW(nullptr,module,MAX_PATH);
    if (!n || n>=MAX_PATH) throw Problem{E_FAIL,L"Localizar biblioteca de streaming"};
    wstring path(module);
    path=path.substr(0,path.find_last_of(L'\\'))+L"\\youtube\\vlc\\libvlc.dll";
    dll=LoadLibraryExW(path.c_str(),nullptr,LOAD_WITH_ALTERED_SEARCH_PATH);
    if (!dll) throw Problem{HRESULT_FROM_WIN32(GetLastError()),L"Carregar biblioteca de streaming x86"};
    std::vector<const char*> args={"--ignore-config","--no-video-title-show","--no-osd","--quiet","--network-caching=1500",
                         cpu ? "--avcodec-hw=none" : "--avcodec-hw=dxva2","--no-spu","--no-media-library"};
    if (settings.image==ImageQuality::High) {
        args.push_back("--video-filter=sharpen"); args.push_back("--sharpen-sigma=0.12");
    }
    if (settings.image==ImageQuality::Low) args.push_back("--avcodec-skiploopfilter=1");
    if (settings.image==ImageQuality::Potato) {
        args.push_back("--avcodec-skiploopfilter=4"); args.push_back("--avcodec-fast");
        args.push_back("--avcodec-skip-idct=1"); args.push_back("--swscale-mode=4");
    }
    targetFps=settings.fps;
    limitPresentation=targetFps && sourceFps>targetFps+0.01;
    rgbOutput=rgbOutput || limitPresentation;
    memoryOutput=rgbOutput; output=window; smoothEdges=smooth;
    presentedFrames=0; nextPresentation.QuadPart=0; QueryPerformanceFrequency(&clockFrequency);
    journal.add(rgbOutput ? L"Saída de vídeo solicitada | buffer RGB32 do AtomTube" : L"Saída de vídeo solicitada | Direct3D 9");
    instance=VLC(new,void*(*)(int,const char* const*))(static_cast<int>(args.size()),args.data());
    if (!instance) throw Problem{E_FAIL,L"Iniciar biblioteca de streaming"};
    wstring exe=wstring(module).substr(0,wstring(module).find_last_of(L'\\'))+L"\\youtube\\ffmpeg\\ffmpeg.exe";
    if (localPath) {
        int len=WideCharToMultiByte(CP_UTF8,0,localPath,-1,nullptr,0,nullptr,nullptr);
        std::string pathUtf8(len,'\0');
        if (len && WideCharToMultiByte(CP_UTF8,0,localPath,-1,&pathUtf8[0],len,nullptr,nullptr))
            media=VLC(media_new_path,void*(*)(void*,const char*))(instance,pathUtf8.c_str());
    } else {
        connect(exe,video,audio,offset);
        using ReadCallback=intptr_t(*)(void*,unsigned char*,size_t);
        media=VLC(media_new_callbacks,void*(*)(void*,void*,ReadCallback,void*,void*,void*))(instance,nullptr,read,nullptr,nullptr,this);
    }
    if (!media) throw Problem{E_FAIL,L"Abrir endereço de mídia"};
    if (!localPath) VLC(media_add_option,void(*)(void*,const char*))(media,":demux=ts");
    player=VLC(media_player_new_from_media,void*(*)(void*))(media);
    if (!player) throw Problem{E_FAIL,L"Criar player de streaming"};
    if (rgbOutput) {
        if (!width || !height || width>8192 || height>8192 || static_cast<ULONGLONG>(width)*height>16777216)
            throw Problem{E_INVALIDARG,L"Dimensões inválidas para o buffer RGB"};
        frameWidth=width; frameHeight=height; framePitch=(width*4+31)&~31u;
        frame=static_cast<unsigned char*>(VirtualAlloc(nullptr,
            static_cast<SIZE_T>(framePitch)*frameHeight,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));
        if (!frame) throw Problem{E_OUTOFMEMORY,L"Reservar buffer RGB"};
        frameReady=false;
        using Lock=void*(*)(void*,void**);
        using Unlock=void(*)(void*,void*,void* const*);
        using Display=void(*)(void*,void*);
        VLC(video_set_callbacks,void(*)(void*,Lock,Unlock,Display,void*))(player,lock,unlock,display,this);
        VLC(video_set_format,void(*)(void*,const char*,unsigned,unsigned,unsigned))(
            player,"RV32",frameWidth,frameHeight,framePitch);
        journal.add(fmt(L"Buffer RGB32 | %u × %u | passo %u bytes",frameWidth,frameHeight,framePitch));
        bool direct3d=smoothEdges && createPresenter();
        journal.add(direct3d ? (smoothEdges ? L"Escala RGB32 | Direct3D 9 linear sobre o buffer alinhado" :
            L"Escala RGB32 | Direct3D 9 sem suavização sobre o buffer alinhado") :
            (smoothEdges ? L"Escala RGB32 | fallback GDI HALFTONE" : L"Escala RGB32 | fallback GDI sem suavização"));
        if (limitPresentation) journal.add(fmt(L"Apresentação limitada a %u fps | origem %.3f fps",targetFps,sourceFps));
    } else VLC(media_player_set_hwnd,void(*)(void*,void*))(player,window);
    VLC(video_set_key_input,void(*)(void*,unsigned))(player,0);
    VLC(video_set_mouse_input,void(*)(void*,unsigned))(player,0);
    if (VLC(media_player_play,int(*)(void*))(player)!=0) throw Problem{E_FAIL,L"Iniciar streaming"};
}
void Stream::close() {
    if (job) { CloseHandle(job); job=nullptr; }
    if (transfer) { WaitForSingleObject(transfer,2000); CloseHandle(transfer); transfer=nullptr; }
    if (player) {
        VLC(media_player_stop,void(*)(void*))(player);
        VLC(media_player_release,void(*)(void*))(player); player=nullptr;
    }
    if (media) { VLC(media_release,void(*)(void*))(media); media=nullptr; }
    if (instance) { VLC(release,void(*)(void*))(instance); instance=nullptr; }
    if (dll) { FreeLibrary(dll); dll=nullptr; }
    if (pipe) { CloseHandle(pipe); pipe=nullptr; }
    releasePresenter();
    AcquireSRWLockExclusive(&frameLock);
    if (frame) VirtualFree(frame,0,MEM_RELEASE);
    frame=nullptr; frameReady=false;
    ReleaseSRWLockExclusive(&frameLock);
    output=nullptr; memoryOutput=false;
    limitPresentation=false; presentedFrames=0; nextPresentation.QuadPart=0;
}
int Stream::state() { return VLC(media_player_get_state,int(*)(void*))(player); }
bool Stream::stats(StreamStats& value) { return VLC(media_get_stats,int(*)(void*,StreamStats*))(media,&value)!=0; }
DWORD Stream::transferCode() const {
    DWORD code=0;
    return transfer && GetExitCodeProcess(transfer,&code) ? code : STILL_ACTIVE;
}
void Stream::size(unsigned& w,unsigned& h) {
    VLC(video_get_size,int(*)(void*,unsigned,unsigned*,unsigned*))(player,0,&w,&h);
}
int Stream::audioTrack() { return VLC(audio_get_track,int(*)(void*))(player); }
void Stream::pause(bool paused) { VLC(media_player_set_pause,void(*)(void*,int))(player,paused); }
LONGLONG Stream::position() { return VLC(media_player_get_time,LONGLONG(*)(void*))(player); }
void Stream::seek(LONGLONG ms) { VLC(media_player_set_time,void(*)(void*,LONGLONG))(player,ms); }
void Stream::replay() {
    VLC(media_player_stop,void(*)(void*))(player);
    if (VLC(media_player_play,int(*)(void*))(player)!=0) throw Problem{E_FAIL,L"Iniciar reprodução"};
}
void Stream::volume(int percent) { VLC(audio_set_volume,int(*)(void*,int))(player,percent); }
void Stream::setSmoothing(bool enabled) {
    if (smoothEdges==enabled) return;
    smoothEdges=enabled;
    if (enabled && memoryOutput && !presenterDevice)
        journal.add(createPresenter() ? L"Suavização de bordas ativada | Direct3D 9 linear" :
            L"Suavização de bordas ativada | fallback GDI HALFTONE");
    else journal.add(enabled ? L"Suavização de bordas ativada" :
        L"Suavização de bordas desativada | caminho GDI rápido restaurado");
    if (output) InvalidateRect(output,nullptr,FALSE);
}
void Stream::resize() {
    if (presenterDevice && !resetPresenter()) releasePresenter();
}
bool Stream::presentedPixels(const POINT* clientPoints,COLORREF* colors,unsigned count) {
    if (!presenterDevice || !presenterD3d || !clientPoints || !colors || !count) return false;
    D3DDISPLAYMODE mode={};
    if (FAILED(presenterD3d->GetAdapterDisplayMode(D3DADAPTER_DEFAULT,&mode))) return false;
    IDirect3DSurface9* capture=nullptr;
    HRESULT hr=presenterDevice->CreateOffscreenPlainSurface(mode.Width,mode.Height,D3DFMT_A8R8G8B8,
        D3DPOOL_SYSTEMMEM,&capture,nullptr);
    if (SUCCEEDED(hr)) hr=presenterDevice->GetFrontBufferData(0,capture);
    D3DLOCKED_RECT locked={};
    if (SUCCEEDED(hr)) hr=capture->LockRect(&locked,nullptr,D3DLOCK_READONLY);
    if (SUCCEEDED(hr)) {
        for (unsigned i=0;i<count;++i) {
            POINT screen=clientPoints[i]; ClientToScreen(output,&screen);
            if (screen.x<0 || screen.y<0 || static_cast<unsigned>(screen.x)>=mode.Width ||
                static_cast<unsigned>(screen.y)>=mode.Height) { hr=E_INVALIDARG; break; }
            DWORD pixel=*reinterpret_cast<DWORD*>(static_cast<unsigned char*>(locked.pBits)+
                static_cast<size_t>(locked.Pitch)*screen.y+static_cast<size_t>(screen.x)*4);
            colors[i]=RGB((pixel>>16)&0xff,(pixel>>8)&0xff,pixel&0xff);
        }
        capture->UnlockRect();
    }
    if (capture) capture->Release();
    return SUCCEEDED(hr);
}
void Stream::repaint(HWND window) {
    if (!memoryOutput) return;
    if (smoothEdges && repaintPresenter()) return;
    AcquireSRWLockShared(&frameLock);
    if (frame && frameReady && frameWidth && frameHeight) {
        RECT client; GetClientRect(window,&client);
        int cw=client.right, ch=client.bottom;
        int dw=cw, dh=static_cast<int>((static_cast<ULONGLONG>(cw)*frameHeight)/frameWidth);
        if (dh>ch) { dh=ch; dw=static_cast<int>((static_cast<ULONGLONG>(ch)*frameWidth)/frameHeight); }
        int x=(cw-dw)/2, y=(ch-dh)/2;
        HDC dc=GetDC(window);
        if (x) { RECT left={0,0,x,ch},right={x+dw,0,cw,ch}; FillRect(dc,&left,static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH))); FillRect(dc,&right,static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH))); }
        if (y) { RECT top={0,0,cw,y},bottom={0,y+dh,cw,ch}; FillRect(dc,&top,static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH))); FillRect(dc,&bottom,static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH))); }
        BITMAPINFO info={}; info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth=framePitch/4; info.bmiHeader.biHeight=-static_cast<LONG>(frameHeight);
        info.bmiHeader.biPlanes=1; info.bmiHeader.biBitCount=32; info.bmiHeader.biCompression=BI_RGB;
        bool resized=dw!=static_cast<int>(frameWidth) || dh!=static_cast<int>(frameHeight);
        SetStretchBltMode(dc,resized && smoothEdges ? HALFTONE : COLORONCOLOR);
        if (resized && smoothEdges) SetBrushOrgEx(dc,0,0,nullptr);
        StretchDIBits(dc,x,y,dw,dh,0,0,frameWidth,frameHeight,frame,&info,DIB_RGB_COLORS,SRCCOPY);
        ReleaseDC(window,dc);
    }
    ReleaseSRWLockShared(&frameLock);
}

