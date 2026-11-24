// Teste de integração local. Usa os mesmos caminhos de abertura, eventos e medição da janela.
#include "../src/player.h"
#include "../src/youtube.h"
#include <cstdio>
static void until(Player& p, State target, DWORD timeout) {
    ULONGLONG end=GetTickCount64()+timeout;
    while (p.state!=target) {
        MSG m;
        while (PeekMessage(&m,nullptr,0,0,PM_REMOVE)) { TranslateMessage(&m); DispatchMessage(&m); }
        p.poll();
        if (GetTickCount64()>end) throw Problem{HRESULT_FROM_WIN32(WAIT_TIMEOUT),L"Prazo do teste"};
        Sleep(15);
    }
}
static void testYoutubeInput() {
    const wstring id=L"HOfdboHvshg";
    for (auto link:{L"https://youtu.be/HOfdboHvshg?si=abc",L"www.youtube.com/watch?list=x&v=HOfdboHvshg",
                    L" https://m.youtube.com/shorts/HOfdboHvshg "})
        if (youtubeVideoId(link)!=id) throw Problem{E_FAIL,L"Interpretar link de vídeo"};
    for (auto link:{L"https://youtube.com.example/watch?v=HOfdboHvshg",L"https://youtu.be/../file",
                    L"https://youtube.com/playlist?list=HOfdboHvshg",L"https://youtu.be/HOfdboHvshg-extra"})
        if (!youtubeVideoId(link).empty()) throw Problem{E_FAIL,L"Rejeitar link inválido"};
}
int wmain(int argc,wchar_t** argv) {
    if (argc<4) { fputws(L"Uso: playback.exe arquivo-ou-link dxva|cpu log.txt [completo|youtube]\n",stderr); return 2; }
    YoutubeDownload download;
    Player p;
    HRESULT co=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    if (FAILED(co)) return 2;
    HRESULT mf=MFStartup(MF_VERSION);
    if (FAILED(mf)) { CoUninitialize(); return 2; }
    journal.init(false);
    p.video=CreateWindowW(L"STATIC",L"",WS_POPUP,0,0,640,360,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    int result=0;
    try {
        testYoutubeInput();
        if (!p.video) throw Problem{HRESULT_FROM_WIN32(GetLastError()),L"Criar superfície de teste"};
        bool completeMp4=argc>4 && wcscmp(argv[4],L"youtube-mp4")==0;
        bool online=completeMp4 || (argc>4 && wcscmp(argv[4],L"youtube")==0);
        wstring path=argv[1];
        if (online) {
            unsigned height=argc>5 ? _wtoi(argv[5]) : 720;
            download.start(youtubeVideoId(path),height,!completeMp4);
            ULONGLONG deadline=GetTickCount64()+180000;
            while (!download.poll()) {
                if (!download.active()) throw Problem{E_FAIL,L"Preparar vídeo online"};
                if (GetTickCount64()>deadline) throw Problem{HRESULT_FROM_WIN32(WAIT_TIMEOUT),L"Prazo do ensaio online"};
                Sleep(100);
            }
            path=download.videoPath();
        }
        bool cpu=wcscmp(argv[2],L"cpu")==0;
        if (online && !completeMp4) p.openStream(download.videoUrl,download.audioUrl,cpu,download.startTime(),
            download.videoWidth,download.videoHeight,download.videoFps);
        else p.open(path.c_str(),cpu);
        until(p,State::Playing,65000);
        bool full=online || (argc>4 && wcscmp(argv[4],L"completo")==0);
        if (!full || online) {
            p.toggle(); until(p,State::Paused,5000);
            p.toggle(); until(p,State::Playing,5000);
        }
        ULONGLONG playing=GetTickCount64();
        until(p,State::Ended,full ? 330000 : 25000);
        if (online && !completeMp4 && (download.durationSeconds==0 || GetTickCount64()-playing+3000<download.durationSeconds*1000ULL))
            throw Problem{E_FAIL,L"Fim prematuro da transmissão no ensaio"};
        if (!full) { p.toggle(); until(p,State::Playing,5000); }
    } catch (Problem problem) {
        p.fail(problem); fwprintf(stderr,L"%s\n",p.status.c_str()); result=1;
    }
    p.close();
    download.clear();
    if (!journal.save(argv[3])) result=3;
    DestroyWindow(p.video);
    MFShutdown(); CoUninitialize();
    return result;
}
