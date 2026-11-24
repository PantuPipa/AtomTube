#include "player.h"
#include "youtube.h"
#include "search.h"
#include "theme.h"
#include <commdlg.h>
#include <commctrl.h>
#include <shellapi.h>
#include <algorithm>
#include <cmath>
#include "ui_language.h"
static Player player;
static YoutubeDownload download;
static YoutubeSearch search;
static std::vector<SearchResult> results;
static wstring resultStatus=L"Pesquise por título ou assunto.";
static wstring resultQuery;
static SearchKind resultKind=SearchKind::Videos;
static HWND channelsButton,playingChannel,backButton;
static unsigned nextResult=0;
enum class Page { Home, Catalog, Watch };
static Page page=Page::Home;
static HWND moreButton,brand,videoTab,playlistTab,liveTab,channelField,withinButton;
static HWND nowTitle,relatedTitle,relatedList,scene;
static HFONT wordmark,subhead;
static YoutubeSearch recommendations,preview;
static std::vector<SearchResult> suggestions;
static ChannelHeader channelHeader;
static SearchResult now;
static wstring collectionName,channelName;
static bool relatedStarted=false,showScene=false,online=false;
static bool relatedFromChannel=false;
static int listColumns=1,selectedColumn=0;
static HBITMAP grayscale=nullptr;
struct SavedList {
    SearchKind kind; wstring query,status,name,channelName;
    unsigned next; int selection,top;
    std::vector<SearchResult> rows;
    ChannelHeader header;
};
static std::vector<SavedList> history;
static HWND urlEdit, qualityBox, watchButton, searchButton, resultList, resultPlay, resultCount, resultTitle, resultToggle;
static HWND progressBar, volumeBar, muteButton, timeLabel, activity, settingsQuality;
static HWND window, openButton, modeLabel, modeBox, playButton, fullscreenButton, logButton, summary, diagnostic, showStats, edgeSmoothing;
static HFONT font, heading;
static HBRUSH backgroundBrush, panelBrush;
static wstring current;
static bool mfReady=false;
static bool fullscreen=false, overlay=false, sidebar=true, searching=false;
static int volume=100, restoreVolume=100;
static unsigned animation=0;
static LONGLONG queuedSeek=-1;
static DWORD normalStyle=0;
static WINDOWPLACEMENT normalPlacement={sizeof(normalPlacement)};
enum { Open=100, Mode, Play, Export, ShowStats, Url, QualityChoice, Watch, Fullscreen, EdgeSmoothing,
       Search, Results, ResultPlay, Browse, Progress, Volume, Mute, PlaybackTime, PlaybackStatus, MoreResults,
    FindChannels,PlayingChannel,Back,Home,VideoTab,PlaylistTab,LiveTab,Within,ChannelField,RelatedList,RecentSearches,ExtraMenu,ImageChoice,FpsChoice,LanguageChoice, ForYouTab, SubsTab, AccountButton, FeedPrev, FeedNext, FeedRetry, FirstCard, SavedTab=FirstCard+4 };
