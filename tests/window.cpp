#include <windows.h>
#include <cstdio>
#include <string>

static std::wstring caption(HWND control) {
    wchar_t value[1024]={};
    SendMessageW(control,WM_GETTEXT,_countof(value),reinterpret_cast<LPARAM>(value));
    return value;
}
static DWORD controlPixels(HWND control) {
    RECT r; GetClientRect(control,&r);
    BITMAPINFO info={};
    info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth=r.right; info.bmiHeader.biHeight=r.bottom;
    info.bmiHeader.biPlanes=1; info.bmiHeader.biBitCount=32;
    HDC source=GetDC(control), copy=CreateCompatibleDC(source);
    void* pixels=nullptr;
    HBITMAP bitmap=CreateDIBSection(copy,&info,DIB_RGB_COLORS,&pixels,nullptr,0);
    DWORD hash=0;
    if (bitmap) {
        auto old=SelectObject(copy,bitmap);
        if (BitBlt(copy,0,0,r.right,r.bottom,source,0,0,SRCCOPY)) {
            GdiFlush(); hash=2166136261u;
            auto colors=static_cast<const DWORD*>(pixels);
            for (int i=0;i<r.right*r.bottom;++i) hash=(hash^(colors[i]&0xffffff))*16777619u;
        }
        SelectObject(copy,old); DeleteObject(bitmap);
    }
    DeleteDC(copy); ReleaseDC(control,source); return hash;
}
static bool resizeTest(HWND main) {
    const int ids[]={110,106,101,109,100};
    DWORD before[_countof(ids)]={};
    SetWindowPos(main,nullptr,0,0,1024,560,SWP_NOZORDER|SWP_SHOWWINDOW);
    RedrawWindow(main,nullptr,nullptr,RDW_INVALIDATE|RDW_ALLCHILDREN|RDW_UPDATENOW);
    Sleep(150);
    for (size_t i=0;i<_countof(ids);++i) before[i]=controlPixels(GetDlgItem(main,ids[i]));
    for (int i=0;i<3;++i) {
        SetWindowPos(main,nullptr,0,0,760,440,SWP_NOZORDER);
        Sleep(60);
        ShowWindow(main,SW_MAXIMIZE); Sleep(60); ShowWindow(main,SW_RESTORE);
        SetWindowPos(main,nullptr,0,0,1024,560,SWP_NOZORDER);
        Sleep(150);
        for (size_t j=0;j<_countof(ids);++j)
            if (!before[j] || before[j]!=controlPixels(GetDlgItem(main,ids[j]))) return false;
    }
    return true;
}
static bool waitUntil(DWORD timeout,const std::wstring& label,HWND control) {
    ULONGLONG end=GetTickCount64()+timeout;
    do {
        auto current=caption(control);
        if (current.find(label)!=std::wstring::npos) return true;
        if (current.find(L"(0x")!=std::wstring::npos) return false;
        Sleep(100);
    } while (GetTickCount64()<end);
    return false;
}
static bool clickPicture(HWND main,const wchar_t* expected) {
    auto surface=FindWindowExW(main,nullptr,L"AtomTubeVideo",nullptr);
    RECT r; GetWindowRect(surface,&r); POINT point={(r.left+r.right)/2,(r.top+r.bottom)/2};
    HWND hit=WindowFromPoint(point);
    if (hit!=surface && !IsChild(surface,hit)) return false;
    POINT saved; GetCursorPos(&saved); SetCursorPos(point.x,point.y);
    INPUT input[2]={}; input[0].type=input[1].type=INPUT_MOUSE;
    input[0].mi.dwFlags=MOUSEEVENTF_LEFTDOWN; input[1].mi.dwFlags=MOUSEEVENTF_LEFTUP;
    SendInput(2,input,sizeof(INPUT));
    bool ok=waitUntil(3500,expected,GetDlgItem(main,118));
    SetCursorPos(saved.x,saved.y);
    printf("picture click nativeChild=%d ok=%d\n",hit!=surface,ok); return ok;
}
static void capture(HWND main,const wchar_t* name=L"catalog");
static bool transport(HWND main) {
    HWND status=GetDlgItem(main,118), position=GetDlgItem(main,117);
    if (!waitUntil(65000,L"Reproduzindo",status)) return false;
    HWND related=GetDlgItem(main,129);
    if (IsWindowVisible(related)) {
        for (int i=0;i<240 && SendMessageW(related,LB_GETCOUNT,0,0)==0;++i) Sleep(100);
        if (!SendMessageW(related,LB_GETCOUNT,0,0)) { puts("related videos missing"); return false; }
        printf("related videos=%ld\n",static_cast<long>(SendMessageW(related,LB_GETCOUNT,0,0)));
        Sleep(650); capture(main,L"watch");
    }
    if (!clickPicture(main,L"Em pausa") || !clickPicture(main,L"Reproduzindo")) return false;
    POINT saved={}; GetCursorPos(&saved);
    SendMessageW(main,WM_COMMAND,108,0);
    RECT screen; GetWindowRect(main,&screen);
    SetCursorPos((screen.left+screen.right)/2,screen.bottom-3);
    Sleep(650);
    auto fullscreen=GetDlgItem(main,108);
    RECT button,video;
    GetWindowRect(fullscreen,&button);
    GetWindowRect(FindWindowExW(main,nullptr,L"AtomTubeVideo",nullptr),&video);
    POINT hit={(button.left+button.right)/2,(button.top+button.bottom)/2};
    bool visible=IsWindowVisible(fullscreen) && video.bottom<=button.top && WindowFromPoint(hit)==fullscreen;
    if (!clickPicture(main,L"Em pausa") || !clickPicture(main,L"Reproduzindo")) return false;
    SendMessageW(main,WM_COMMAND,108,0);
    SetCursorPos(saved.x,saved.y);
    printf("fullscreen transport=%d\n",visible);
    SendMessageW(GetDlgItem(main,115),WM_USER+1,1000,0);
    SendMessageW(main,WM_COMMAND,115,0);
    PostMessageW(main,WM_KEYDOWN,VK_DOWN,0);
    bool volume=waitUntil(2000,L"95%",GetDlgItem(main,116));
    PostMessageW(main,WM_KEYDOWN,VK_SPACE,0);
    bool paused=waitUntil(3000,L"Em pausa",status);
    auto beforeSeek=caption(position);
    PostMessageW(main,WM_KEYDOWN,VK_RIGHT,0);
    bool forward=false;
    for (int i=0;i<180;++i) {
        auto after=caption(position);
        if (after!=beforeSeek && after.find(L" / ")!=std::wstring::npos) { forward=true; break; }
        Sleep(100);
    }
    forward=forward && waitUntil(15000,L"Em pausa",status);
    auto progress=GetDlgItem(main,114);
    RECT track; GetClientRect(progress,&track);
    SendMessageW(progress,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(track.right/2,8));
    SendMessageW(progress,WM_LBUTTONUP,0,MAKELPARAM(track.right/2,8));
    Sleep(700);
    auto slider=SendMessageW(progress,WM_USER+2,0,0);
    bool clicked=slider>=450 && slider<=550 && waitUntil(15000,L"Em pausa",status);
    PostMessageW(main,WM_KEYDOWN,VK_SPACE,0);
    bool resumed=waitUntil(4000,L"Reproduzindo",status);
    printf("volume=%d paused=%d forward=%d clicked=%d resumed=%d\n",volume,paused,forward,clicked,resumed);
    return visible && volume && paused && forward && clicked && resumed;
}
static void capture(HWND main,const wchar_t* name) {
    RECT rect; GetClientRect(main,&rect);
    BITMAPINFO info={};
    info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth=rect.right; info.bmiHeader.biHeight=rect.bottom;
    info.bmiHeader.biPlanes=1; info.bmiHeader.biBitCount=32;
    void* pixels=nullptr;
    HDC dc=CreateCompatibleDC(nullptr);
    HBITMAP bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&pixels,nullptr,0);
    HGDIOBJ old=bitmap ? SelectObject(dc,bitmap) : nullptr;
    if (bitmap && PrintWindow(main,dc,PW_CLIENTONLY|2)) {
        const DWORD size=static_cast<DWORD>(rect.right*rect.bottom*4);
        BITMAPFILEHEADER head={};
        head.bfType=0x4d42; head.bfOffBits=sizeof(head)+sizeof(info.bmiHeader); head.bfSize=head.bfOffBits+size;
        FILE* file=nullptr;
        auto path=L"build\\ui-071-"+std::wstring(name)+L".bmp";
        if (!_wfopen_s(&file,path.c_str(),L"wb")) {
            fwrite(&head,sizeof(head),1,file);
            fwrite(&info.bmiHeader,sizeof(info.bmiHeader),1,file);
            fwrite(pixels,1,size,file); fclose(file);
        }
    }
    if (bitmap) { SelectObject(dc,old); DeleteObject(bitmap); }
    DeleteDC(dc);
}
static bool search(HWND main) {
    SetWindowPos(main,nullptr,0,0,1024,560,SWP_NOACTIVATE|SWP_NOMOVE|SWP_NOZORDER);
    HWND input=GetDlgItem(main,105), list=GetDlgItem(main,111);
    SendMessageW(input,WM_SETTEXT,0,reinterpret_cast<LPARAM>(L"matemática ensino médio"));
    PostMessageW(main,WM_COMMAND,110,0);
    Sleep(350); capture(main,L"searching");
    HWND scene=FindWindowExW(main,nullptr,L"AtomTubeScene",nullptr);
    DWORD previous=controlPixels(scene); int changes=0;
    ULONGLONG start=GetTickCount64();
    while (IsWindowVisible(scene) && GetTickCount64()-start<1200) {
        Sleep(12); DWORD current=controlPixels(scene);
        if (previous!=current) ++changes;
        previous=current;
    }
    printf("animation=%d distinct updates in %llu ms\n",changes,GetTickCount64()-start);
    ULONGLONG end=GetTickCount64()+26000;
    LRESULT results=0;
    do {
        Sleep(100);
        results=SendMessageW(list,LB_GETCOUNT,0,0);
    } while (results<=0 && GetTickCount64()<end);
    printf("search results=%ld\n",static_cast<long>(results));
    if (results!=15) return false;
    PostMessageW(main,WM_COMMAND,119,0);
    end=GetTickCount64()+30000;
    while (results==15 && GetTickCount64()<end) {
        Sleep(100);
        results=SendMessageW(list,LB_GETCOUNT,0,0);
    }
    printf("after loading more=%ld\n",static_cast<long>(results));
    if (results<=15 || results>30) return false;
    Sleep(2500);
    SendMessageW(list,LB_SETCURSEL,0,0);
    SendMessageW(main,WM_COMMAND,MAKEWPARAM(111,LBN_SELCHANGE),reinterpret_cast<LPARAM>(list));
    capture(main);
    PostMessageW(main,WM_COMMAND,112,0);
    Sleep(350); capture(main,L"loading");
    return transport(main);
}

