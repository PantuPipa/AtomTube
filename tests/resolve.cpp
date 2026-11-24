#include "../src/youtube.h"
#include <cstdio>

int wmain(int argc,wchar_t** argv) {
    if (argc!=3 && argc!=6) return 2;
    HRESULT co=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    if (FAILED(co)) return 2;
    journal.init(false);
    YoutubeDownload download;
    int result=1;
    try {
        unsigned requested=_wtoi(argv[2]);
        PlaybackOptions settings;
        if (argc==6) {
            settings.image=static_cast<ImageQuality>(_wtoi(argv[3]));
            settings.fps=_wtoi(argv[4]); settings.language=static_cast<Language>(_wtoi(argv[5]));
        }
        download.start(youtubeVideoId(argv[1]),requested,true,settings);
        ULONGLONG deadline=GetTickCount64()+90000;
        while (!download.poll()) {
            if (!download.active()) throw Problem{E_FAIL,L"Resolver formatos"};
            if (GetTickCount64()>deadline) throw Problem{HRESULT_FROM_WIN32(WAIT_TIMEOUT),L"Prazo da consulta"};
            Sleep(50);
        }
        wprintf(L"%u x %u | %.3f fps | limite %u\n",download.videoWidth,download.videoHeight,download.videoFps,requested);
        wprintf(L"Audio: %s\n",download.audioLanguage.c_str());
        result=requested==600 ? (download.videoWidth<=1280 && download.videoHeight<=720 ? 0 : 1) :
            (download.videoHeight==requested ? 0 : 1);
    } catch (Problem p) {
        fwprintf(stderr,L"%s (0x%08lX)\n",p.operation,p.hr);
    }
    download.clear(); CoUninitialize(); return result;
}