static PlaybackOptions settings;
static int fpsIndex=6;
static bool drawer=false;
static YoutubeSearch feedWorker;
static std::vector<SearchResult> feedRows;
static HWND homeForYou,homeSubs,homeSaved,homeAccount,homeStatus,homePrev,homeNext,homeRetry,homeCards[4];
static SearchKind homeTab=SearchKind::ForYou;
static wstring savedPrev,savedNext;
static bool savedPagePending=false,savedBackwards=false;
static bool feedBusy=false,feedLoaded=false,watchedRecorded=false,connected=false;
static SearchKind feedAction=SearchKind::ForYou;
static unsigned cardOffset=0,cardCount=4;
static const wchar_t* feedMessage=L"";
static const UINT StartHome=WM_APP+37;
static void layout();
static void showFeedCards();
static HWND menuButton,imageBox,fpsBox,langBox,imageLabel,fpsLabel,langLabel,noteLabel;
struct Caption { HWND hwnd; const wchar_t* pt; };
static std::vector<Caption> captions;
static HWND widget(const wchar_t* cls,const wchar_t* caption,DWORD style,int id) {
    HWND control=CreateWindowExW(0,cls,tr(caption),WS_CHILD|WS_VISIBLE|style,0,0,0,0,window,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),GetModuleHandleW(nullptr),nullptr);
    SendMessageW(control,WM_SETFONT,reinterpret_cast<WPARAM>(font),TRUE);
    if (*caption) captions.push_back({control,caption});
    return control;
}
static bool diagnosticsOn() { return SendMessageW(showStats,BM_GETCHECK,0,0)==BST_CHECKED; }
static bool smoothingOn() { return SendMessageW(edgeSmoothing,BM_GETCHECK,0,0)==BST_CHECKED; }
static const wchar_t settingsKey[]=L"Software\\AtomTube";
static HWND recentList;
static std::vector<wstring> recent;
static int preference(const wchar_t* key,int fallback,int maxValue) {
    DWORD value=0,bytes=sizeof(value);
    return RegGetValueW(HKEY_CURRENT_USER,settingsKey,key,RRF_RT_REG_DWORD,nullptr,&value,&bytes)==ERROR_SUCCESS &&
        value<=static_cast<DWORD>(maxValue) ? static_cast<int>(value) : fallback;
}
static void savePreferences() {
    HKEY key;
    if (RegCreateKeyExW(HKEY_CURRENT_USER,settingsKey,0,nullptr,0,KEY_SET_VALUE,nullptr,&key,nullptr)!=ERROR_SUCCESS) return;
    auto write=[&](const wchar_t* name,DWORD value) {
        RegSetValueExW(key,name,0,REG_DWORD,reinterpret_cast<const BYTE*>(&value),sizeof(value));
    };
    write(L"ImageProfile",static_cast<DWORD>(settings.image)); write(L"TargetFpsIndex",fpsIndex);
    write(L"Language",static_cast<DWORD>(settings.language));
    write(L"Quality",static_cast<DWORD>(SendMessageW(qualityBox,CB_GETCURSEL,0,0)));
    write(L"Decoder",static_cast<DWORD>(SendMessageW(modeBox,CB_GETCURSEL,0,0)));
    write(L"Smoothing",smoothingOn()); write(L"Diagnostics",diagnosticsOn());
    write(L"Volume",volume); write(L"UnmutedVolume",restoreVolume); RegCloseKey(key);
}
static void readRecent() {
    wchar_t data[3200]={}; DWORD bytes=sizeof(data);
    recent.clear();
    if (RegGetValueW(HKEY_CURRENT_USER,settingsKey,L"Searches",RRF_RT_REG_MULTI_SZ,nullptr,data,&bytes)!=ERROR_SUCCESS) return;
    data[_countof(data)-1]=0;
    for (auto p=data;p<data+_countof(data) && *p && recent.size()<15;p+=wcslen(p)+1) recent.emplace_back(p);
}
static void remember(const wstring& term) {
    readRecent();
    recent.erase(std::remove_if(recent.begin(),recent.end(),[&](const wstring& s){return !_wcsicmp(s.c_str(),term.c_str());}),recent.end());
    feedLoaded=false; recent.insert(recent.begin(),term); if (recent.size()>15) recent.resize(15);
    wstring data;
    for (const auto& s:recent) { data+=s; data.push_back(0); }
    RegSetKeyValueW(HKEY_CURRENT_USER,settingsKey,L"Searches",REG_MULTI_SZ,data.c_str(),static_cast<DWORD>((data.size()+1)*sizeof(wchar_t)));
}
static void showRecent(bool all) {
    if (!recentList || GetFocus()!=urlEdit || fullscreen) return;
    wchar_t term[205]={}; GetWindowTextW(urlEdit,term,_countof(term));
    SendMessageW(recentList,LB_RESETCONTENT,0,0);
    for (const auto& s:recent) if (all || !_wcsnicmp(s.c_str(),term,wcslen(term)))
        SendMessageW(recentList,LB_ADDSTRING,0,reinterpret_cast<LPARAM>(s.c_str()));
    int count=static_cast<int>(SendMessageW(recentList,LB_GETCOUNT,0,0));
    if (!count) { ShowWindow(recentList,SW_HIDE); return; }
    RECT r; GetWindowRect(urlEdit,&r);
    int row=static_cast<int>(SendMessageW(recentList,LB_GETITEMHEIGHT,0,0));
    int height=4+row*std::min(count,7);
    MONITORINFO monitor={sizeof(monitor)};
    GetMonitorInfoW(MonitorFromRect(&r,MONITOR_DEFAULTTONEAREST),&monitor);
    int top=r.bottom+4;
    if (top+height>monitor.rcWork.bottom) top=std::max<LONG>(monitor.rcWork.top,r.top-height-4);
    SetWindowPos(recentList,HWND_TOP,r.left,top,r.right-r.left,height,SWP_SHOWWINDOW|SWP_NOACTIVATE);
}
static void showControl(HWND control,bool visible) {
    if (visible!=((GetWindowLongW(control,GWL_STYLE)&WS_VISIBLE)!=0)) ShowWindow(control,visible ? SW_SHOWNA : SW_HIDE);
}
static void place(HWND control,int x,int y,int w,int h) {
    SetWindowPos(control,nullptr,x,y,std::max(1,w),std::max(1,h),
        SWP_NOZORDER|SWP_NOACTIVATE|SWP_NOCOPYBITS|SWP_NOREDRAW);
}
static void transport(int x,int y,int w,bool visible) {
    for (HWND control:{progressBar,playButton,timeLabel,muteButton,volumeBar,fullscreenButton}) {
        showControl(control,visible);
        if (visible) SetWindowPos(control,HWND_TOP,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);
    }
    if (!visible) return;
    place(progressBar,x,y,w,20);
    place(playButton,x,y+22,80,30);
    place(timeLabel,x+86,y+22,w-329,30);
    place(muteButton,x+w-240,y+22,65,30);
    place(volumeBar,x+w-169,y+22,63,30);
    place(fullscreenButton,x+w-100,y+22,100,30);
}
static constexpr int homeTabsBelowSearch=106,homeCardsBelowTabs=62;
static constexpr int maxHomeCardHeight=205,homeFooterSpace=52;
static int homeSearchY(int h) {
    if(h<490) return 62;
    // Center the visible artwork and the full card row together when space permits.
    const int cardsBottom=homeTabsBelowSearch+homeCardsBelowTabs+maxHomeCardHeight;
    const int centered=(h+100-cardsBottom)/2;
    return std::max(112,std::min(centered,h-homeFooterSpace-cardsBottom));
}
static void layoutHomeFeed(bool visible,int inset,int w,int h,int sy) {
    if (!visible && feedBusy) {
        feedWorker.cancel(); KillTimer(window,9); feedBusy=false; feedLoaded=false;
    }
    int x=inset+22,width=w-x-22,top=sy+homeTabsBelowSearch;
    auto box=[&](HWND control,int cx,int cy,int cw,int ch) {
        showControl(control,visible);
        if (visible) place(control,cx,cy,cw,ch);
    };
    box(homeForYou,x,top,109,30); box(homeSubs,x+116,top,116,30);
    box(homeSaved,x+239,top,141,30);
    bool saved=homeTab==SearchKind::SavedVideos;
    box(homeAccount,x+width-142,top,142,30);
    bool emptySaved=saved && feedRows.empty();
    box(homeStatus,x,top+(emptySaved ? 66 : 34),width-(emptySaved ? 0 : saved && connected ? 185 : 98),21);
    box(homePrev,x+width-84,top+32,39,25); box(homeNext,x+width-40,top+32,40,25);
    cardCount=std::min(4,std::max(2,width/180));
    if (!feedRows.empty() && cardOffset>=feedRows.size()) cardOffset=0;
    int gap=8,cell=(width-gap*(cardCount-1))/cardCount;
    int cardsY=top+homeCardsBelowTabs;
    int cardHeight=std::max(64,std::min(maxHomeCardHeight,h-homeFooterSpace-cardsY));
    for (unsigned i=0;i<4;++i) {
        bool active=visible && i<cardCount && cardOffset+i<feedRows.size();
        showControl(homeCards[i],active);
        if(active) place(homeCards[i],x+i*(cell+gap),cardsY,cell,cardHeight);
    }
    bool retry=visible && ((saved && connected) || (!saved && !feedBusy && feedLoaded && feedRows.empty() && (homeTab==SearchKind::ForYou || connected)));
    showControl(homeRetry,retry);
    if(retry) {
        if(saved) place(homeRetry,x+width-177,top+32,86,25);
        else place(homeRetry,x,top+63,150,30);
        EnableWindow(homeRetry,!feedBusy);
    }
    EnableWindow(homePrev,cardOffset>0 || (saved && !feedBusy && !savedPrev.empty()));
    EnableWindow(homeNext,cardOffset+cardCount<feedRows.size() || (saved && !feedBusy && !savedNext.empty()));
    if(visible && !feedLoaded && !feedBusy) PostMessageW(window,StartHome,0,0);
}
static bool stageNeeded() {
    return download.active() || player.state==State::Opening || player.state==State::Starting ||
        (!player.active() && page==Page::Watch);
}
static int listIndex(UINT message) {
    int row=static_cast<int>(SendMessageW(resultList,message,0,0));
    return row<0 ? -1 : row*listColumns+(message==LB_GETCURSEL ? selectedColumn : 0);
}
static void populateList() {
    size_t visibleRows=(results.size()+listColumns-1)/listColumns;
    for (size_t i=static_cast<size_t>(SendMessageW(resultList,LB_GETCOUNT,0,0));i<visibleRows;++i)
        SendMessageW(resultList,LB_ADDSTRING,0,i);
}
static void restoreList(int selected,int top) {
    selectedColumn=selected<0 ? 0 : selected%listColumns;
    SendMessageW(resultList,LB_SETCURSEL,selected<0 ? -1 : selected/listColumns,0);
    SendMessageW(resultList,LB_SETTOPINDEX,std::max(0,top)/listColumns,0);
}
static void animateScene(bool enable) {
    static bool running=false;
    if (enable==running) return;
    if (enable) running=SetTimer(scene,2,33,nullptr)!=0;
    else { KillTimer(scene,2); running=false; }
}
static void layout() {
    if (!scene || !player.video) return;
    if (recentList) ShowWindow(recentList,SW_HIDE);
    RECT r; GetClientRect(window,&r);
    int w=r.right,h=r.bottom;
    bool home=!fullscreen && page==Page::Home, catalog=!fullscreen && page==Page::Catalog;
    bool watchPage=fullscreen || page==Page::Watch, chrome=!fullscreen;
    bool channel=catalog && channelPage(resultKind);
    bool diagnostics=chrome && !home && diagnosticsOn();
    bool loading=catalog && searching && results.empty();
    showScene=watchPage && stageNeeded();
    auto box=[](HWND c,bool visible,int x,int y,int ww,int hh) {
        showControl(c,visible); if (visible) place(c,x,y,ww,hh);
    };
    bool panel=home && drawer;
    int inset=panel ? 230 : 0;
    bool dedicated=panel && w<850;
    int sy=homeSearchY(h),left=(w+inset-564)/2;
    layoutHomeFeed(home && !dedicated,inset,w,h,sy);
    box(menuButton,home,14,12,42,33);
    int px=dedicated ? (w-250)/2 : 16;
    HWND labels[]={imageLabel,fpsLabel,langLabel};
    HWND choices[]={imageBox,fpsBox,langBox};
    for (int i=0;i<3;++i) {
        int y=72+72*i;
        box(labels[i],panel,px,y,201,20);
        box(choices[i],panel,px,y+23,201,215);
    }
    box(noteLabel,panel,px,292,210,42);
    if (dedicated) chrome=false;
    box(brand,chrome && !home,8,9,141,35);
    box(urlEdit,chrome,home ? left+12 : 161,home ? sy+8 : 16,home ? 457 : w-454,22);
    box(searchButton,chrome,home ? left+474 : w-285,home ? sy : 10,home ? 90 : 80,home ? 35 : 32);
    box(openButton,chrome,home ? w-113 : w-199,home ? h-46 : 10,94,32);
    box(watchButton,chrome && !home,w-99,10,87,32);
    int settingsY=home ? sy+50 : 54;
    int sx=home ? left : 14;
    box(settingsQuality,chrome,sx,settingsY,93,17);
    box(qualityBox,chrome,sx,settingsY+18,134,145);
    box(modeLabel,chrome,sx+147,settingsY,142,17);
    box(modeBox,chrome,sx+147,settingsY+18,168,110);
    box(edgeSmoothing,chrome,sx+321,settingsY+11,183,28);
    box(showStats,chrome && !home,w-243,settingsY+10,111,26);
    box(logButton,chrome && !home,w-131,settingsY+10,119,26);
    box(activity,false,0,0,1,1);
    box(diagnostic,diagnostics,16,h-43,w-32,40);
    int bottom=h-(diagnostics ? 49 : 8);
    bool inList=channel || (catalog && resultKind==SearchKind::Entries);
    int tabs=channel ? 184 : 109;
    if (catalog) {
        int columns=channel ? std::max(2,std::min(6,(w-46)/184)) : 1;
        if (columns!=listColumns) {
            int selected=listIndex(LB_GETCURSEL),top=listIndex(LB_GETTOPINDEX);
            SendMessageW(resultList,LB_RESETCONTENT,0,0);
            listColumns=columns; populateList(); restoreList(selected,top);
        }
        SendMessageW(resultList,LB_SETITEMHEIGHT,0,channel ? 163 : 91);
    }
    bool returnList=chrome && page==Page::Watch && !resultQuery.empty();
    box(backButton,(catalog && !history.empty()) || returnList,w-105,109,91,27);
    SetWindowTextW(backButton,returnList ? tr(L"Resultados") : tr(L"Voltar"));
    box(videoTab,catalog && resultKind!=SearchKind::Entries,16,tabs,86,28);
    box(channelsButton,catalog && !inList,108,tabs,86,28);
    box(liveTab,channel,108,tabs,86,28);
    box(playlistTab,catalog && resultKind!=SearchKind::Entries,200,tabs,92,28);
    box(channelField,channel,308,tabs,w-444,27);
    box(withinButton,channel,w-128,tabs,112,27);
    bool returnVideo=player.active() || download.active();
    box(resultToggle,chrome && (page==Page::Watch || returnVideo),home ? w-251 : w-224,
        home ? h-46 : 109,home ? 128 : 106,28);
    SetWindowTextW(resultToggle,page==Page::Watch ? (sidebar ? tr(L"Ocultar lista") : tr(L"Relacionados")) : tr(L"Voltar ao vídeo"));
    bool entries=catalog && resultKind==SearchKind::Entries;
    box(resultTitle,entries,16,106,w-272,32);
    int listY=tabs+38;
    box(resultList,catalog && !loading,16,listY,w-32,bottom-listY-46);
    box(resultCount,catalog && !loading,18,bottom-35,w-333,29);
    box(moreButton,catalog && !loading,w-154,bottom-36,138,31);
    box(resultPlay,catalog && !loading,w-293,bottom-36,130,31);
    int aside=sidebar && !fullscreen ? std::min(300,std::max(214,w/4)) : 0;
    int vw=fullscreen ? w : w-32-(aside ? aside+12 : 0);
    int top=fullscreen ? 0 : 151;
    int end=fullscreen ? h-(overlay ? 70 : 0) : bottom-78;
    box(nowTitle,chrome && watchPage,16,102,w-482,47);
    box(playingChannel,chrome && watchPage,w-453,108,219,31);
    box(relatedTitle,chrome && watchPage && sidebar,w-aside-16,top,aside,28);
    box(relatedList,chrome && watchPage && sidebar,w-aside-16,top+32,aside,bottom-top-32);
    // Keep the native video HWND sized for decoders even while the loading scene covers it.
    if (watchPage) place(player.video,fullscreen ? 0 : 16,top,vw,end-top);
    showControl(player.video,watchPage && !showScene);
    box(scene,loading || showScene,loading ? 16 : fullscreen ? 0 : 16,
        loading ? listY : top,loading ? w-32 : vw,loading ? bottom-listY : end-top);
    animateScene(loading || (showScene && (player.active() || download.active())));
    if (watchPage) transport(fullscreen ? 12 : 16,fullscreen ? h-64 : end+2,fullscreen ? w-24 : vw,!fullscreen || overlay);
    else transport(0,0,0,false);
    box(summary,chrome && watchPage,18,end+56,vw-4,22);
    if (watchPage && player.active()) player.resize();
    RedrawWindow(window,nullptr,nullptr,RDW_INVALIDATE|RDW_ERASE|RDW_ALLCHILDREN);
}
static void toggleFullscreen() {
    if (!window) return;
    if (!fullscreen) {
        normalStyle=GetWindowLongW(window,GWL_STYLE);
        normalPlacement.length=sizeof(normalPlacement);
        GetWindowPlacement(window,&normalPlacement);
        MONITORINFO monitor={sizeof(monitor)};
        GetMonitorInfoW(MonitorFromWindow(window,MONITOR_DEFAULTTONEAREST),&monitor);
        fullscreen=true; overlay=false;
        SetTimer(window,4,200,nullptr);
        SetWindowLongW(window,GWL_STYLE,(normalStyle&~(WS_CAPTION|WS_THICKFRAME|WS_MINIMIZEBOX|WS_MAXIMIZEBOX|WS_SYSMENU))|WS_POPUP);
        SetWindowPos(window,HWND_TOP,monitor.rcMonitor.left,monitor.rcMonitor.top,
            monitor.rcMonitor.right-monitor.rcMonitor.left,monitor.rcMonitor.bottom-monitor.rcMonitor.top,
            SWP_FRAMECHANGED|SWP_SHOWWINDOW);
        journal.add(L"Tela cheia ativada");
    } else {
        fullscreen=false; KillTimer(window,4);
        SetWindowLongW(window,GWL_STYLE,normalStyle);
        SetWindowPlacement(window,&normalPlacement);
        SetWindowPos(window,nullptr,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_FRAMECHANGED|SWP_SHOWWINDOW);
        journal.add(L"Tela cheia desativada");
    }
    SetWindowTextW(fullscreenButton,fullscreen ? tr(L"Sair tela cheia") : tr(L"Tela cheia"));
    SetFocus(window);
    layout();
}
static wstring localMessage(const wstring& text) {
    if(uiLanguage==Language::Portuguese) return text;
    for (auto prefix:{L"Baixando faixa do YouTube"}) if (text.compare(0,wcslen(prefix),prefix)==0)
        return wstring(tr(prefix))+text.substr(wcslen(prefix));
    auto suffix=text.find(L" resultados no YouTube");
    if(suffix!=wstring::npos) return text.substr(0,suffix)+tr(L" resultados no YouTube");
    auto error=text.find(L" (0x");
    if(error!=wstring::npos) {
        auto operation=text.substr(0,error);
        auto translated=tr(operation.c_str());
        if(translated==operation.c_str()) return (uiLanguage==Language::English ? L"Playback error" : L"Error de reproducción")+text.substr(error);
        return wstring(translated)+text.substr(error);
    }
    return tr(text.c_str());
}
static wstring displayMetrics(wstring value) {
    if(uiLanguage==Language::Portuguese || value.empty()) return value;
    static const wchar_t* const terms[][3]={
        {L"FPS apresentados",L"Displayed FPS",L"FPS presentados"},
        {L" | exibidos ",L" | displayed ",L" | mostrados "},
        {L" | perdidos no EVR ",L" | dropped by EVR ",L" | perdidos en EVR "},
        {L" | perdidos ",L" | dropped ",L" | perdidos "},
        {L"buffers de áudio",L"audio buffers",L"búferes de audio"},
        {L"CPU sistema",L"System CPU",L"CPU del sistema"},
        {L"fonte ",L"source ",L"fuente "},{L" fps de origem",L" source fps",L" fps de origen"},
        {L"apresentação ",L"presentation ",L"presentación "},
        {L"Última leitura válida",L"Last valid reading",L"Última lectura válida"}
    };
    for(const auto& term:terms) {
        auto pos=value.find(term[0]);
        if(pos!=wstring::npos) value.replace(pos,wcslen(term[0]),term[uiLanguage==Language::English ? 1 : 2]);
    }
    return value;
}
static void updateText(HWND control,const wstring& value) {
    wchar_t before[1024]; GetWindowTextW(control,before,_countof(before));
    if (value!=before) SetWindowTextW(control,value.c_str());
}
static wstring timeText(LONGLONG ms) {
    auto seconds=std::max(0LL,ms/1000);
    return seconds>=3600 ? fmt(L"%lld:%02lld:%02lld",seconds/3600,seconds/60%60,seconds%60) :
        fmt(L"%02lld:%02lld",seconds/60,seconds%60);
}
static bool seekable() {
    return player.duration()>0 &&
        (player.state==State::Playing || player.state==State::Paused || player.state==State::Ended || queuedSeek>=0);
}
static void timeline() {
    LONGLONG total=player.duration(), at=queuedSeek>=0 ? queuedSeek : player.position();
    bool dragging=SendMessageW(progressBar,SliderTracking,0,0)!=0;
    if (dragging) at=total*SendMessageW(progressBar,SliderGet,0,0)/1000;
    else SendMessageW(progressBar,SliderSet,static_cast<WPARAM>(total>0 ?
        std::min(1000LL,std::max(0LL,at*1000/total)) : 0),0);
    EnableWindow(progressBar,seekable());
    updateText(timeLabel,timeText(at)+L" / "+(total>0 ? timeText(total) : L"--:--"));
}
static wstring accountDir() {
    wchar_t path[MAX_PATH];
    DWORD n=GetEnvironmentVariableW(L"LOCALAPPDATA",path,_countof(path));
    return n && n<_countof(path) ? wstring(path)+L"\\AtomTube" : L"";
}
static bool accountExists() {
    auto dir=accountDir();
    return !dir.empty() && GetFileAttributesW((dir+L"\\youtube-account.bin").c_str())!=INVALID_FILE_ATTRIBUTES;
}
static void recordHomeWatch(const SearchResult& row) {
    wstring topic=row.author.empty() ? row.title : row.author;
    std::replace_if(topic.begin(),topic.end(),[](wchar_t c){return c<L' ';},L' ');
    wstring value=row.id+L"\t"+topic.substr(0,140);
    wchar_t previous[5000]={}; DWORD size=sizeof(previous);
    wstring data=value; data.push_back(0);
    if(RegGetValueW(HKEY_CURRENT_USER,settingsKey,L"HomeWatched",RRF_RT_REG_MULTI_SZ,nullptr,previous,&size)==ERROR_SUCCESS) {
        previous[_countof(previous)-1]=0;
        unsigned count=1;
        for(auto p=previous;*p && count<24;p+=wcslen(p)+1) {
            if(wcsncmp(p,row.id.c_str(),row.id.size())==0) continue;
            data+=p; data.push_back(0); ++count;
        }
    }
    RegSetKeyValueW(HKEY_CURRENT_USER,settingsKey,L"HomeWatched",REG_MULTI_SZ,data.c_str(),static_cast<DWORD>((data.size()+1)*sizeof(wchar_t)));
}
static void setFeedText() {
    updateText(homeStatus,tr(feedMessage));
    updateText(homeRetry,tr(homeTab==SearchKind::SavedVideos ? L"Atualizar" : L"Tentar de novo"));
    updateText(homeAccount,tr(feedBusy && feedAction==SearchKind::Connect ? L"Cancelar" : connected ? L"Desconectar" : L"Conectar conta"));
}
static void showFeedCards() {
    for(unsigned i=0;i<4;++i) {
        unsigned j=cardOffset+i;
        updateText(homeCards[i],j<feedRows.size() ? feedRows[j].title : L"");
        InvalidateRect(homeCards[i],nullptr,FALSE);
    }
}
static void drawFeedCard(const DRAWITEMSTRUCT& item) {
    unsigned j=cardOffset+item.CtlID-FirstCard;
    auto r=item.rcItem; HDC dc=item.hDC;
    Theme::fill(dc,r,Theme::panel);
    if(j>=feedRows.size()) return;
    const auto& row=feedRows[j];
    int w=r.right,h=r.bottom,photo=std::min(w*9/16,h-59);
    if(photo>=27) {
        int imageW=photo*16/9, x=(w-imageW)/2;
        if(row.thumbnail) Theme::bitmap(dc,static_cast<HBITMAP>(row.thumbnail.get()),RECT{x,0,x+imageW,photo});
        else Theme::mark(dc,w/2-10,photo/2-10,20);
    } else photo=0;
    SelectObject(dc,font);
    Theme::label(dc,row.title,RECT{8,photo+4,w-7,h-21},Theme::text,DT_WORDBREAK|DT_END_ELLIPSIS|DT_NOPREFIX);
    wstring detail=row.author;
    if(!row.duration.empty()) { if(!detail.empty()) detail+=L"  ·  "; detail+=row.duration; }
    Theme::label(dc,detail,RECT{8,h-21,w-7,h-1},Theme::muted,DT_SINGLELINE|DT_END_ELLIPSIS|DT_NOPREFIX);
    if(item.itemState&ODS_FOCUS) DrawFocusRect(dc,&r);
}
static void resetFeed() {
    feedWorker.cancel(); KillTimer(window,9); feedBusy=false; feedLoaded=false; feedRows.clear();
    savedPrev.clear(); savedNext.clear(); savedPagePending=false;
    cardOffset=0; feedMessage=L""; setFeedText(); showFeedCards();
}
static void startFeed(SearchKind action,const wstring& cursor=L"",bool backwards=false) {
    feedAction=action; feedLoaded=true;
    if(action==SearchKind::SavedVideos && !connected) {
        feedMessage=L"Conecte sua conta para abrir a playlist Ver no AtomTube."; setFeedText(); return;
    }
    if(action==SearchKind::Subscriptions && !connected) {
        feedMessage=L"Conecte sua conta para ver os canais que você acompanha."; setFeedText(); return;
    }
    try {
        feedWorker.start(action,cursor,1,{},settings.catalogLanguage()); feedBusy=true;
        savedPagePending=action==SearchKind::SavedVideos; savedBackwards=backwards;
        feedMessage=action==SearchKind::Connect ? L"Conclua o login no navegador." : L"Buscando vídeos…";
        SetTimer(window,9,350,nullptr);
    } catch(Problem p) { journal.add(p.operation); feedMessage=L"Falha ao carregar. Tente de novo."; }
    setFeedText(); layout();
}
static const wchar_t* feedError(const wstring& code) {
    if(code==L"config") return L"Cliente OAuth inválido. Importe um JSON de app para computador.";
    if(code==L"browser") return L"Não foi possível abrir o navegador padrão.";
    if(code==L"cancel") return L"Login cancelado ou expirado.";
    if(code==L"reauth") return L"Sessão expirada. Desconecte e entre novamente.";
    if(code==L"api") return L"Google recusou o pedido. Verifique a API, o acesso e a cota do projeto.";
    return L"Falha ao carregar. Tente de novo.";
}
static void pollFeed() {
    ChannelHeader report; wstring status; unsigned ignored=0; std::vector<SearchResult> rows;
    bool running=feedWorker.poll(rows,status,ignored,&report);
    if(!running) { KillTimer(window,9); feedBusy=false; }
    if(!status.empty()) {
        if(feedAction==SearchKind::SavedVideos && report.name.compare(0,6,L"saved|")==0) {
            size_t sep=report.name.find(L'|',6);
            if(sep!=wstring::npos) {
                savedPrev=report.name.substr(6,sep-6); savedNext=report.name.substr(sep+1);
                feedRows=std::move(rows);
                if(savedPagePending) {
                    cardOffset=savedBackwards && !feedRows.empty() ? static_cast<unsigned>((feedRows.size()-1)/cardCount)*cardCount : 0;
                    savedPagePending=false;
                }
                feedMessage=feedRows.empty() ? (savedNext.empty() && savedPrev.empty() ?
                    L"Salve um vídeo nessa playlist no YouTube e clique em Atualizar." : L"Esta página não tem vídeos disponíveis.") :
                    L"Vídeos que você salvou no YouTube";
                showFeedCards();
            }
        } else if(feedAction==SearchKind::SavedVideos && (report.name==L"no-playlist" || report.name==L"missing")) {
            feedRows.clear(); cardOffset=0; savedPrev.clear(); savedNext.clear();
            feedMessage=L"Crie a playlist Ver no AtomTube no YouTube e clique em Atualizar."; showFeedCards();
        } else if(report.name!=L"ok" && report.name!=L"starter") feedMessage=feedError(report.name);
        else if(feedAction==SearchKind::Connect) {
            connected=accountExists();
            if(connected) { resetFeed(); layout(); return; }
            feedMessage=feedError(L"");
        } else {
            feedRows=std::move(rows);
            feedMessage=feedRows.empty() ? L"Nenhum vídeo disponível." : homeTab==SearchKind::Subscriptions ?
                L"Publicações recentes dos seus canais" : report.name==L"starter" ? L"Para começar, ciência e astronomia" : L"Sugestões com base na sua atividade";
            showFeedCards();
        }
    }
    setFeedText();
    if(!status.empty() || !running) {
        // Only rearrange when cards appear or the retry button changes.
        if (page==Page::Home && !fullscreen && (!running || feedAction==SearchKind::SavedVideos || (!feedRows.empty() && !IsWindowVisible(homeCards[0])))) {
            RECT r; GetClientRect(window,&r);
            layoutHomeFeed(!(drawer && r.right<850),drawer ? 230 : 0,r.right,r.bottom,homeSearchY(r.bottom));
            if (IsWindowVisible(homeRetry)) InvalidateRect(homeRetry,nullptr,FALSE);
        }
    }
}
static bool importClient() {
    wchar_t path[MAX_PATH]={};
    OPENFILENAMEW of={}; of.lStructSize=sizeof(of); of.hwndOwner=window;
    of.lpstrTitle=tr(L"Importar cliente OAuth para computador"); of.lpstrFilter=L"Google OAuth (JSON)\0*.json\0";
    of.lpstrFile=path; of.nMaxFile=_countof(path); of.Flags=OFN_FILEMUSTEXIST|OFN_NOCHANGEDIR;
    if(!GetOpenFileNameW(&of)) return false;
    wstring dir=accountDir(); if(dir.empty()) return false;
    CreateDirectoryW(dir.c_str(),nullptr);
    WIN32_FILE_ATTRIBUTE_DATA info={};
    if(GetFileAttributesExW(path,GetFileExInfoStandard,&info) && !info.nFileSizeHigh && info.nFileSizeLow<=65536 &&
        (!_wcsicmp(path,(dir+L"\\google-oauth.json").c_str()) || CopyFileW(path,(dir+L"\\google-oauth.json").c_str(),FALSE))) return true;
    MessageBoxW(window,tr(L"Não foi possível importar o arquivo."),L"AtomTube",MB_OK|MB_ICONWARNING); return false;
}
static void manageAccount() {
    if(feedBusy && feedAction==SearchKind::Connect) {
        resetFeed(); connected=accountExists(); layout(); return;
    }
    wstring dir=accountDir();
    if(connected) {
        if(MessageBoxW(window,tr(L"Desconectar o YouTube e remover as inscrições salvas neste PC?"),L"AtomTube",MB_YESNO|MB_ICONQUESTION)!=IDYES) return;
        resetFeed();
        DeleteFileW((dir+L"\\youtube-account.bin").c_str()); DeleteFileW((dir+L"\\subscriptions.json").c_str());
        connected=accountExists(); setFeedText();
        if(connected) MessageBoxW(window,tr(L"Não foi possível desconectar a conta."),L"AtomTube",MB_OK);
        layout(); return;
    }
    wchar_t path[MAX_PATH]={}; GetModuleFileNameW(nullptr,path,_countof(path));
    wstring kit=path; kit=kit.substr(0,kit.find_last_of(L'\\'))+L"\\google-oauth.json";
    if(!wcscmp(feedMessage,feedError(L"config")) || (GetFileAttributesW(kit.c_str())==INVALID_FILE_ATTRIBUTES &&
        GetFileAttributesW((dir+L"\\google-oauth.json").c_str())==INVALID_FILE_ATTRIBUTES)) {
        if(MessageBoxW(window,tr(L"Para conectar, importe o JSON de um cliente OAuth para computador com a YouTube Data API v3 ativada. Selecionar arquivo?"),
            L"AtomTube",MB_YESNO|MB_ICONINFORMATION)!=IDYES || !importClient()) return;
    }
    if(homeTab==SearchKind::ForYou) homeTab=SearchKind::Subscriptions;
    resetFeed(); startFeed(SearchKind::Connect);
}
static void refresh() {
    EnableWindow(playingChannel,!download.channelId.empty() || !now.channel.empty());
    if (!download.title.empty() && now.title.empty()) now.title=download.title;
    if (!download.author.empty() && now.author.empty()) now.author=download.author;
    if (online && !watchedRecorded && player.state==State::Playing && !now.title.empty()) {
        recordHomeWatch(now); watchedRecorded=true; feedLoaded=false;
    }
    updateText(nowTitle,now.title.empty() ? tr(L"Vídeo do YouTube") : now.title);
    updateText(playingChannel,now.author.empty() ? tr(L"Canal do vídeo") : now.author);
    if ((page==Page::Watch || fullscreen) && showScene!=stageNeeded()) layout();
    if ((page==Page::Watch || fullscreen) && showScene) {
        animateScene(player.active() || download.active());
        InvalidateRect(scene,nullptr,FALSE);
    }
    if (page==Page::Watch && sidebar && online && !relatedStarted && !now.title.empty() && player.state==State::Playing) {
        relatedStarted=true;
        try { recommendations.start(SearchKind::Videos,now.title.substr(0,160),1,{},settings.catalogLanguage()); SetTimer(window,7,450,nullptr); }
        catch (Problem p) { journal.add(p.operation); updateText(relatedTitle,tr(L"Relacionados indisponíveis")); }
    }
    bool busy=download.active();
    EnableWindow(qualityBox,mfReady && !busy);
    EnableWindow(watchButton,mfReady);
    updateText(watchButton,busy ? tr(L"Cancelar") : tr(L"Abrir link"));
    updateText(summary,localMessage(player.status)+(player.media.empty() ? L"" : L"   |   "+displayMetrics(player.media)));
    if (diagnosticsOn()) updateText(diagnostic,displayMetrics(player.stats));
    updateText(playButton,player.state==State::Ended ? tr(L"Repetir") : player.state==State::Playing ? tr(L"Pausar") : tr(L"Reproduzir"));
    EnableWindow(playButton,player.state==State::Playing || player.state==State::Paused || player.state==State::Ended);
    bool loading=busy || player.state==State::Opening || player.state==State::Starting;
    static const wchar_t* frames[]={L"●··",L"·●·",L"··●"};
    updateText(activity,loading ? frames[animation++%3] : L"");
    timeline();
}
static void loadCurrent(ULONGLONG requested=0,LONGLONG offset=0,bool paused=false) {
    bool cpu=SendMessageW(modeBox,CB_GETCURSEL,0,0)==1;
    try {
        if (!download.videoUrl.empty()) player.openStream(download.videoUrl,download.audioUrl,cpu,
            requested ? requested : GetTickCount64(),download.videoWidth,download.videoHeight,download.videoFps,smoothingOn(),
            static_cast<LONGLONG>(download.durationSeconds)*1000,offset,paused);
        else player.open(current.c_str(),cpu);
    }
    catch (Problem p) { player.fail(p); }
    if (player.active()) SetTimer(window,1,300,nullptr); else KillTimer(window,1);
    refresh();
}
static void queueSeek(LONGLONG at) {
    if (!seekable()) return;
    queuedSeek=std::max(0LL,std::min(at,std::max(0LL,player.duration()-500)));
    SetTimer(window,5,180,nullptr);
    timeline();
}
static void cancelSeek() { queuedSeek=-1; KillTimer(window,5); }
static void changeVolume(int value) {
    volume=std::max(0,std::min(100,value));
    if (volume) restoreVolume=volume;
    try { player.setVolume(volume); } catch (Problem p) { journal.add(p.operation); }
    SendMessageW(volumeBar,SliderSet,volume*10,0);
    updateText(muteButton,volume ? fmt(L"Vol %d%%",volume) : tr(L"Sem som"));
}
static void beginWatch(const SearchResult& selected,bool fromYoutube) {
    recommendations.cancel(); preview.cancel(); KillTimer(window,7); KillTimer(window,8);
    relatedStarted=false; relatedFromChannel=false; online=fromYoutube; suggestions.clear(); SendMessageW(relatedList,LB_RESETCONTENT,0,0);
    updateText(relatedTitle,fromYoutube ? tr(L"Relacionados") : tr(L"Arquivo local"));
    watchedRecorded=false; now=selected; player.options=settings; player.localSmooth=smoothingOn();
    if (grayscale) { DeleteObject(grayscale); grayscale=nullptr; }
    drawer=false; page=Page::Watch; sidebar=fromYoutube; layout();
}
static void openLocal(const wstring& path) {
    if (!mfReady) return;
    cancelSeek(); player.close(); download.clear(); KillTimer(window,2);
    SearchResult file; file.title=path.substr(path.find_last_of(L"\\/")+1); beginWatch(file,false);
    current=path; loadCurrent(); SetFocus(window);
    auto base=path.find_last_of(L"\\/");
    updateText(window,L"AtomTube · "+path.substr(base==wstring::npos ? 0 : base+1));
}
static void chooseFile() {
    wchar_t path[MAX_PATH]=L"";
    OPENFILENAMEW of={}; of.lStructSize=sizeof(of); of.hwndOwner=window;
    of.lpstrFilter=uiLanguage==Language::English ? L"MP4 videos\0*.mp4;*.m4v\0All files\0*.*\0" : uiLanguage==Language::Spanish ? L"Vídeos MP4\0*.mp4;*.m4v\0Todos los archivos\0*.*\0" : L"Vídeos MP4\0*.mp4;*.m4v\0Todos os arquivos\0*.*\0";
    of.lpstrFile=path; of.nMaxFile=MAX_PATH; of.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR;
    if (GetOpenFileNameW(&of)) openLocal(path);
}
static wstring inputText() {
    wchar_t value[2049]; GetWindowTextW(urlEdit,value,_countof(value));
    wstring input=value;
    size_t start=input.find_first_not_of(L" \t\r\n");
    return start==wstring::npos ? L"" : input.substr(start,input.find_last_not_of(L" \t\r\n")-start+1);
}
static void playYoutube(wstring id,const SearchResult& selected=SearchResult{}) {
    if (!mfReady || id.empty()) return;
    cancelSeek(); player.close(); download.clear(); KillTimer(window,1); KillTimer(window,2); current.clear();
    SearchResult entry=selected; entry.id=id; beginWatch(entry,true);
    if (!now.thumbnail) {
        try { preview.start(SearchKind::Preview,id,1,{}); SetTimer(window,8,300,nullptr); }
        catch (Problem p) { journal.add(p.operation); }
    }
    player.media.clear(); player.stats.clear();
    unsigned heights[]={720,600,480,360};
    int quality=static_cast<int>(SendMessageW(qualityBox,CB_GETCURSEL,0,0));
    try {
        download.start(id,heights[quality>=0 && quality<4 ? quality : 0],quality!=4,settings);
        player.status=download.status; SetTimer(window,2,350,nullptr);
    } catch (Problem p) { player.fail(p); }
    updateText(window,now.title.empty() ? L"AtomTube · YouTube" : L"AtomTube · "+now.title);
    refresh(); SetFocus(window);
}
static void watch() {
    if (download.active()) {
        download.clear(); KillTimer(window,2);
        player.status=L"Download cancelado."; refresh(); return;
    }
    wstring id=youtubeVideoId(inputText());
    if (id.empty()) {
        MessageBoxW(window,tr(L"Cole o link de um vídeo do YouTube."),L"AtomTube",MB_OK|MB_ICONINFORMATION);
        SetFocus(urlEdit); return;
    }
    playYoutube(id);
}
static void refreshResults() {
    if (searching) {
        static const wchar_t* frames[]={L"Pesquisando…",L"Pesquisando ·",L"Pesquisando ··"};
        updateText(resultCount,tr(frames[animation++%3]));
    } else updateText(resultCount,localMessage(resultStatus));
    updateText(searchButton,searching ? tr(L"Cancelar") : tr(L"Buscar"));
    updateText(resultPlay,resultKind==SearchKind::Channels ? tr(L"Abrir canal") : playlistRows(resultKind) ? tr(L"Abrir playlist") : tr(L"Assistir"));
    EnableWindow(resultPlay,mfReady && SendMessageW(resultList,LB_GETCURSEL,0,0)!=LB_ERR);
    EnableWindow(moreButton,!searching && nextResult>0);
}
static void searchQuery(bool more=false,SearchKind kind=SearchKind::Videos,wstring target=L"",wstring name=L"") {
    if (more && (searching || !nextResult)) return;
    wstring input=more ? resultQuery : target.empty() ? inputText() : target;
    if (input.empty()) { SetFocus(urlEdit); return; }
    wstring id=youtubeVideoId(input);
    if (!more && kind==SearchKind::Videos && !id.empty()) { playYoutube(id); return; }
    if (input.size()>204) { MessageBoxW(window,tr(L"Use até 180 caracteres para pesquisar."),L"AtomTube",MB_OK); return; }
    bool container=channelPage(kind) || kind==SearchKind::Entries;
    ShowWindow(recentList,SW_HIDE);
    if (!more && !container && id.empty()) remember(input);
    if (!more) {
        if (container && !resultQuery.empty() && !(channelPage(kind) && channelPage(resultKind) && resultQuery.substr(0,24)==input.substr(0,24))) {
            if (history.size()==3) history.erase(history.begin());
            history.push_back({resultKind,resultQuery,resultStatus,collectionName,channelName,nextResult,
                listIndex(LB_GETCURSEL),listIndex(LB_GETTOPINDEX),
                std::move(results),channelHeader});
        } else if (!container) history.clear();
        if (!channelPage(kind) || input.substr(0,24)!=resultQuery.substr(0,24)) channelHeader={};
        if (channelPage(kind) && !name.empty()) channelName=name;
        collectionName=name; resultKind=kind;
        updateText(resultTitle,name.empty() ? tr(L"Playlist") : name);
        SendMessageW(resultList,LB_RESETCONTENT,0,0); selectedColumn=0; results.clear(); resultQuery=input; nextResult=1;
    }
    drawer=false; page=Page::Catalog;
    try {
        search.start(resultKind,input,nextResult,results,settings.catalogLanguage());
        searching=true; resultStatus.clear(); SetTimer(window,3,250,nullptr);
    } catch (Problem p) { searching=false; resultStatus=p.operation; journal.add(resultStatus); }
    refreshResults(); layout();
}
static void selectedResult() {
    auto index=static_cast<size_t>(listIndex(LB_GETCURSEL));
    if (index>=results.size()) return;
    auto row=results[index];
    if (resultKind==SearchKind::Channels) searchQuery(false,SearchKind::Uploads,row.id,row.title);
    else if (playlistRows(resultKind)) searchQuery(false,SearchKind::Entries,row.id,row.title);
    else if (row.live==L"is_live" || row.live==L"is_upcoming")
        MessageBoxW(window,tr(L"Esta transmissão ainda está ao vivo ou agendada. Escolha uma gravação."),L"AtomTube",MB_OK);
    else playYoutube(row.id,row);
}
static void backToResults() {
    if (history.empty()) return;
    search.cancel(); KillTimer(window,3); searching=false;
    auto saved=std::move(history.back()); history.pop_back();
    resultQuery=saved.query; resultKind=saved.kind; results=std::move(saved.rows);
    nextResult=saved.next; resultStatus=saved.status; collectionName=saved.name; channelName=saved.channelName; channelHeader=saved.header;
    SendMessageW(resultList,LB_RESETCONTENT,0,0);
    updateText(resultTitle,collectionName); layout(); populateList(); restoreList(saved.selection,saved.top);
    refreshResults(); InvalidateRect(resultList,nullptr,FALSE); SetFocus(resultList);
}
static void exportLog() {
    wchar_t path[MAX_PATH];
    swprintf_s(path,L"AtomTube_Log_%s.txt",stamp(true).c_str());
    OPENFILENAMEW of={}; of.lStructSize=sizeof(of); of.hwndOwner=window;
    of.lpstrFilter=uiLanguage==Language::English ? L"Text files\0*.txt\0" : uiLanguage==Language::Spanish ? L"Archivo de texto\0*.txt\0" : L"Arquivo de texto\0*.txt\0"; of.lpstrFile=path; of.nMaxFile=MAX_PATH;
    of.lpstrDefExt=L"txt"; of.Flags=OFN_OVERWRITEPROMPT|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR;
    if (GetSaveFileNameW(&of)) {
        bool ok=journal.save(path);
        wstring msg=ok ? tr(L"Log salvo.") : fmt(tr(L"Falha ao salvar o log. Código Win32 %lu."),GetLastError());
        if (!ok) journal.add(msg);
        MessageBoxW(window,msg.c_str(),L"AtomTube",MB_OK|(ok ? MB_ICONINFORMATION : MB_ICONERROR));
    }
}
static HBITMAP grayPreview() {
    if (grayscale || !now.thumbnail) return grayscale;
    DIBSECTION d={}; if (GetObjectW(now.thumbnail.get(),sizeof(d),&d)!=sizeof(d) || !d.dsBm.bmBits) return nullptr;
    BITMAPINFO info={}; info.bmiHeader=d.dsBmih; void* bits=nullptr;
    info.bmiHeader.biHeight=-d.dsBm.bmHeight;
    grayscale=CreateDIBSection(nullptr,&info,DIB_RGB_COLORS,&bits,nullptr,0);
    if (grayscale) {
        auto src=static_cast<const DWORD*>(d.dsBm.bmBits),end=src+d.dsBm.bmWidth*d.dsBm.bmHeight;
        auto dst=static_cast<DWORD*>(bits);
        while(src!=end) { DWORD c=*src++; DWORD y=((c&255)*29+((c>>8)&255)*150+((c>>16)&255)*77)>>8; *dst++=y*0x010101; }
    }
    return grayscale;
}
static void drawScene(HDC dc,RECT r) {
    int w=r.right,h=r.bottom;
    Theme::fill(dc,r,Theme::background);
    bool catalog=page==Page::Catalog && !fullscreen;
    bool failed=!catalog && !download.active() && !player.active();
    auto image=Theme::Opening;
    int reveal=82;
    wstring label=player.status;
    if (catalog) { image=Theme::Searching; label=tr(L"Procurando resultados…"); }
    else if (failed) image=Theme::Error;
    else if (label.find(L"adicionais")!=wstring::npos) { image=Theme::Formats; reveal=35; }
    else if (label.find(L"Baixando")!=wstring::npos) { image=Theme::Downloading; reveal=53; }
    else if (label.find(L"Juntando")!=wstring::npos) { image=Theme::Merging; reveal=72; }
    else if (download.active()) { image=Theme::Tracks; reveal=12; }
    int side=std::min(112,std::max(46,h/2));
    auto tick=GetTickCount64();
    double phase=(tick%1500)*6.283185307/1500;
    int pulse=failed ? 0 : static_cast<int>(4.5-4.5*cos(phase));
    bool cover=!catalog && now.thumbnail && w>=340;
    int center=w/2,cy=h/2-16;
    int mx=center-(cover ? 100 : 17);
    Theme::art(dc,image,mx-side/2-pulse,cy-side/2-pulse,side+2*pulse,side+2*pulse);
    if (!failed) {
        int cx=mx+side/2+24;
        double angle=(tick%960)*6.283185307/960;
        for(int i=0;i<5;++i) {
            int x=cx+static_cast<int>(11*cos(angle-i*0.50)), y=cy+static_cast<int>(11*sin(angle-i*0.50));
            Theme::fill(dc,RECT{x-2,y-2,x+2,y+2},i==0 ? Theme::text : i<3 ? Theme::accent : Theme::line);
        }
    }
    if (cover) {
        RECT coverRect={center+10,cy-42,center+158,cy+42};
        Theme::bitmap(dc,grayPreview(),coverRect);
        if (!failed) {
            int saved=SaveDC(dc); IntersectClipRect(dc,coverRect.left,coverRect.top,coverRect.left+148*reveal/100,coverRect.bottom);
            Theme::bitmap(dc,static_cast<HBITMAP>(now.thumbnail.get()),coverRect); RestoreDC(dc,saved);
        }
    }
    SelectObject(dc,font);
    Theme::label(dc,localMessage(label),RECT{12,cy+side/2+10,w-12,h},Theme::text,DT_CENTER|DT_WORDBREAK|DT_END_ELLIPSIS);
}
static LRESULT CALLBACK videoProc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp) {
    static HDC buffer=nullptr;
    static HBITMAP bitmap=nullptr;
    static HGDIOBJ original=nullptr;
    static SIZE size={};
    if (hwnd==scene && msg==WM_ERASEBKGND) return 1;
    if (hwnd==scene && msg==WM_TIMER && wp==2) {
        RECT r; GetClientRect(hwnd,&r);
        int cx=r.right/2-((page!=Page::Catalog || fullscreen) && now.thumbnail && r.right>=340 ? 100 : 17);
        int cy=r.bottom/2-16;
        RECT region={cx-68,cy-68,cx+98,cy+68};
        if (!IsIconic(window)) InvalidateRect(hwnd,&region,FALSE);
        return 0;
    }
    if (hwnd==scene && msg==WM_DESTROY && buffer) {
        SelectObject(buffer,original); DeleteObject(bitmap); DeleteDC(buffer); buffer=nullptr; bitmap=nullptr;
    }
    if (msg==WM_PAINT) {
        PAINTSTRUCT ps; auto dc=BeginPaint(hwnd,&ps);
        if (hwnd==scene) {
            RECT r; GetClientRect(hwnd,&r);
            if (!buffer) buffer=CreateCompatibleDC(dc);
            if (buffer && (!bitmap || size.cx!=r.right || size.cy!=r.bottom)) {
                auto next=CreateCompatibleBitmap(dc,r.right,r.bottom);
                if (next) {
                    auto old=SelectObject(buffer,next);
                    if (bitmap) DeleteObject(bitmap); else original=old;
                    bitmap=next; size={r.right,r.bottom};
                }
            }
            if (bitmap && size.cx==r.right && size.cy==r.bottom) {
                RECT p=ps.rcPaint; int save=SaveDC(buffer);
                IntersectClipRect(buffer,p.left,p.top,p.right,p.bottom);
                drawScene(buffer,r); RestoreDC(buffer,save);
                BitBlt(dc,p.left,p.top,p.right-p.left,p.bottom-p.top,buffer,p.left,p.top,SRCCOPY);
            } else drawScene(dc,r);
        }
        else player.repaint();
        EndPaint(hwnd,&ps); return 0;
    }
    if (hwnd!=scene) {
        if (msg==WM_LBUTTONDOWN) {
            SetFocus(window);
            SetTimer(hwnd,1,GetDoubleClickTime(),nullptr); return 0;
        }
        if (msg==WM_TIMER && wp==1) { KillTimer(hwnd,1); SendMessageW(window,WM_COMMAND,Play,0); return 0; }
        if (msg==WM_LBUTTONDBLCLK) { KillTimer(hwnd,1); toggleFullscreen(); return 0; }
    }
    return DefWindowProcW(hwnd,msg,wp,lp);
}
static WNDPROC checkProc;
static WNDPROC editProc;
static void pickRecent() {
    int i=static_cast<int>(SendMessageW(recentList,LB_GETCURSEL,0,0));
    wchar_t term[205]={};
    if (i>=0 && SendMessageW(recentList,LB_GETTEXTLEN,i,0)<_countof(term)) {
        SendMessageW(recentList,LB_GETTEXT,i,reinterpret_cast<LPARAM>(term));
        SetWindowTextW(urlEdit,term); SetFocus(window); searchQuery();
    }
}
static WNDPROC recentProc;
static LRESULT CALLBACK recentInput(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp) {
    if (msg==WM_MOUSEACTIVATE) return MA_NOACTIVATE;
    if (msg==WM_LBUTTONDOWN || msg==WM_LBUTTONUP) {
        auto hit=SendMessageW(hwnd,LB_ITEMFROMPOINT,0,lp);
        if (msg==WM_LBUTTONDOWN) SetCapture(hwnd);
        else if (GetCapture()==hwnd) ReleaseCapture();
        if (!HIWORD(hit)) {
            SendMessageW(hwnd,LB_SETCURSEL,LOWORD(hit),0);
            if (msg==WM_LBUTTONUP) pickRecent();
        }
        return 0;
    }
    return CallWindowProcW(recentProc,hwnd,msg,wp,lp);
}
static LRESULT CALLBACK searchEdit(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp) {
    auto result=CallWindowProcW(editProc,hwnd,msg,wp,lp);
    if (hwnd==urlEdit && recentList) {
        if (msg==WM_SETFOCUS || msg==WM_LBUTTONDOWN) showRecent(true);
        if (msg==WM_KILLFOCUS && reinterpret_cast<HWND>(wp)!=recentList) ShowWindow(recentList,SW_HIDE);
    }
    if ((msg==WM_PAINT || msg==WM_PRINTCLIENT) && GetFocus()!=hwnd && !GetWindowTextLengthW(hwnd)) {
        HDC dc=msg==WM_PRINTCLIENT ? reinterpret_cast<HDC>(wp) : GetDC(hwnd);
        if (dc) {
            RECT r; GetClientRect(hwnd,&r); Theme::fill(dc,r,Theme::panel);
            auto old=SelectObject(dc,font); r.left+=2;
            Theme::label(dc,hwnd==urlEdit ? tr(L"Pesquisar ou colar link") : tr(L"Pesquisar neste canal"),r,
                Theme::muted,DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS);
            SelectObject(dc,old); if (msg==WM_PAINT) ReleaseDC(hwnd,dc);
        }
    }
    if (msg==WM_SETFOCUS || msg==WM_KILLFOCUS) {
        InvalidateRect(hwnd,nullptr,TRUE);
        RECT r; GetWindowRect(hwnd,&r); MapWindowPoints(HWND_DESKTOP,window,reinterpret_cast<POINT*>(&r),2);
        InflateRect(&r,14,9); InvalidateRect(window,&r,FALSE);
    }
    return result;
}
static LRESULT CALLBACK darkCheck(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp) {
    if (msg==WM_PAINT) {
        PAINTSTRUCT ps; auto dc=BeginPaint(hwnd,&ps); RECT r; GetClientRect(hwnd,&r);
        bool checked=SendMessageW(hwnd,BM_GETCHECK,0,0)==BST_CHECKED;
        Theme::fill(dc,r,Theme::panel);
        int y=r.bottom/2-7;
        Theme::fill(dc,RECT{5,y,19,y+14},GetFocus()==hwnd || checked ? Theme::accent : Theme::muted);
        Theme::fill(dc,RECT{6,y+1,18,y+13},checked ? Theme::accent : Theme::raised);
        if (checked) {
            auto pen=CreatePen(PS_SOLID,2,Theme::ink);
            auto old=SelectObject(dc,pen);
            POINT tick[]={{8,y+7},{11,y+10},{16,y+4}}; Polyline(dc,tick,_countof(tick));
            SelectObject(dc,old); DeleteObject(pen);
        }
        wchar_t caption[80]; GetWindowTextW(hwnd,caption,_countof(caption));
        SelectObject(dc,font); r.left=25;
        Theme::label(dc,caption,r,Theme::text,DT_SINGLELINE|DT_VCENTER);
        EndPaint(hwnd,&ps); return 0;
    }
    if (msg==WM_ERASEBKGND) return 1;
    auto result=CallWindowProcW(checkProc,hwnd,msg,wp,lp);
    if (msg==BM_SETCHECK || msg==WM_SETFOCUS || msg==WM_KILLFOCUS || msg==WM_LBUTTONUP || msg==WM_KEYUP)
        InvalidateRect(hwnd,nullptr,FALSE);
    return result;
}
static wstring joined(wstring a,const wstring& b) {
    if (b.empty()) return a; return a.empty() ? b : a+L"   ·   "+b;
}
static wstring prettyDate(wstring s) {
    if (s.size()==8 && s.find_first_not_of(L"0123456789")==wstring::npos)
        return s.substr(6,2)+L"/"+s.substr(4,2)+L"/"+s.substr(0,4);
    return s;
}
static wstring prettyViews(wstring s) {
    if (s.empty()) return {};
    if (s.find(L"visualiza")!=wstring::npos || s.find(L"view")!=wstring::npos || s.find(L"vistas")!=wstring::npos) return s;
    if (s.find_first_not_of(L"0123456789")==wstring::npos)
        for (int i=static_cast<int>(s.size())-3;i>0;i-=3) s.insert(i,1,L'.');
    return s+tr(L" visualizações");
}
static void avatar(HDC dc,RECT rect,const std::shared_ptr<void>& photo,const wstring& name) {
    if (photo) { Theme::bitmap(dc,static_cast<HBITMAP>(photo.get()),rect,true); return; }
    auto brush=SelectObject(dc,GetStockObject(DC_BRUSH)); auto pen=SelectObject(dc,GetStockObject(NULL_PEN));
    SetDCBrushColor(dc,Theme::raised); Ellipse(dc,rect.left,rect.top,rect.right,rect.bottom);
    SelectObject(dc,pen); SelectObject(dc,brush);
    Theme::label(dc,name.empty() ? L"@" : name.substr(0,1),rect,Theme::accent,DT_SINGLELINE|DT_CENTER|DT_VCENTER);
}
static void drawBadge(HDC dc,RECT tile,wstring badge,bool playlist) {
    if (badge.empty()) return;
    if (playlist && badge.find_first_not_of(L"0123456789")==wstring::npos) badge+=tr(L" vídeos");
    SelectObject(dc,font);
    SIZE size={}; GetTextExtentPoint32W(dc,badge.c_str(),static_cast<int>(badge.size()),&size);
    RECT tag={std::max(tile.left,tile.right-size.cx-8),tile.bottom-19,tile.right,tile.bottom};
    Theme::fill(dc,tag,Theme::background);
    Theme::label(dc,badge,tag,Theme::text,DT_SINGLELINE|DT_CENTER|DT_VCENTER);
}
static void drawChannelCards(const DRAWITEMSTRUCT& item) {
    HDC dc=item.hDC; const RECT r=item.rcItem; Theme::fill(dc,r,Theme::background);
    for (int col=0;col<listColumns;++col) {
        size_t i=static_cast<size_t>(item.itemID)*listColumns+col;
        if (i>=results.size()) break;
        const auto& row=results[i];
        RECT card={r.left+(r.right-r.left)*col/listColumns+3,r.top+2,
            r.left+(r.right-r.left)*(col+1)/listColumns-3,r.bottom-4};
        bool selected=(item.itemState&ODS_SELECTED) && selectedColumn==col;
        Theme::rounded(dc,card,selected ? Theme::raised : Theme::panel,selected ? Theme::accent : Theme::panel,8);
        LONG width=std::min(172L,card.right-card.left-8),x=(card.left+card.right-width)/2;
        RECT tile={x,card.top+4,x+width,card.top+4+width*9/16};
        Theme::bitmap(dc,static_cast<HBITMAP>(row.thumbnail.get()),tile);
        bool playlist=playlistRows(resultKind);
        drawBadge(dc,tile,playlist ? row.count : row.duration,playlist);
        SelectObject(dc,font);
        RECT line={card.left+7,tile.bottom+1,card.right-5,tile.bottom+21};
        int fits=0; SIZE extent={};
        GetTextExtentExPointW(dc,row.title.c_str(),static_cast<int>(row.title.size()),
            line.right-line.left,&fits,nullptr,&extent);
        size_t split=static_cast<size_t>(fits);
        if (split<row.title.size()) {
            auto space=row.title.rfind(L' ',split);
            if (space!=wstring::npos && space>0) split=space;
            else if (split && IS_HIGH_SURROGATE(row.title[split-1])) --split;
        }
        Theme::label(dc,row.title.substr(0,split),line,Theme::text,DT_SINGLELINE);
        auto second=row.title.find_first_not_of(L' ',split);
        if (second!=wstring::npos) {
            OffsetRect(&line,0,20);
            Theme::label(dc,row.title.substr(second),line,Theme::text,DT_SINGLELINE|DT_END_ELLIPSIS);
        }
        Theme::label(dc,row.date.empty() ? prettyViews(row.views) : prettyDate(row.date),
            RECT{card.left+7,tile.bottom+41,card.right-5,card.bottom},Theme::muted,DT_SINGLELINE|DT_END_ELLIPSIS);
    }
}
static void drawRow(const DRAWITEMSTRUCT& item) {
    RECT r=item.rcItem; HDC dc=item.hDC;
    bool compact=item.CtlID==RelatedList;
    if (!compact && listColumns>1) { drawChannelCards(item); return; }
    const auto& rows=compact ? suggestions : results;
    bool channel=!compact && resultKind==SearchKind::Channels;
    Theme::fill(dc,r,item.itemState&ODS_SELECTED ? Theme::raised : Theme::background);
    if (item.itemID>=rows.size()) return;
    const auto& row=rows[item.itemID];
    int x=r.left+8,y=r.top+5,iw=channel ? 72 : compact ? 90 : 142;
    int ih=channel ? 72 : compact ? 52 : 80;
    RECT tile={x,y,x+iw,y+ih}; SelectObject(dc,font);
    if (channel) avatar(dc,tile,row.thumbnail,row.title);
    else {
        Theme::fill(dc,tile,Theme::panel);
        Theme::bitmap(dc,static_cast<HBITMAP>(row.thumbnail.get()),tile);
        bool playlist=playlistRows(resultKind) && !compact;
        drawBadge(dc,tile,playlist ? row.count : row.duration,playlist);
    }
    int left=x+iw+12;
    SelectObject(dc,compact ? font : subhead);
    Theme::label(dc,row.title+(channel && row.verified ? L"  ✓" : L""),RECT{left,y,r.right-9,y+(compact ? 44 : 34)},
        Theme::text,compact ? DT_WORDBREAK|DT_END_ELLIPSIS|DT_EDITCONTROL : DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS);
    SelectObject(dc,font);
    if (compact) {
        Theme::label(dc,row.author,RECT{left,y+42,r.right-6,r.bottom},Theme::muted,DT_SINGLELINE|DT_END_ELLIPSIS);
    } else if (channel) {
        Theme::label(dc,row.subscribers,RECT{left,y+31,r.right-9,y+54},Theme::accent,DT_SINGLELINE|DT_END_ELLIPSIS);
        Theme::label(dc,row.description,RECT{left,y+56,r.right-9,r.bottom},Theme::muted,DT_SINGLELINE|DT_END_ELLIPSIS);
    } else {
        Theme::label(dc,row.author,RECT{left+35,y+36,r.right-8,y+58},Theme::accent,DT_SINGLELINE|DT_END_ELLIPSIS);
        Theme::label(dc,joined(prettyDate(row.date),prettyViews(row.views)),RECT{left+35,y+60,r.right-8,r.bottom},Theme::muted,DT_SINGLELINE|DT_END_ELLIPSIS);
        avatar(dc,RECT{left,y+38,left+27,y+65},row.avatar,row.author);
    }
    Theme::fill(dc,RECT{r.left+8,r.bottom-1,r.right-8,r.bottom},Theme::panel);
    if (item.itemState&ODS_FOCUS) DrawFocusRect(dc,&item.rcItem);
}
static WNDPROC listProc;
static LRESULT CALLBACK catalogProc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp) {
    if (hwnd==relatedList) {
        auto result=CallWindowProcW(listProc,hwnd,msg,wp,lp);
        if (msg==WM_LBUTTONUP) {
            auto hit=SendMessageW(hwnd,LB_ITEMFROMPOINT,0,lp);
            RECT row;
            if (!HIWORD(hit) && LOWORD(hit)<suggestions.size() &&
                SendMessageW(hwnd,LB_GETITEMRECT,LOWORD(hit),reinterpret_cast<LPARAM>(&row))!=LB_ERR &&
                static_cast<short>(HIWORD(lp))<row.bottom)
                SendMessageW(window,WM_COMMAND,MAKEWPARAM(RelatedList,LBN_DBLCLK),0);
        }
        return result;
    }
    if (listColumns>1 && msg==WM_KEYDOWN && (wp==VK_LEFT || wp==VK_RIGHT || wp==VK_UP || wp==VK_DOWN) && !results.empty()) {
        int i=listIndex(LB_GETCURSEL);
        int delta=wp==VK_LEFT ? -1 : wp==VK_RIGHT ? 1 : wp==VK_UP ? -listColumns : listColumns;
        i=std::max(0,std::min(static_cast<int>(results.size())-1,i<0 ? 0 : i+delta));
        selectedColumn=i%listColumns; SendMessageW(hwnd,LB_SETCURSEL,i/listColumns,0);
        InvalidateRect(hwnd,nullptr,FALSE); refreshResults(); return 0;
    }
    if (msg==WM_LBUTTONUP) {
        auto hit=SendMessageW(hwnd,LB_ITEMFROMPOINT,0,lp);
        unsigned index=LOWORD(hit); RECT row;
        if (!HIWORD(hit) && index<results.size() && SendMessageW(hwnd,LB_GETITEMRECT,index,reinterpret_cast<LPARAM>(&row))!=LB_ERR) {
            int x=static_cast<short>(LOWORD(lp))-row.left, y=static_cast<short>(HIWORD(lp))-row.top;
            if (listColumns>1) {
                auto result=CallWindowProcW(listProc,hwnd,msg,wp,lp);
                if (x>=0 && y>=0 && y<row.bottom-row.top && x<row.right-row.left) {
                    selectedColumn=x*listColumns/(row.right-row.left);
                    selectedResult();
                }
                return result;
            }
            SIZE extent={}; HDC dc=GetDC(hwnd); auto old=SelectObject(dc,font);
            GetTextExtentPoint32W(dc,results[index].author.c_str(),static_cast<int>(results[index].author.size()),&extent);
            SelectObject(dc,old); ReleaseDC(hwnd,dc);
            auto result=CallWindowProcW(listProc,hwnd,msg,wp,lp);
            if (resultKind!=SearchKind::Channels && x>=162 && x<197+extent.cx && y>=40 && y<=69 && !results[index].channel.empty())
                searchQuery(false,SearchKind::Uploads,results[index].channel,results[index].author);
            else if (y>=0 && y<row.bottom-row.top) selectedResult();
            return result;
        }
    }
    return CallWindowProcW(listProc,hwnd,msg,wp,lp);
}
static void drawCombo(const DRAWITEMSTRUCT& item) {
    RECT r=item.rcItem;
    Theme::fill(item.hDC,r,item.itemState&ODS_SELECTED ? Theme::raised : Theme::panel);
    if (item.itemID==UINT(-1)) return;
    wchar_t label[96]={};
    SendMessageW(item.hwndItem,CB_GETLBTEXT,item.itemID,reinterpret_cast<LPARAM>(label));
    r.left+=5;
    SelectObject(item.hDC,font);
    Theme::label(item.hDC,label,r,Theme::text,DT_SINGLELINE|DT_VCENTER);
    if (item.itemState&ODS_FOCUS) DrawFocusRect(item.hDC,&item.rcItem);
}
static void fillOptions(HWND box,std::initializer_list<const wchar_t*> items,int selected) {
    SendMessageW(box,CB_RESETCONTENT,0,0);
    for(auto item:items) SendMessageW(box,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(tr(item)));
    SendMessageW(box,CB_SETCURSEL,selected,0);
}
static void localizeControls() {
    for(const auto& item:captions) SetWindowTextW(item.hwnd,tr(item.pt));
    SetWindowTextW(recentList,tr(L"Pesquisas recentes"));
    if (!collectionName.empty()) SetWindowTextW(resultTitle,collectionName.c_str());
    if (!suggestions.empty()) SetWindowTextW(relatedTitle,relatedFromChannel ? tr(L"Mais deste canal") : tr(L"Relacionados"));
    fillOptions(imageBox,{L"Original",L"Alta",L"Média",L"Baixa",L"Batata"},static_cast<int>(settings.image));
    fillOptions(fpsBox,{L"Original da fonte",L"60 fps",L"50 fps",L"40 fps",L"35 fps",L"30 fps",L"24 fps",L"17 fps"},fpsIndex);
    fillOptions(langBox,{L"Português (Brasil)",L"English",L"Español"},static_cast<int>(uiLanguage));
    fillOptions(modeBox,{L"Preferir GPU · DXVA2",L"Usar CPU"},static_cast<int>(SendMessageW(modeBox,CB_GETCURSEL,0,0)));
    fillOptions(qualityBox,{L"Até 720p",L"Tela 1024 × 600",L"Até 480p",L"Até 360p",L"MP4 completo"},static_cast<int>(SendMessageW(qualityBox,CB_GETCURSEL,0,0)));
    SendMessageW(urlEdit,EM_SETCUEBANNER,FALSE,reinterpret_cast<LPARAM>(tr(L"Pesquisar ou colar link")));
    SendMessageW(channelField,EM_SETCUEBANNER,FALSE,reinterpret_cast<LPARAM>(tr(L"Pesquisar neste canal")));
}
static LRESULT CALLBACK appProc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp) {
    switch (msg) {
    case WM_CREATE: {
        window=hwnd;
        settings.language=uiLanguage=static_cast<Language>(preference(L"Language",0,2));
        settings.image=static_cast<ImageQuality>(preference(L"ImageProfile",2,4));
        fpsIndex=preference(L"TargetFpsIndex",6,7); settings.fps=frameTargets[fpsIndex];
        font=CreateFontW(-15,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
        heading=CreateFontW(-20,0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
        subhead=CreateFontW(-18,0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
        wordmark=CreateFontW(-62,0,0,0,FW_HEAVY,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
        homeForYou=widget(L"BUTTON",L"Para você",WS_TABSTOP|BS_OWNERDRAW,ForYouTab);
        homeSaved=widget(L"BUTTON",L"Ver no AtomTube",WS_TABSTOP|BS_OWNERDRAW,SavedTab);
        homeSubs=widget(L"BUTTON",L"Inscrições",WS_TABSTOP|BS_OWNERDRAW,SubsTab);
        homeAccount=widget(L"BUTTON",L"",WS_TABSTOP|BS_OWNERDRAW,AccountButton);
        homePrev=widget(L"BUTTON",L"‹",WS_TABSTOP|BS_OWNERDRAW,FeedPrev);
        homeNext=widget(L"BUTTON",L"›",WS_TABSTOP|BS_OWNERDRAW,FeedNext);
        homeRetry=widget(L"BUTTON",L"Tentar de novo",WS_TABSTOP|BS_OWNERDRAW,FeedRetry);
        homeStatus=widget(L"STATIC",L"",SS_CENTERIMAGE|SS_ENDELLIPSIS|SS_NOPREFIX,0);
        for(int i=0;i<4;++i) homeCards[i]=widget(L"BUTTON",L"",WS_TABSTOP|BS_OWNERDRAW,FirstCard+i);
        connected=accountExists(); setFeedText();
        menuButton=widget(L"BUTTON",L"Configurações extras",WS_TABSTOP|BS_OWNERDRAW,ExtraMenu);
        imageLabel=widget(L"STATIC",L"Qualidade da imagem",SS_LEFT,0);
        fpsLabel=widget(L"STATIC",L"FPS alvo",SS_LEFT,0);
        langLabel=widget(L"STATIC",L"Idioma",SS_LEFT,0);
        noteLabel=widget(L"STATIC",L"Imagem e FPS valem para o próximo vídeo.",SS_LEFT,0);
        const DWORD choice=WS_TABSTOP|CBS_DROPDOWNLIST|CBS_HASSTRINGS|CBS_OWNERDRAWFIXED;
        imageBox=widget(L"COMBOBOX",L"",choice,ImageChoice);
        fpsBox=widget(L"COMBOBOX",L"",choice,FpsChoice);
        langBox=widget(L"COMBOBOX",L"",choice,LanguageChoice);
        openButton=widget(L"BUTTON",L"Abrir MP4",WS_TABSTOP|BS_OWNERDRAW,Open);
        modeLabel=widget(L"STATIC",L"Decodificação",SS_CENTERIMAGE,0);
        modeBox=widget(L"COMBOBOX",L"",WS_TABSTOP|CBS_DROPDOWNLIST|CBS_OWNERDRAWFIXED|CBS_HASSTRINGS,Mode);
        SendMessageW(modeBox,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(tr(L"Preferir GPU · DXVA2")));
        SendMessageW(modeBox,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(tr(L"Usar CPU")));
        SendMessageW(modeBox,CB_SETCURSEL,0,0);
        playButton=widget(L"BUTTON",L"Pausar",WS_TABSTOP|BS_OWNERDRAW,Play);
        fullscreenButton=widget(L"BUTTON",L"Tela cheia",WS_TABSTOP|BS_OWNERDRAW,Fullscreen);
        showStats=widget(L"BUTTON",L"Diagnóstico",WS_TABSTOP|BS_AUTOCHECKBOX,ShowStats);
        edgeSmoothing=widget(L"BUTTON",L"Suavização de bordas",WS_TABSTOP|BS_AUTOCHECKBOX,EdgeSmoothing);
        SendMessageW(edgeSmoothing,BM_SETCHECK,BST_CHECKED,0);
        checkProc=reinterpret_cast<WNDPROC>(SetWindowLongPtrW(showStats,GWLP_WNDPROC,reinterpret_cast<LONG_PTR>(darkCheck)));
        SetWindowLongPtrW(edgeSmoothing,GWLP_WNDPROC,reinterpret_cast<LONG_PTR>(darkCheck));
        logButton=widget(L"BUTTON",L"Exportar Log (.txt)",WS_TABSTOP|BS_OWNERDRAW,Export);
        urlEdit=widget(L"EDIT",L"",WS_TABSTOP|ES_AUTOHSCROLL,Url);
        SendMessageW(urlEdit,EM_SETLIMITTEXT,2048,0);
        SendMessageW(urlEdit,EM_SETMARGINS,EC_LEFTMARGIN|EC_RIGHTMARGIN,0);
        SendMessageW(urlEdit,EM_SETCUEBANNER,FALSE,reinterpret_cast<LPARAM>(tr(L"Pesquisar ou colar link")));
        settingsQuality=widget(L"STATIC",L"Resolução",SS_CENTERIMAGE,0);
        qualityBox=widget(L"COMBOBOX",L"",WS_TABSTOP|CBS_DROPDOWNLIST|CBS_OWNERDRAWFIXED|CBS_HASSTRINGS,QualityChoice);
        for (auto label:{L"Até 720p",L"Tela 1024 × 600",L"Até 480p",L"Até 360p",L"MP4 completo"})
            SendMessageW(qualityBox,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(label));
        SendMessageW(qualityBox,CB_SETCURSEL,0,0);
        watchButton=widget(L"BUTTON",L"Abrir link",WS_TABSTOP|BS_OWNERDRAW,Watch);
        searchButton=widget(L"BUTTON",L"Buscar",WS_TABSTOP|BS_OWNERDRAW,Search);
        channelsButton=widget(L"BUTTON",L"Canais",WS_TABSTOP|BS_OWNERDRAW,FindChannels);
        playingChannel=widget(L"BUTTON",L"Canal do vídeo",WS_TABSTOP|BS_OWNERDRAW,PlayingChannel);
        backButton=widget(L"BUTTON",L"Voltar",WS_TABSTOP|BS_OWNERDRAW,Back);
        resultToggle=widget(L"BUTTON",L"Ocultar lista",WS_TABSTOP|BS_OWNERDRAW,Browse);
        resultTitle=widget(L"STATIC",L"Resultados",SS_CENTERIMAGE|SS_NOPREFIX|SS_ENDELLIPSIS,0);
        SendMessageW(resultTitle,WM_SETFONT,reinterpret_cast<WPARAM>(heading),FALSE);
        resultCount=widget(L"STATIC",L"",SS_CENTERIMAGE|SS_ENDELLIPSIS|SS_NOPREFIX,0);
        resultList=widget(L"LISTBOX",L"",WS_TABSTOP|WS_VSCROLL|LBS_OWNERDRAWFIXED|LBS_NOINTEGRALHEIGHT|LBS_NOTIFY,Results);
        listProc=reinterpret_cast<WNDPROC>(SetWindowLongPtrW(resultList,GWLP_WNDPROC,reinterpret_cast<LONG_PTR>(catalogProc)));
        brand=widget(L"BUTTON",L"AtomTube · Início",WS_TABSTOP|BS_OWNERDRAW,Home);
        videoTab=widget(L"BUTTON",L"Vídeos",WS_TABSTOP|BS_OWNERDRAW,VideoTab);
        playlistTab=widget(L"BUTTON",L"Playlists",WS_TABSTOP|BS_OWNERDRAW,PlaylistTab);
        liveTab=widget(L"BUTTON",L"Ao vivo",WS_TABSTOP|BS_OWNERDRAW,LiveTab);
        channelField=widget(L"EDIT",L"",WS_TABSTOP|WS_BORDER|ES_AUTOHSCROLL,ChannelField);
        SendMessageW(channelField,EM_SETLIMITTEXT,180,0);
        SendMessageW(channelField,EM_SETCUEBANNER,FALSE,reinterpret_cast<LPARAM>(tr(L"Pesquisar neste canal")));
        editProc=reinterpret_cast<WNDPROC>(SetWindowLongPtrW(urlEdit,GWLP_WNDPROC,reinterpret_cast<LONG_PTR>(searchEdit)));
        SetWindowLongPtrW(channelField,GWLP_WNDPROC,reinterpret_cast<LONG_PTR>(searchEdit));
        withinButton=widget(L"BUTTON",L"No canal",WS_TABSTOP|BS_OWNERDRAW,Within);
        nowTitle=widget(L"STATIC",L"",SS_LEFT|SS_NOPREFIX,0);
        SendMessageW(nowTitle,WM_SETFONT,reinterpret_cast<WPARAM>(subhead),FALSE);
        relatedTitle=widget(L"STATIC",L"Relacionados",SS_CENTERIMAGE|SS_NOPREFIX,0);
        SendMessageW(relatedTitle,WM_SETFONT,reinterpret_cast<WPARAM>(subhead),FALSE);
        relatedList=widget(L"LISTBOX",L"",WS_TABSTOP|WS_VSCROLL|LBS_OWNERDRAWFIXED|LBS_NOINTEGRALHEIGHT|LBS_NOTIFY,RelatedList);
        SetWindowLongPtrW(relatedList,GWLP_WNDPROC,reinterpret_cast<LONG_PTR>(catalogProc));
        resultPlay=widget(L"BUTTON",L"Assistir",WS_TABSTOP|BS_OWNERDRAW,ResultPlay);
        moreButton=widget(L"BUTTON",L"Carregar mais",WS_TABSTOP|BS_OWNERDRAW,MoreResults);
        player.video=widget(L"AtomTubeVideo",L"",0,0);
        scene=widget(L"AtomTubeScene",L"",0,0);
        progressBar=widget(L"AtomTubeSlider",L"Posição",WS_TABSTOP,Progress);
        volumeBar=widget(L"AtomTubeSlider",L"Volume",WS_TABSTOP,Volume);
        muteButton=widget(L"BUTTON",L"Vol 100%",WS_TABSTOP|BS_OWNERDRAW,Mute);
        timeLabel=widget(L"STATIC",L"",SS_CENTERIMAGE|SS_ENDELLIPSIS|SS_NOPREFIX,PlaybackTime);
        activity=widget(L"STATIC",L"",SS_CENTERIMAGE|SS_NOPREFIX,0);
        summary=widget(L"STATIC",L"",SS_CENTERIMAGE|SS_ENDELLIPSIS|SS_NOPREFIX,PlaybackStatus);
        diagnostic=widget(L"STATIC",L"",SS_LEFT|SS_NOPREFIX,0);
        recentList=CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE,L"LISTBOX",L"Pesquisas recentes",
            WS_POPUP|WS_BORDER|WS_VSCROLL|LBS_NOINTEGRALHEIGHT,0,0,0,0,hwnd,nullptr,GetModuleHandleW(nullptr),nullptr);
        SendMessageW(recentList,WM_SETFONT,reinterpret_cast<WPARAM>(font),FALSE);
        recentProc=reinterpret_cast<WNDPROC>(SetWindowLongPtrW(recentList,GWLP_WNDPROC,reinterpret_cast<LONG_PTR>(recentInput)));
        ShowWindow(recentList,SW_HIDE); readRecent();
        SendMessageW(qualityBox,CB_SETCURSEL,preference(L"Quality",0,4),0);
        SendMessageW(modeBox,CB_SETCURSEL,preference(L"Decoder",0,1),0);
        SendMessageW(edgeSmoothing,BM_SETCHECK,preference(L"Smoothing",1,1),0);
        SendMessageW(showStats,BM_SETCHECK,preference(L"Diagnostics",0,1),0);
        restoreVolume=std::max(1,preference(L"UnmutedVolume",100,100));
        localizeControls();
        changeVolume(preference(L"Volume",100,100));
        EnableWindow(openButton,mfReady); EnableWindow(modeBox,mfReady);
        if (mfReady) player.status=L"Espaço pausa  ·  ← → pulam 10 s  ·  ↑ ↓ ajustam volume  ·  F11 tela cheia";
        refresh(); refreshResults(); return 0;
    }
    case WM_GETMINMAXINFO:
        reinterpret_cast<MINMAXINFO*>(lp)->ptMinTrackSize=POINT{760,440}; return 0;
    case WM_WINDOWPOSCHANGING: {
        auto pos=reinterpret_cast<WINDOWPOS*>(lp);
        if (!(pos->flags&SWP_NOSIZE)) pos->flags|=SWP_NOCOPYBITS;
        break;
    }
    case WM_SIZE:
        if (wp==SIZE_MINIMIZED) animateScene(false); else if (player.video) layout();
        return 0;
    case WM_MOVE: if (recentList) ShowWindow(recentList,SW_HIDE); break;
    case WM_LBUTTONDOWN: if (recentList && IsWindowVisible(recentList)) SetFocus(hwnd); break;
    case WM_ACTIVATE: if (LOWORD(wp)==WA_INACTIVE && recentList) ShowWindow(recentList,SW_HIDE); break;
    case WM_EXITSIZEMOVE:
        RedrawWindow(hwnd,nullptr,nullptr,RDW_INVALIDATE|RDW_ERASE|RDW_ALLCHILDREN);
        return 0;
    case WM_DROPFILES: {
        auto drop=reinterpret_cast<HDROP>(wp);
        wchar_t path[MAX_PATH];
        UINT needed=DragQueryFileW(drop,0,nullptr,0);
        bool valid=needed && needed<MAX_PATH && DragQueryFileW(drop,0,path,_countof(path));
        DragFinish(drop);
        if (valid) openLocal(path);
        return 0;
    }
    case WM_CTLCOLORSTATIC: case WM_CTLCOLOREDIT: case WM_CTLCOLORLISTBOX: {
        auto dc=reinterpret_cast<HDC>(wp);
        auto control=reinterpret_cast<HWND>(lp);
        bool panel=msg==WM_CTLCOLOREDIT || control==recentList || control==timeLabel || control==modeBox || control==qualityBox;
        SetTextColor(dc,control==activity ? Theme::accent :
            (control==summary || control==diagnostic || control==resultCount ? Theme::muted : Theme::text));
        SetBkColor(dc,panel ? Theme::panel : Theme::background);
        return reinterpret_cast<LRESULT>(panel ? panelBrush : backgroundBrush);
    }
    case WM_PAINT: {
        PAINTSTRUCT ps; auto dc=BeginPaint(hwnd,&ps); RECT r; GetClientRect(hwnd,&r);
        Theme::fill(dc,r,fullscreen ? RGB(0,0,0) : Theme::background);
        if (!fullscreen && !(page==Page::Home && drawer && r.right<850)) {
            bool home=page==Page::Home;
            int left=home ? (r.right+(drawer ? 230 : 0)-564)/2 : 151;
            int top=home ? homeSearchY(r.bottom) : 10;
            RECT field={left,top,home ? left+562 : r.right-210,top+(home ? 35 : 32)};
            Theme::rounded(dc,field,Theme::panel,GetFocus()==urlEdit ? Theme::accent : Theme::muted,13);
        }
        if (!fullscreen && page==Page::Home) {
            if (drawer && r.right<850) { EndPaint(hwnd,&ps); return 0; }
            if (drawer) Theme::fill(dc,RECT{229,0,230,r.bottom},Theme::line);
            int x=(r.right+(drawer ? 230 : 0))/2-198,y=homeSearchY(r.bottom)-108;
            if (r.bottom<490) {
                y=homeSearchY(r.bottom)-52; Theme::art(dc,Theme::App,x+92,y,42,42);
                SelectObject(dc,heading);
                Theme::label(dc,L"AtomTube",RECT{x+143,y,x+307,y+42},Theme::text,DT_SINGLELINE|DT_VCENTER);
            } else {
            Theme::art(dc,Theme::App,x,y,91,91);
            SelectObject(dc,wordmark);
            Theme::label(dc,L"AtomTube",RECT{x+101,y-6,x+443,y+65},Theme::text,DT_SINGLELINE|DT_VCENTER);
            SelectObject(dc,font);
            Theme::label(dc,tr(L"Trazendo seu Atom antigo de volta à Web"),RECT{x+104,y+64,x+452,y+88},
                Theme::muted,DT_SINGLELINE|DT_VCENTER);
            }
            Theme::art(dc,Theme::Inside,12+(drawer ? 230 : 0),r.bottom-46,42,38);
            Theme::label(dc,L"Atomzinho inside",RECT{55+(drawer ? 230 : 0),r.bottom-33,169+(drawer ? 230 : 0),r.bottom-9},Theme::muted,DT_CENTER|DT_SINGLELINE);
        } else if (!fullscreen) {
            Theme::fill(dc,RECT{0,99,r.right,100},Theme::line);
            if (page==Page::Catalog && channelPage(resultKind)) {
                wstring name=channelHeader.name.empty() ? channelName : channelHeader.name;
                SelectObject(dc,heading);
                SIZE size={}; GetTextExtentPoint32W(dc,name.c_str(),static_cast<int>(name.size()),&size);
                RECT tag={89,106,std::min<LONG>(r.right/3,98+size.cx),177};
                avatar(dc,RECT{16,110,80,174},channelHeader.avatar,name);
                Theme::label(dc,name,tag,Theme::text,DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
                LONG limit=r.right-((player.active() || download.active()) ? 233 : 115);
                BITMAP bm={}; auto banner=static_cast<HBITMAP>(channelHeader.banner.get());
                if (banner && GetObjectW(banner,sizeof(bm),&bm) && bm.bmHeight>0) {
                    LONG width=std::min(limit-tag.right-12,74*bm.bmWidth/bm.bmHeight);
                    LONG height=width*bm.bmHeight/bm.bmWidth;
                    LONG x=std::max(tag.right+12,std::min((r.right-width)/2,limit-width)),y=142-height/2;
                    Theme::bitmap(dc,banner,RECT{x,y,x+width,y+height});
                }
            }
        }
        EndPaint(hwnd,&ps); return 0;
    }
    case WM_MEASUREITEM: {
        auto item=reinterpret_cast<MEASUREITEMSTRUCT*>(lp);
        if (item->CtlID==Results || item->CtlID==RelatedList || item->CtlID==QualityChoice || item->CtlID==Mode || (item->CtlID>=ImageChoice && item->CtlID<=LanguageChoice)) {
            item->itemHeight=item->CtlID==Results ? 91 : item->CtlID==RelatedList ? 68 : 21; return TRUE;
        }
        break;
    }
    case WM_DRAWITEM: {
        auto& item=*reinterpret_cast<DRAWITEMSTRUCT*>(lp);
        if (item.CtlID==Results || item.CtlID==RelatedList) drawRow(item);
        else if (item.CtlID>=FirstCard && item.CtlID<FirstCard+4) drawFeedCard(item);
        else if (item.CtlID==ExtraMenu) {
            Theme::fill(item.hDC,item.rcItem,Theme::background);
            for(int y=7;y<=23;y+=8) Theme::rounded(item.hDC,RECT{6,y,34,y+4},Theme::text,Theme::text,4);
            if(item.itemState&ODS_FOCUS) DrawFocusRect(item.hDC,&item.rcItem);
        }
        else if (item.CtlID==Home) {
            Theme::fill(item.hDC,item.rcItem,Theme::background);
            Theme::art(item.hDC,Theme::Logo,2,6,32,23);
            SelectObject(item.hDC,subhead);
            Theme::label(item.hDC,L"AtomTube",RECT{39,0,item.rcItem.right,35},Theme::text,DT_SINGLELINE|DT_VCENTER);
            if (item.itemState&ODS_FOCUS) DrawFocusRect(item.hDC,&item.rcItem);
        } else if (item.CtlID==PlayingChannel) {
            Theme::fill(item.hDC,item.rcItem,Theme::panel); SelectObject(item.hDC,font);
            avatar(item.hDC,RECT{4,2,31,29},now.avatar,now.author);
            Theme::label(item.hDC,now.author.empty()?tr(L"Canal do vídeo"):now.author,RECT{38,0,214,31},Theme::accent,DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
            if (item.itemState&ODS_FOCUS) DrawFocusRect(item.hDC,&item.rcItem);
        }
        else if (item.CtlType==ODT_COMBOBOX) drawCombo(item);
        else if (item.CtlType==ODT_BUTTON) Theme::button(item,(item.CtlID==ForYouTab && homeTab==SearchKind::ForYou) || (item.CtlID==SubsTab && homeTab==SearchKind::Subscriptions) || (item.CtlID==SavedTab && homeTab==SearchKind::SavedVideos) || item.CtlID==Search || item.CtlID==ResultPlay || item.CtlID==Play ||
            (item.CtlID==VideoTab && (resultKind==SearchKind::Uploads || resultKind==SearchKind::Videos)) ||
            (item.CtlID==FindChannels && resultKind==SearchKind::Channels) || (item.CtlID==PlaylistTab && playlistRows(resultKind)) ||
            (item.CtlID==LiveTab && resultKind==SearchKind::Streams),item.CtlID==Search && !searching);
        else break;
        return TRUE;
    }
    case StartHome:
        if(page==Page::Home && !fullscreen && IsWindowVisible(homeForYou) && !feedLoaded && !feedBusy)
            startFeed(homeTab);
        return 0;
    case WM_COMMAND:
        if (LOWORD(wp)>=FirstCard && LOWORD(wp)<FirstCard+4) {
            unsigned index=cardOffset+LOWORD(wp)-FirstCard;
            if(page==Page::Home && index<feedRows.size()) playYoutube(feedRows[index].id,feedRows[index]);
            return 0;
        }
        switch (LOWORD(wp)) {
        case ForYouTab: case SubsTab: case SavedTab: {
            SearchKind tab=LOWORD(wp)==SavedTab ? SearchKind::SavedVideos : LOWORD(wp)==SubsTab ? SearchKind::Subscriptions : SearchKind::ForYou;
            if(homeTab!=tab) { homeTab=tab; resetFeed(); layout(); }
            break;
        }
        case FeedNext:
            if(cardOffset+cardCount<feedRows.size()) cardOffset+=cardCount;
            else if(!feedBusy && !savedNext.empty()) { startFeed(homeTab,savedNext); break; }
            showFeedCards(); layout(); break;
        case FeedPrev:
            if(cardOffset) cardOffset=cardOffset>cardCount ? cardOffset-cardCount : 0;
            else if(!feedBusy && !savedPrev.empty()) { startFeed(homeTab,savedPrev,true); break; }
            showFeedCards(); layout(); break;
        case FeedRetry:
            if(homeTab==SearchKind::SavedVideos && !feedBusy) startFeed(homeTab);
            else if(!feedBusy) { resetFeed(); layout(); }
            break;
        case AccountButton: manageAccount(); break;
        case ExtraMenu: drawer=!drawer; layout(); if(drawer) SetFocus(imageBox); break;
        case ImageChoice: case FpsChoice: case LanguageChoice:
            if (HIWORD(wp)==CBN_SELCHANGE) {
                settings.image=static_cast<ImageQuality>(SendMessageW(imageBox,CB_GETCURSEL,0,0));
                fpsIndex=static_cast<int>(SendMessageW(fpsBox,CB_GETCURSEL,0,0));
                settings.fps=frameTargets[fpsIndex];
                settings.language=static_cast<Language>(SendMessageW(langBox,CB_GETCURSEL,0,0));
                if (settings.language!=uiLanguage) {
                    resetFeed(); uiLanguage=settings.language; setFeedText(); localizeControls(); layout(); refresh(); refreshResults(); changeVolume(volume);
                }
                savePreferences();
            }
            break;
        case Url: if (HIWORD(wp)==EN_CHANGE) showRecent(false); break;
        case Open: chooseFile(); break;
        case Watch: watch(); break;
        case Home:
            search.cancel(); KillTimer(window,3); searching=false; page=Page::Home;
            SetWindowTextW(urlEdit,L""); layout(); SetFocus(window); break;
        case Search:
            if (page==Page::Catalog && searching) {
                search.cancel(); KillTimer(window,3); searching=false; resultStatus=L"Pesquisa cancelada."; refreshResults(); layout();
            } else searchQuery();
            break;
        case VideoTab:
            if (channelPage(resultKind)) searchQuery(false,SearchKind::Uploads,resultQuery.substr(0,24),channelName);
            else searchQuery(); break;
        case PlaylistTab:
            if (channelPage(resultKind)) searchQuery(false,SearchKind::ChannelPlaylists,resultQuery.substr(0,24),channelName);
            else searchQuery(false,SearchKind::Playlists); break;
        case LiveTab: searchQuery(false,SearchKind::Streams,resultQuery.substr(0,24),channelName); break;
        case Within: {
            wchar_t term[181]; GetWindowTextW(channelField,term,_countof(term));
            if (*term && channelPage(resultKind)) searchQuery(false,SearchKind::ChannelSearch,resultQuery.substr(0,24)+term,channelName);
            break;
        }
        case FindChannels: searchQuery(false,SearchKind::Channels); break;
        case PlayingChannel:
            if (!download.channelId.empty() || !now.channel.empty())
                searchQuery(false,SearchKind::Uploads,download.channelId.empty() ? now.channel : download.channelId,now.author);
            break;
        case Back:
            if (page==Page::Watch && !resultQuery.empty()) { page=Page::Catalog; layout(); SetFocus(resultList); }
            else backToResults(); break;
        case MoreResults: searchQuery(true); break;
        case Results:
            if (HIWORD(wp)==LBN_DBLCLK) selectedResult();
            else if (HIWORD(wp)==LBN_SELCHANGE) refreshResults();
            break;
        case ResultPlay: selectedResult(); break;
        case RelatedList:
            if (HIWORD(wp)==LBN_DBLCLK) {
                auto i=static_cast<size_t>(SendMessageW(relatedList,LB_GETCURSEL,0,0));
                if (i<suggestions.size()) playYoutube(suggestions[i].id,suggestions[i]);
            } break;
        case Browse:
            if (page!=Page::Watch) page=Page::Watch; else sidebar=!sidebar;
            layout(); refresh(); break;
        case Progress:
            if (HIWORD(wp)==SliderCommit) queueSeek(player.duration()*SendMessageW(progressBar,SliderGet,0,0)/1000);
            else timeline();
            break;
        case Volume: changeVolume(static_cast<int>(SendMessageW(volumeBar,SliderGet,0,0))/10); break;
        case Mute: changeVolume(volume ? 0 : restoreVolume); break;
        case Export: exportLog(); break;
        case Fullscreen: toggleFullscreen(); break;
        case Mode:
            if (HIWORD(wp)==CBN_SELCHANGE && (!current.empty() || !download.videoUrl.empty()) && !download.active()) {
                cancelSeek(); loadCurrent();
            }
            break;
        case Play:
            if (player.state==State::Ended && (!download.videoUrl.empty() || player.localStream)) { loadCurrent(); break; }
            try { player.toggle(); }
            catch (Problem p) { player.fail(p); }
            if (player.active()) SetTimer(hwnd,1,300,nullptr);
            refresh(); break;
        case ShowStats: layout(); refresh(); break;
        case EdgeSmoothing: player.setSmoothing(smoothingOn()); break;
        }
        return 0;
    case WM_KEYDOWN:
        if (wp==VK_F11 || (wp==VK_ESCAPE && fullscreen)) { toggleFullscreen(); return 0; }
        break;
    case WM_TIMER: {
        if (wp==9) { pollFeed(); return 0; }
        if (wp==7 || wp==8) {
            std::vector<SearchResult> rows; wstring status; unsigned next=0;
            if (!(wp==7 ? recommendations : preview).poll(rows,status,next)) KillTimer(hwnd,wp);
            if (!status.empty()) {
                if (wp==8) {
                    if (!rows.empty() && rows[0].id==now.id && rows[0].thumbnail) {
                        now.thumbnail=rows[0].thumbnail; InvalidateRect(scene,nullptr,FALSE);
                    }
                } else {
                    if (!now.avatar && !download.channelId.empty()) {
                        for (const auto& row:rows) if (row.channel==download.channelId && row.avatar) {
                            now.avatar=row.avatar; InvalidateRect(playingChannel,nullptr,FALSE); break;
                        }
                    }
                    rows.erase(std::remove_if(rows.begin(),rows.end(),[](const SearchResult& row){return row.id==now.id;}),rows.end());
                    if (rows.empty() && !relatedFromChannel && !download.channelId.empty()) {
                        relatedFromChannel=true;
                        try {
                            recommendations.start(SearchKind::Uploads,download.channelId,1,{},settings.catalogLanguage());
                            updateText(relatedTitle,tr(L"Mais deste canal")); SetTimer(hwnd,7,450,nullptr);
                        } catch (Problem p) { updateText(relatedTitle,tr(L"Relacionados indisponíveis")); journal.add(p.operation); }
                        return 0;
                    }
                    size_t count=suggestions.size(); suggestions=std::move(rows);
                    for(size_t i=count;i<suggestions.size();++i) SendMessageW(relatedList,LB_ADDSTRING,0,i);
                    updateText(relatedTitle,suggestions.empty() ? tr(L"Sem relacionados") : relatedFromChannel ? tr(L"Mais deste canal") : tr(L"Relacionados"));
                    InvalidateRect(relatedList,nullptr,FALSE);
                }
            }
            return 0;
        }
        if (wp==3) {
            auto before=results.size();
            wstring status;
            if (!search.poll(results,status,nextResult,&channelHeader)) KillTimer(hwnd,3);
            if (!status.empty()) {
                bool changedPage=searching;
                searching=false; resultStatus=status;
                if (before!=results.size()) populateList();
                InvalidateRect(resultList,nullptr,FALSE);
                if (page==Page::Catalog) {
                    if (changedPage) layout();
                    else if (channelPage(resultKind)) { RECT band={16,107,32767,177}; InvalidateRect(hwnd,&band,FALSE); }
                }
            }
            if (searching && page==Page::Catalog) InvalidateRect(scene,nullptr,FALSE);
            refreshResults(); return 0;
        }
        if (wp==4) {
            POINT cursor; GetCursorPos(&cursor); ScreenToClient(hwnd,&cursor);
            RECT r; GetClientRect(hwnd,&r);
            bool visible=SendMessageW(progressBar,SliderTracking,0,0) || SendMessageW(volumeBar,SliderTracking,0,0) ||
                (cursor.x>=0 && cursor.x<r.right && cursor.y>=r.bottom-78 && cursor.y<r.bottom);
            if (visible!=overlay) {
                overlay=visible;
                layout();
                if (!overlay) { SetFocus(hwnd); InvalidateRect(player.video,nullptr,FALSE); }
            }
            return 0;
        }
        if (wp==5) {
            auto target=queuedSeek; cancelSeek();
            if (target>=0 && seekable()) {
                journal.add(fmt(L"Posição solicitada | %s | %s",timeText(target).c_str(),
                    download.videoUrl.empty() ? L"MP4" : L"reabertura do streaming"));
                if (!download.videoUrl.empty()) loadCurrent(0,target,player.state==State::Paused);
                else {
                    try { player.seekLocal(target); SetTimer(window,1,300,nullptr); }
                    catch (Problem p) { player.fail(p); }
                }
                refresh();
            }
            return 0;
        }
        if (wp==2) {
            if (!download.active()) { KillTimer(hwnd,2); return 0; }
            if (download.poll()) {
                KillTimer(hwnd,2);
                if (download.videoUrl.empty()) current=download.videoPath();
                loadCurrent(download.startTime());
            } else {
                if (!download.active()) KillTimer(hwnd,2);
                player.status=download.status; refresh();
            }
            return 0;
        }
        unsigned before=player.revision;
        try { player.poll(); } catch (Problem p) { player.fail(p); }
        if (before!=player.revision || player.state==State::Opening || player.state==State::Starting) refresh();
        else if (player.state==State::Playing && (!fullscreen || overlay)) timeline();
        if (!player.active() || player.state==State::Ended) KillTimer(hwnd,1);
        return 0;
    }
    case WM_CLOSE:
        savePreferences();
        feedWorker.cancel(); search.cancel(); recommendations.cancel(); preview.cancel(); player.close(); download.clear(); DestroyWindow(hwnd); return 0;
    case WM_DESTROY:
        Theme::releaseArt(); if (grayscale) DeleteObject(grayscale);
        DeleteObject(font); DeleteObject(heading); DeleteObject(subhead); DeleteObject(wordmark); PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(hwnd,msg,wp,lp);
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,LPWSTR,int show) {
    journal.init();
    HRESULT co=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    HRESULT mf=SUCCEEDED(co) ? MFStartup(MF_VERSION) : co;
    mfReady=SUCCEEDED(mf);
    if (!mfReady) player.fail({mf,L"Iniciar componentes de mídia do Windows"});
    WNDCLASSW wc={}; wc.hInstance=instance; wc.hCursor=LoadCursor(nullptr,IDC_ARROW);
    wc.style=CS_DBLCLKS; wc.lpszClassName=L"AtomTubeVideo"; wc.hbrBackground=static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)); wc.lpfnWndProc=videoProc;
    RegisterClassW(&wc);
    wc.lpszClassName=L"AtomTubeScene"; RegisterClassW(&wc);
    wc.style=0; wc.lpszClassName=L"AtomTubeSlider"; wc.hbrBackground=nullptr; wc.lpfnWndProc=sliderProc;
    RegisterClassW(&wc);
    backgroundBrush=CreateSolidBrush(Theme::background); panelBrush=CreateSolidBrush(Theme::panel);
    wc.lpszClassName=L"AtomTube"; wc.hbrBackground=backgroundBrush;
    wc.lpfnWndProc=appProc; wc.hIcon=LoadIconW(instance,MAKEINTRESOURCEW(1)); RegisterClassW(&wc);
    RECT work={}; SystemParametersInfoW(SPI_GETWORKAREA,0,&work,0);
    HWND main=CreateWindowExW(WS_EX_CONTROLPARENT,wc.lpszClassName,L"AtomTube",WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,
        CW_USEDEFAULT,CW_USEDEFAULT,std::min(1008L,work.right-work.left),std::min(590L,work.bottom-work.top),nullptr,nullptr,instance,nullptr);
    if (main) {
        DragAcceptFiles(main,TRUE);
        ShowWindow(main,show);
        int count=0;
        auto args=CommandLineToArgvW(GetCommandLineW(),&count);
        if (args && count==2) {
            wstring id=youtubeVideoId(args[1]);
            if (id.empty()) openLocal(args[1]); else playYoutube(id);
        }
        if (args) LocalFree(args);
        MSG msg;
        while (GetMessageW(&msg,nullptr,0,0)>0) {
            if (msg.message==WM_KEYDOWN && msg.wParam==VK_ESCAPE && drawer && page==Page::Home && !fullscreen &&
                !SendMessageW(msg.hwnd,CB_GETDROPPEDSTATE,0,0) && !IsWindowVisible(recentList)) {
                drawer=false; layout(); SetFocus(menuButton); continue;
            }
            if (msg.message==WM_KEYDOWN) {
                auto key=msg.wParam;
                if ((msg.hwnd==urlEdit || msg.hwnd==recentList) && IsWindowVisible(recentList)) {
                    int i=static_cast<int>(SendMessageW(recentList,LB_GETCURSEL,0,0));
                    if (key==VK_DOWN || key==VK_UP) {
                        int matches=static_cast<int>(SendMessageW(recentList,LB_GETCOUNT,0,0));
                        SendMessageW(recentList,LB_SETCURSEL,std::max(0,std::min(matches-1,i+(key==VK_DOWN ? 1 : -1))),0); continue;
                    }
                    if (key==VK_RETURN && i>=0) { pickRecent(); continue; }
                    if (key==VK_ESCAPE) { SetFocus(urlEdit); ShowWindow(recentList,SW_HIDE); continue; }
                }
                if (key==VK_F11 || (key==VK_ESCAPE && fullscreen)) { toggleFullscreen(); continue; }
                if (!fullscreen && key=='L' && GetKeyState(VK_CONTROL)<0) {
                    SetFocus(urlEdit); SendMessageW(urlEdit,EM_SETSEL,0,-1); continue;
                }
                if (key==VK_RETURN && msg.hwnd==urlEdit) { searchQuery(); continue; }
                if (key==VK_RETURN && msg.hwnd==channelField) { SendMessageW(main,WM_COMMAND,Within,0); continue; }
                if (key==VK_RETURN && msg.hwnd==relatedList) {
                    SendMessageW(main,WM_COMMAND,MAKEWPARAM(RelatedList,LBN_DBLCLK),0); continue;
                }
                if (key==VK_RETURN && msg.hwnd==resultList) { selectedResult(); continue; }
                if (msg.hwnd!=urlEdit && msg.hwnd!=channelField && msg.hwnd!=relatedList && msg.hwnd!=resultList && msg.hwnd!=modeBox && msg.hwnd!=qualityBox && msg.hwnd!=imageBox && msg.hwnd!=fpsBox && msg.hwnd!=langBox) {
                    if (key==VK_SPACE && (msg.hwnd==main || msg.hwnd==player.video || msg.hwnd==progressBar ||
                        msg.hwnd==volumeBar || msg.hwnd==playButton)) {
                        if (!(msg.lParam&(1L<<30))) SendMessageW(main,WM_COMMAND,Play,0);
                        continue;
                    }
                    if (key==VK_LEFT || key==VK_RIGHT) {
                        queueSeek((queuedSeek>=0 ? queuedSeek : player.position())+(key==VK_LEFT ? -10000 : 10000)); continue;
                    }
                    if (key==VK_UP || key==VK_DOWN) { changeVolume(volume+(key==VK_UP ? 5 : -5)); continue; }
                }
            }
            if (!IsDialogMessageW(main,&msg)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
        }
    }
    DeleteObject(backgroundBrush); DeleteObject(panelBrush);
    if (mfReady) MFShutdown();
    if (SUCCEEDED(co)) CoUninitialize();
    return main ? 0 : 1;
}
