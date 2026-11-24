// Verifica os pixels apresentados pelo GDI com o padrão RGB descrito no README.
#include "../src/stream.h"
#include <cstdio>
static Stream* currentStream=nullptr;
static LRESULT CALLBACK videoProc(HWND window,UINT message,WPARAM wp,LPARAM lp) {
    if (message==WM_PAINT) {
        PAINTSTRUCT paint; BeginPaint(window,&paint);
        if (currentStream) currentStream->repaint(window);
        EndPaint(window,&paint); return 0;
    }
    return DefWindowProcW(window,message,wp,lp);
}

static bool correctPixels(HWND window,int expectedWidth) {
    RECT rect;
    GetClientRect(window,&rect);
    POINT center={rect.right/2,rect.bottom/2};
    HWND child=ChildWindowFromPointEx(window,center,CWP_SKIPINVISIBLE);
    if (child && child!=window) return correctPixels(child,expectedWidth);
    if (rect.right!=expectedWidth || rect.bottom!=expectedWidth*9/16) return false;
    POINT points[8]; COLORREF colors[8];
    for (int col=0;col<8;col++) points[col]=POINT{rect.right*(2*col+1)/16,rect.bottom/2};
    bool captured=currentStream && currentStream->presentedPixels(points,colors,8);
    POINT origin={0,0}; ClientToScreen(window,&origin);
    HDC dc=captured ? nullptr : GetDC(nullptr);
    bool ok=true;
    for (int col=0;col<8;col++) {
        COLORREF c=captured ? colors[col] : GetPixel(dc,origin.x+points[col].x,origin.y+points[col].y);
        int r=GetRValue(c),g=GetGValue(c),b=GetBValue(c);
        if (col<2) ok=ok && r>170 && g<70 && b<70;
        else if (col<6) ok=ok && g>170 && r<70 && b<70;
        else ok=ok && b>170 && r<70 && g<70;
    }
    if (dc) ReleaseDC(nullptr,dc);
    return ok;
}
int main(int argc,char** argv) {
    if (argc!=3 && argc!=5) return 2;
    SetProcessDPIAware();
    HRESULT co=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    if (FAILED(co)) return 2;
    journal.init(false);
    WNDCLASSW cls={}; cls.hInstance=GetModuleHandleW(nullptr); cls.lpfnWndProc=videoProc;
    cls.lpszClassName=L"AtomTubeColorTest"; cls.hbrBackground=static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    RegisterClassW(&cls);
    HWND window=CreateWindowExW(WS_EX_TOPMOST|WS_EX_NOACTIVATE,cls.lpszClassName,L"AtomTube - teste RGB",
        WS_POPUP|WS_VISIBLE|WS_CLIPCHILDREN,20,20,640,360,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    Stream stream;
    currentStream=&stream;
    int result=1;
    ULONGLONG measurement=0;
    int initialFrames=0;
    try {
        if (!window) throw Problem{E_FAIL,L"Criar janela do teste"};
        PlaybackOptions settings;
        int sourceFps=30;
        if (argc==5) { settings.fps=atoi(argv[3]); sourceFps=atoi(argv[4]); }
        stream.open(window,argv[1],{},strcmp(argv[2],"cpu")==0,true,854,480,sourceFps,true,0,settings);
        ULONGLONG end=GetTickCount64()+18000;
        bool resized=false;
        while (GetTickCount64()<end) {
            MSG m;
            while (PeekMessage(&m,nullptr,0,0,PM_REMOVE)) { TranslateMessage(&m); DispatchMessage(&m); }
            StreamStats stats={};
            if (stream.stats(stats) && stats.displayedPictures>1 && stats.playedAudioBuffers>0 &&
                correctPixels(window,resized ? 480 : 640)) {
                if (resized) {
                    if (!measurement) { measurement=GetTickCount64(); initialFrames=stream.presentationFrames(); }
                    if (GetTickCount64()-measurement>=3000) {
                        int shown=stream.presentationFrames()-initialFrames;
                        int expected=3*((settings.fps && settings.fps<static_cast<unsigned>(sourceFps)) ? settings.fps : sourceFps);
                        if (shown>=expected-5 && shown<=expected+5) {
                            printf("RGB correto | %s | %d quadros em 3 s | alvo %u fps\n",argv[2],shown,settings.fps);
                            result=0;
                        }
                        break;
                    }
                } else {
                    MoveWindow(window,20,20,480,270,TRUE); stream.resize(); resized=true;
                }
            }
            if (stream.state()>=6) break;
            Sleep(40);
        }
    } catch (Problem p) { fwprintf(stderr,L"%s (0x%08lX)\n",p.operation,p.hr); }
    if (result) {
        StreamStats finalStats={}; stream.stats(finalStats);
        printf("Falha | estado %d | vídeo %d | áudio %d | apresentados %d\n",
            stream.state(),finalStats.decodedVideo,finalStats.playedAudioBuffers,stream.presentationFrames());
        journal.save(L"cores-failure.txt");
    }
    stream.close(); currentStream=nullptr; DestroyWindow(window); CoUninitialize();
    return result;
}