static bool waitForRows(HWND list) {
    for (int attempt=0;attempt<300;++attempt) {
        if (SendMessageW(list,LB_GETCOUNT,0,0)>0) return true;
        Sleep(100);
    }
    return false;
}
static bool channels(HWND main) {
    HWND list=GetDlgItem(main,111), input=GetDlgItem(main,105);
    SendMessageW(input,WM_SETTEXT,0,reinterpret_cast<LPARAM>(L"Manual do Mundo"));
    SendMessageW(main,WM_COMMAND,120,0);
    if (!waitForRows(list) || caption(GetDlgItem(main,112))!=L"Abrir canal") return false;
    Sleep(1800); capture(main,L"channels");
    SendMessageW(list,LB_SETCURSEL,0,0);
    SendMessageW(main,WM_COMMAND,112,0);
    if (!waitForRows(list) || !IsWindowVisible(GetDlgItem(main,122))) return false;
    Sleep(1800); capture(main,L"channel");
    printf("channel grid rows=%ld\n",static_cast<long>(SendMessageW(list,LB_GETCOUNT,0,0)));
    SendMessageW(main,WM_COMMAND,126,0);
    if (!waitForRows(list)) { puts("channel streams failed"); return false; }
    SendMessageW(main,WM_COMMAND,125,0);
    if (!waitForRows(list)) { puts("channel playlists failed"); return false; }
    SendMessageW(list,LB_SETCURSEL,0,0); SendMessageW(main,WM_COMMAND,112,0);
    if (!waitForRows(list)) { puts("channel playlist contents failed"); return false; }
    SendMessageW(main,WM_COMMAND,122,0);
    SendMessageW(GetDlgItem(main,128),WM_SETTEXT,0,reinterpret_cast<LPARAM>(L"submarino"));
    SendMessageW(main,WM_COMMAND,127,0);
    if (!waitForRows(list)) { puts("channel search failed"); return false; }
    printf("filtered channel grid rows=%ld\n",static_cast<long>(SendMessageW(list,LB_GETCOUNT,0,0)));
    SendMessageW(main,WM_COMMAND,122,0);
    if (caption(GetDlgItem(main,112))!=L"Abrir canal" || SendMessageW(list,LB_GETCURSEL,0,0)!=0) return false;
    SendMessageW(main,WM_COMMAND,112,0);
    if (!waitForRows(list)) return false;
    SendMessageW(list,LB_SETCURSEL,0,0); SendMessageW(main,WM_COMMAND,112,0);
    if (!waitUntil(65000,L"Reproduzindo",GetDlgItem(main,118)) || !IsWindowEnabled(GetDlgItem(main,121))) return false;
    SendMessageW(main,WM_COMMAND,121,0);
    return waitForRows(list) && caption(GetDlgItem(main,118)).find(L"Reproduzindo")!=std::wstring::npos;
}
static bool playlists(HWND main) {
    HWND list=GetDlgItem(main,111);
    SendMessageW(GetDlgItem(main,105),WM_SETTEXT,0,reinterpret_cast<LPARAM>(L"matemática básica"));
    SendMessageW(main,WM_COMMAND,125,0);
    if (!waitForRows(list)) return false;
    Sleep(1600); capture(main,L"playlists");
    printf("playlists=%ld\n",static_cast<long>(SendMessageW(list,LB_GETCOUNT,0,0)));
    SendMessageW(list,LB_SETCURSEL,0,0); SendMessageW(main,WM_COMMAND,112,0);
    if (!waitForRows(list)) return false;
    printf("playlist videos=%ld\n",static_cast<long>(SendMessageW(list,LB_GETCOUNT,0,0)));
    SendMessageW(main,WM_COMMAND,122,0);
    return caption(GetDlgItem(main,112))==L"Abrir playlist" && SendMessageW(list,LB_GETCURSEL,0,0)==0;
}
static bool bannerLayout(HWND main) {
    HWND list=GetDlgItem(main,111);
    SendMessageW(GetDlgItem(main,105),WM_SETTEXT,0,reinterpret_cast<LPARAM>(L"Pantu Pipa"));
    SendMessageW(main,WM_COMMAND,120,0);
    if (!waitForRows(list)) return false;
    SendMessageW(list,LB_SETCURSEL,0,0); SendMessageW(main,WM_COMMAND,112,0);
    if (!waitForRows(list)) return false;
    Sleep(2000);
    SetWindowPos(main,nullptr,0,0,1024,560,SWP_NOZORDER|SWP_SHOWWINDOW);
    Sleep(200); capture(main,L"canal-netbook");
    ShowWindow(main,SW_MAXIMIZE); Sleep(350); capture(main,L"canal-maximizado");
    return true;
}
int wmain(int argc,wchar_t** argv) {
    if (argc<2 || argc>3) return 2;
    STARTUPINFOW start={}; start.cb=sizeof(start); start.dwFlags=STARTF_USESHOWWINDOW; start.wShowWindow=SW_HIDE;
    PROCESS_INFORMATION process={};
    bool homeFeed=argc==3 && wcscmp(argv[2],L"home-feed")==0;
    bool runSearch=argc==3 && wcscmp(argv[2],L"search")==0;
    bool runChannels=argc==3 && wcscmp(argv[2],L"channels")==0;
    bool runPlaylists=argc==3 && wcscmp(argv[2],L"playlists")==0;
    bool banner=argc==3 && wcscmp(argv[2],L"banner")==0;
    bool writeSettings=argc==3 && wcscmp(argv[2],L"pref-write")==0;
    bool readSettings=argc==3 && wcscmp(argv[2],L"pref-read")==0;
    bool extras=argc==3 && wcscmp(argv[2],L"extras")==0;
    bool restoredExtras=argc==3 && wcscmp(argv[2],L"extras-read")==0;
    std::wstring command=L"\""+std::wstring(argv[1])+L"\"";
    if (argc>=3 && !homeFeed && !runSearch && !runChannels && !runPlaylists && !banner && !writeSettings && !readSettings && !extras && !restoredExtras)
        command+=L" \""+std::wstring(argv[2])+L"\"";
    if (!CreateProcessW(argv[1],&command[0],nullptr,nullptr,FALSE,0,nullptr,nullptr,&start,&process)) return 3;
    HWND main=nullptr;
    for (int attempt=0;attempt<100 && !main;++attempt) {
        Sleep(50);
        HWND candidate=nullptr;
        while ((candidate=FindWindowExW(nullptr,candidate,L"AtomTube",nullptr))!=nullptr) {
            DWORD pid=0; GetWindowThreadProcessId(candidate,&pid);
            if (pid==process.dwProcessId && GetDlgItem(candidate,118)) { main=candidate; break; }
        }
    }
    int result=1;
    if (main && homeFeed) {
        ShowWindow(main,SW_SHOW); SetWindowPos(main,nullptr,0,0,1024,560,SWP_NOZORDER);
        auto card=GetDlgItem(main,141);
        auto waitCards=[&] {
            for(int i=0;i<240 && (!IsWindowVisible(card) || caption(card).empty());++i) Sleep(250);
            return IsWindowVisible(card) && !caption(card).empty();
        };
        bool ok=waitCards();
        if(ok) {
            auto first=caption(card);
            SendMessageW(main,WM_COMMAND,139,0); ok=caption(card)!=first;
            SendMessageW(main,WM_COMMAND,138,0); ok=ok && caption(card)==first;
            Sleep(1800); capture(main,L"home-090");
            HWND saved=GetDlgItem(main,145);
            ok=ok && IsWindowVisible(saved);
            SendMessageW(main,WM_COMMAND,145,0); Sleep(100);
            ok=ok && !IsWindowVisible(card) && IsWindowVisible(GetDlgItem(main,137));
            SendMessageW(main,WM_COMMAND,136,0); ok=ok && !IsWindowVisible(card) && IsWindowVisible(GetDlgItem(main,137));
            SendMessageW(main,WM_COMMAND,135,0); ok=ok && waitCards();
            SendMessageW(main,WM_COMMAND,141,0); ok=ok && !IsWindowVisible(card) && IsWindowVisible(GetDlgItem(main,123));
            SendMessageW(main,WM_COMMAND,123,0); ok=ok && IsWindowVisible(GetDlgItem(main,135));
        }
        printf("Home feed: cards, paging, subscriptions, playback navigation and return: %d\n",ok);
        result=ok ? 0 : 1;
        PostMessageW(main,WM_CLOSE,0,0);
    } else if (main && (extras || restoredExtras)) {
        ShowWindow(main,SW_SHOW); SetWindowPos(main,nullptr,0,0,1024,560,SWP_NOZORDER);
        SendMessageW(main,WM_COMMAND,131,0);
        auto value=[&](int id){return SendMessageW(GetDlgItem(main,id),CB_GETCURSEL,0,0);};
        auto set=[&](int id,int index){
            HWND control=GetDlgItem(main,id); SendMessageW(control,CB_SETCURSEL,index,0);
            SendMessageW(main,WM_COMMAND,MAKEWPARAM(id,CBN_SELCHANGE),reinterpret_cast<LPARAM>(control));
        };
        bool ok=IsWindowVisible(GetDlgItem(main,132));
        if(extras) {
            ok=ok && value(132)==2 && value(133)==6 && value(134)==0;
            Sleep(200); capture(main,L"extras-080");
            set(134,1); ok=ok && caption(GetDlgItem(main,110))==L"Search";
            set(132,4); set(133,7); set(134,2);
        } else {
            ok=ok && value(132)==4 && value(133)==7 && value(134)==2 && caption(GetDlgItem(main,100))==L"Abrir MP4";
            ok=ok && caption(GetDlgItem(main,120))==L"Canales";
            SetWindowPos(main,nullptr,0,0,760,440,SWP_NOZORDER);
            ok=ok && !IsWindowVisible(GetDlgItem(main,105)) && IsWindowVisible(GetDlgItem(main,134));
            SendMessageW(main,WM_COMMAND,131,0);
            ok=ok && IsWindowVisible(GetDlgItem(main,105)) && !IsWindowVisible(GetDlgItem(main,134));
        }
        printf("extra settings checked=%d\n",ok); result=ok ? 0 : 1;
        SendMessageW(main,WM_CLOSE,0,0);
    } else if (main && (writeSettings || readSettings)) {
        HWND input=GetDlgItem(main,105),quality=GetDlgItem(main,106),list=nullptr;
        while ((list=FindWindowExW(nullptr,list,L"ListBox",L"Pesquisas recentes")) && GetWindow(list,GW_OWNER)!=main) {}
        if (writeSettings) {
            for (int i=0;i<17;++i) {
                auto term=L"AtomTube teste "+std::to_wstring(i==16 ? 5 : i);
                SendMessageW(input,WM_SETTEXT,0,reinterpret_cast<LPARAM>(term.c_str()));
                SendMessageW(main,WM_COMMAND,110,0); SendMessageW(main,WM_COMMAND,110,0);
            }
            SendMessageW(quality,CB_SETCURSEL,2,0);
            SendMessageW(GetDlgItem(main,101),CB_SETCURSEL,1,0);
            SendMessageW(GetDlgItem(main,109),BM_SETCHECK,BST_UNCHECKED,0); result=0;
        } else {
            ShowWindow(main,SW_SHOW);
            SendMessageW(input,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(4,4)); SendMessageW(input,WM_LBUTTONUP,0,MAKELPARAM(4,4));
            int count=static_cast<int>(SendMessageW(list,LB_GETCOUNT,0,0));
            wchar_t first[205]={}; SendMessageW(list,LB_GETTEXT,0,reinterpret_cast<LPARAM>(first));
            bool settings=SendMessageW(quality,CB_GETCURSEL,0,0)==2 && SendMessageW(GetDlgItem(main,101),CB_GETCURSEL,0,0)==1 &&
                SendMessageW(GetDlgItem(main,109),BM_GETCHECK,0,0)==BST_UNCHECKED;
            bool history=IsWindowVisible(list) && count==15 && std::wstring(first)==L"AtomTube teste 5";
            PostMessageW(input,WM_KEYDOWN,VK_DOWN,0); PostMessageW(input,WM_KEYDOWN,VK_RETURN,0);
            Sleep(500); bool reused=caption(input)==first;
            SendMessageW(input,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(6,6)); SendMessageW(input,WM_LBUTTONUP,0,MAKELPARAM(6,6));
            SendMessageW(input,WM_SETTEXT,0,reinterpret_cast<LPARAM>(L"AtomTube teste"));
            HWND decoder=GetDlgItem(main,101);
            RedrawWindow(decoder,nullptr,nullptr,RDW_INVALIDATE|RDW_ERASE|RDW_UPDATENOW);
            RECT box; GetWindowRect(decoder,&box);
            POINT point={(box.left+box.right)/2,(box.top+box.bottom)/2};
            bool inFront=WindowFromPoint(point)==list;
            bool picked=false;
            if (inFront) {
                POINT local=point,saved; ScreenToClient(list,&local); GetCursorPos(&saved);
                int index=LOWORD(SendMessageW(list,LB_ITEMFROMPOINT,0,MAKELPARAM(local.x,local.y)));
                wchar_t expected[205]={}; SendMessageW(list,LB_GETTEXT,index,reinterpret_cast<LPARAM>(expected));
                SetCursorPos(point.x,point.y);
                INPUT clicks[2]={}; clicks[0].type=clicks[1].type=INPUT_MOUSE;
                clicks[0].mi.dwFlags=MOUSEEVENTF_LEFTDOWN; clicks[1].mi.dwFlags=MOUSEEVENTF_LEFTUP;
                SendInput(2,clicks,sizeof(INPUT)); Sleep(350); SetCursorPos(saved.x,saved.y);
                picked=caption(input)==expected && !IsWindowVisible(list);
            }
            printf("settings=%d history=%d keyboard=%d count=%d above decoder=%d mouse=%d\n",settings,history,reused,count,inFront,picked);
            result=settings && history && reused && inFront && picked ? 0 : 1;
        }
        SendMessageW(main,WM_CLOSE,0,0);
    } else if (main) {
        HWND smoothing=GetDlgItem(main,109);
        bool smoothingDefault=smoothing && SendMessageW(smoothing,BM_GETCHECK,0,0)==BST_CHECKED;
        if (smoothing) SendMessageW(smoothing,BM_SETCHECK,BST_UNCHECKED,0);
        bool smoothingToggle=smoothing && SendMessageW(smoothing,BM_GETCHECK,0,0)==BST_UNCHECKED;
        if (smoothing) SendMessageW(smoothing,BM_SETCHECK,BST_CHECKED,0);
        LONG normal=GetWindowLongW(main,GWL_STYLE);
        POINT initialCursor; GetCursorPos(&initialCursor);
        SendMessageW(main,WM_COMMAND,108,0);
        RECT monitor; GetWindowRect(main,&monitor);
        SetCursorPos((monitor.left+monitor.right)/2,(monitor.top+monitor.bottom)/2);
        Sleep(450);
        LONG full=GetWindowLongW(main,GWL_STYLE);
        RECT client={},video={};
        GetClientRect(main,&client);
        HWND surface=FindWindowExW(main,nullptr,L"AtomTubeVideo",nullptr);
        GetWindowRect(surface,&video);
        POINT origin={video.left,video.top}; ScreenToClient(main,&origin);
        bool filled=origin.x==0 && origin.y==0 && video.right-video.left==client.right && video.bottom-video.top==client.bottom;
        SendMessageW(main,WM_KEYDOWN,VK_ESCAPE,0);
        SetCursorPos(initialCursor.x,initialCursor.y);
        Sleep(200);
        LONG restored=GetWindowLongW(main,GWL_STYLE);
        printf("caption=%d fullCaption=%d restored=%d filled=%d smoothingDefault=%d smoothingToggle=%d\n",
            !!(normal&WS_CAPTION),!!(full&WS_CAPTION),!!(restored&WS_CAPTION),filled,smoothingDefault,smoothingToggle);
        if ((normal&WS_CAPTION) && !(full&WS_CAPTION) && (restored&WS_CAPTION) && filled &&
            smoothingDefault && smoothingToggle) {
            puts("Tela cheia restaurada e suavização de bordas alternável");
            result=0;
        }
        if (!result && argc==2) {
            capture(main,L"home");
            result=resizeTest(main) ? 0 : 1;
            printf("resize pixels=%d\n",result==0);
        }
        if (!result && argc>=3) result=(banner ? bannerLayout(main) : runChannels ? channels(main) : runPlaylists ? playlists(main) : runSearch ? search(main) : transport(main)) ? 0 : 1;
        PostMessageW(main,WM_CLOSE,0,0);
    }
    else puts("Janela AtomTube não encontrada");
    if (WaitForSingleObject(process.hProcess,3000)==WAIT_TIMEOUT) TerminateProcess(process.hProcess,1);
    CloseHandle(process.hProcess); CloseHandle(process.hThread);
    return result;
}
