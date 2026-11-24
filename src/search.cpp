#include "search.h"
#include "youtube.h"
#include <algorithm>
#include <winhttp.h>
#include <wincodec.h>
#include <process.h>
#include <mutex>
#include <cstdlib>

static wstring argument(const wstring& value) {
    wstring result=L"\"";
    size_t slashes=0;
    for (wchar_t c:value) {
        if (c==L'\\') { ++slashes; continue; }
        result.append(c==L'"' ? slashes*2+1 : slashes,L'\\');
        slashes=0;
        result+=c;
    }
    result.append(slashes*2,L'\\');
    return result+L"\"";
}
#include "metadata.h"
static std::shared_ptr<void> thumbnail(HINTERNET session,IWICImagingFactory* factory,wstring url,int w,int h,bool crop=false) {
    if (url.compare(0,2,L"//")==0) url=L"https:"+url;
    if (url.compare(0,8,L"https://")!=0 || url.size()>4096) return {};
    size_t slash=url.find(L'/',8); if (slash==wstring::npos) return {};
    auto hostname=url.substr(8,slash-8);
    if (hostname!=L"i.ytimg.com" && hostname!=L"yt3.ggpht.com" && hostname!=L"yt3.googleusercontent.com") return {};
    auto path=url.substr(slash);
    if (hostname!=L"i.ytimg.com" && path.find(L'=')!=wstring::npos)
        path=path.substr(0,path.rfind(L'='))+fmt(L"=w%d-h%d-p",w,h);
    HINTERNET host=WinHttpConnect(session,hostname.c_str(),INTERNET_DEFAULT_HTTPS_PORT,0);
    if (!host) return {};
    HINTERNET request=WinHttpOpenRequest(host,L"GET",path.c_str(),nullptr,WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,WINHTTP_FLAG_SECURE);
    if (!request) { WinHttpCloseHandle(host); return {}; }
    DWORD disable=WINHTTP_DISABLE_REDIRECTS, code=0, size=sizeof(code);
    WinHttpSetOption(request,WINHTTP_OPTION_DISABLE_FEATURE,&disable,sizeof(disable));
    std::vector<BYTE> data;
    if (WinHttpSendRequest(request,WINHTTP_NO_ADDITIONAL_HEADERS,0,WINHTTP_NO_REQUEST_DATA,0,0,0) &&
        WinHttpReceiveResponse(request,nullptr) &&
        WinHttpQueryHeaders(request,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,nullptr,&code,&size,nullptr) &&
        code==200) {
        BYTE buffer[4096]; DWORD got=0;
        do {
            if (!WinHttpReadData(request,buffer,sizeof(buffer),&got) || data.size()+got>192*1024) {
                data.clear(); break;
            }
            data.insert(data.end(),buffer,buffer+got);
        } while (got);
    }
    WinHttpCloseHandle(request);
    WinHttpCloseHandle(host);
    if (data.empty()) return {};
    ComPtr<IWICStream> stream;
    ComPtr<IWICBitmapDecoder> decoder;
    ComPtr<IWICBitmapFrameDecode> frame;
    ComPtr<IWICBitmapScaler> scaler;
    ComPtr<IWICFormatConverter> converter;
    UINT width=0,height=0;
    if (FAILED(factory->CreateStream(&stream)) ||
        FAILED(stream->InitializeFromMemory(data.data(),static_cast<DWORD>(data.size()))) ||
        FAILED(factory->CreateDecoderFromStream(stream.Get(),nullptr,WICDecodeMetadataCacheOnDemand,&decoder)) ||
        FAILED(decoder->GetFrame(0,&frame)) || FAILED(frame->GetSize(&width,&height)) ||
        !width || !height || width>2048 || height>1536) return {};
    IWICBitmapSource* source=frame.Get();
    ComPtr<IWICBitmapClipper> clip;
    if (crop && width*h!=height*w) {
        int cw=std::min<int>(width,height*w/h),ch=std::min<int>(height,width*h/w);
        WICRect rect={(static_cast<int>(width)-cw)/2,(static_cast<int>(height)-ch)/2,cw,ch};
        if (FAILED(factory->CreateBitmapClipper(&clip)) || FAILED(clip->Initialize(source,&rect))) return {};
        source=clip.Get();
    }
    if (FAILED(factory->CreateBitmapScaler(&scaler)) ||
        FAILED(scaler->Initialize(source,w,h,WICBitmapInterpolationModeLinear)) ||
        FAILED(factory->CreateFormatConverter(&converter)) ||
        FAILED(converter->Initialize(scaler.Get(),GUID_WICPixelFormat32bppBGR,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom))) return {};
    BITMAPINFO dib={}; dib.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
    dib.bmiHeader.biWidth=w; dib.bmiHeader.biHeight=-h; dib.bmiHeader.biPlanes=1; dib.bmiHeader.biBitCount=32;
    void* pixels=nullptr;
    HBITMAP bitmap=CreateDIBSection(nullptr,&dib,DIB_RGB_COLORS,&pixels,nullptr,0);
    if (bitmap && FAILED(converter->CopyPixels(nullptr,w*4,w*h*4,static_cast<BYTE*>(pixels)))) {
        DeleteObject(bitmap); bitmap=nullptr;
    }
    return std::shared_ptr<void>(bitmap,[](void* handle) { if (handle) DeleteObject(handle); });
}
static wstring encoded(const wstring& value) {
    int len=WideCharToMultiByte(CP_UTF8,0,value.c_str(),-1,nullptr,0,nullptr,nullptr);
    if (!len) return {};
    std::string bytes(len,'\0');
    WideCharToMultiByte(CP_UTF8,0,value.c_str(),-1,&bytes[0],len,nullptr,nullptr); bytes.pop_back();
    const wchar_t hex[]=L"0123456789ABCDEF"; wstring url;
    for (unsigned char b:bytes) { url+=L'%'; url+=hex[b>>4]; url+=hex[b&15]; }
    return url;
}
static bool playlistId(const wstring& id) {
    return id.size()>=12 && id.size()<=80 && id.find_first_not_of(
        L"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-")==wstring::npos;
}
static std::vector<wstring> fields(const wstring& line) {
    std::vector<wstring> parts;
    for (size_t pos=0;;) {
        size_t end=line.find(L'\t',pos);
        wstring field=line.substr(pos,end-pos);
        if (!field.empty() && field.front()==L'"') field=titleFromJson(field);
        if (field==L"NA" || field==L"null") field.clear();
        parts.push_back(field);
        if (end==wstring::npos) break;
        pos=end+1;
    }
    return parts;
}
struct YoutubeSearch::Task {
    wstring language;
    ChannelHeader header;
    SearchKind kind=SearchKind::Videos;
    HANDLE stop=CreateEventW(nullptr,TRUE,FALSE,nullptr);
    std::mutex mutex;
    std::vector<SearchResult> items;
    wstring query,status;
    unsigned first=1,next=0,returned=0;
    bool extracted=false;
    bool dirty=false,done=false;
    ~Task() { if (stop) CloseHandle(stop); }
    bool cancelled() const { return WaitForSingleObject(stop,0)==WAIT_OBJECT_0; }
    std::vector<SearchResult> extract() {
        wchar_t module[MAX_PATH];
        DWORD count=GetModuleFileNameW(nullptr,module,MAX_PATH);
        if (!count || count>=MAX_PATH) return {};
        wstring exe=wstring(module,count);
        exe=exe.substr(0,exe.find_last_of(L'\\'))+L"\\youtube\\yt-dlp.exe";
        const unsigned last=first+14;
        wstring target;
        if (kind==SearchKind::Preview) {
            if (youtubeVideoId(L"https://youtu.be/"+query)!=query) return {};
            SearchResult row; row.id=query; extracted=true; return {row};
        }
        bool feed=kind==SearchKind::SavedVideos || kind==SearchKind::ForYou || kind==SearchKind::Subscriptions || kind==SearchKind::Connect;
        if (feed) {
            const wchar_t* action=kind==SearchKind::SavedVideos ? L"saved" : kind==SearchKind::ForYou ? L"foryou" : kind==SearchKind::Connect ? L"connect" : L"subscriptions";
            target=wstring(L"atomtubehome:")+action+L":"+language;
            if (kind==SearchKind::SavedVideos && !query.empty()) target+=L":"+query;
        } else if (kind==SearchKind::Channels || kind==SearchKind::Playlists) {
            target=wstring(L"https://www.youtube.com/results?sp=")+(kind==SearchKind::Channels ? L"EgIQAg" : L"EgIQAw")+
                L"%3D%3D&search_query="+encoded(query);
        } else if (channelPage(kind)) {
            wstring id=query.substr(0,24);
            if (!youtubeChannelId(id)) return {};
            target=L"https://www.youtube.com/channel/"+id;
            if (kind==SearchKind::ChannelSearch) target+=L"/search?query="+encoded(query.substr(24));
            else target+=kind==SearchKind::Uploads ? L"/videos" : kind==SearchKind::Streams ? L"/streams" : L"/playlists";
        } else if (kind==SearchKind::Entries) {
            if (!playlistId(query)) return {};
            target=L"https://www.youtube.com/playlist?list="+query;
        } else target=L"ytsearch"+std::to_wstring(last)+L":"+query;
        wstring plugins=exe.substr(0,exe.find_last_of(L'\\'))+L"\\yt-dlp-plugins";
        wstring cmd=argument(exe)+L" --playlist-items "+std::to_wstring(first)+L":"+std::to_wstring(last)+
            L" --ignore-config --no-plugin-dirs --plugin-dirs "+argument(plugins)+
            L" --flat-playlist --extractor-args youtube:lang="+language+L" --extractor-args youtubetab:approximate_date --socket-timeout 8"
            L" --retries 0 --extractor-retries 0 --encoding utf-8"
            L" --print \"%(id)s\t%(duration_string)s\t%(title)j\t%(channel,uploader)j\t%(channel_id)s\t%(at_date,upload_date)s\t%(at_views,view_count)s\t%(at_subs)s\t%(description)j\t%(at_count)s\t%(thumbnails.0.url)s\t%(at_avatar)s\t%(channel_is_verified)s\t%(live_status)s\" "+
            (feed ? L"--print \"playlist:FEED\t%(title)s\" " : L"")+(channelPage(kind) ? L"--print \"playlist:HEADER\t%(channel,title)j\t%(thumbnails.-1.id)s\t%(thumbnails.-1.url)s\t%(thumbnails.-3.id)s\t%(thumbnails.-3.url)s\" " : L"")+L"-- "+
            argument(target);
        SECURITY_ATTRIBUTES sa={sizeof(sa),nullptr,TRUE};
        HANDLE reader=nullptr,writer=nullptr;
        HANDLE job=CreateJobObjectW(nullptr,nullptr);
        HANDLE nul=CreateFileW(L"NUL",GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,&sa,OPEN_EXISTING,0,nullptr);
        STARTUPINFOW info={}; info.cb=sizeof(info); info.dwFlags=STARTF_USESTDHANDLES;
        PROCESS_INFORMATION pi={};
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits={};
        limits.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        std::string output;
        if (job && nul!=INVALID_HANDLE_VALUE && CreatePipe(&reader,&writer,&sa,0)) {
            info.hStdInput=nul; info.hStdError=nul; info.hStdOutput=writer;
            if (SetHandleInformation(reader,HANDLE_FLAG_INHERIT,0) &&
                SetInformationJobObject(job,JobObjectExtendedLimitInformation,&limits,sizeof(limits)) &&
                CreateProcessW(exe.c_str(),&cmd[0],nullptr,nullptr,TRUE,CREATE_NO_WINDOW|CREATE_SUSPENDED|BELOW_NORMAL_PRIORITY_CLASS,
                    nullptr,nullptr,&info,&pi)) {
                if (AssignProcessToJobObject(job,pi.hProcess) && ResumeThread(pi.hThread)!=DWORD(-1)) {
                    ULONGLONG deadline=GetTickCount64()+(kind==SearchKind::Connect ? 190000 : feed ? 85000 : 45000);
                    while (!cancelled() && GetTickCount64()<deadline && output.size()<512*1024) {
                        DWORD bytes=0;
                        if (!PeekNamedPipe(reader,nullptr,0,nullptr,&bytes,nullptr)) break;
                        if (bytes) {
                            char part[4096]; DWORD got=0;
                            if (!ReadFile(reader,part,bytes<sizeof(part) ? bytes : sizeof(part),&got,nullptr)) break;
                            output.append(part,got);
                        } else if (WaitForSingleObject(pi.hProcess,0)==WAIT_OBJECT_0) {
                            DWORD code=1;
                            extracted=GetExitCodeProcess(pi.hProcess,&code) && code==0;
                            // The process may have written its last line after PeekNamedPipe.
                            if (!PeekNamedPipe(reader,nullptr,0,nullptr,&bytes,nullptr) || !bytes) break;
                        }
                        else WaitForSingleObject(stop,40);
                    }
                } else TerminateProcess(pi.hProcess,1);
                CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
            }
        }
        if (job) CloseHandle(job);
        if (reader) CloseHandle(reader);
        if (writer) CloseHandle(writer);
        if (nul!=INVALID_HANDLE_VALUE) CloseHandle(nul);
        std::vector<SearchResult> results;
        for (size_t start=0,end; (end=output.find('\n',start))!=std::string::npos; start=end+1) {
            wstring line=utf8(output.substr(start,end-start));
            if (!line.empty() && line.back()==L'\r') line.pop_back();
            auto f=fields(line);
            if (feed && f.size()==2 && f[0]==L"FEED") { header.name=f[1]; continue; }
            if (f.size()==6 && f[0]==L"HEADER") {
                header.name=f[1]; if(f[2]==L"avatar_uncropped") header.avatarUrl=f[3];
                if(f[4]==L"banner_uncropped") header.bannerUrl=f[5];
                continue;
            }
            if (f.size()!=14) continue;
            ++returned;
            if (results.size()>=15 || f[0].empty() || f[2].empty()) continue;
            if (kind==SearchKind::Channels ? !youtubeChannelId(f[0]) : playlistRows(kind) ? !playlistId(f[0]) :
                youtubeVideoId(L"https://youtu.be/"+f[0])!=f[0]) continue;
            SearchResult row; row.id=f[0]; row.duration=f[1]; row.title=f[2]; row.author=f[3];
            if (youtubeChannelId(f[4])) row.channel=f[4];
            row.date=f[5]; row.views=f[6]; row.subscribers=f[7]; row.description=f[8].substr(0,220);
            row.count=f[9]; row.imageUrl=f[10]; row.avatarUrl=f[11]; row.verified=f[12]==L"True"; row.live=f[13];
            results.push_back(std::move(row));
        }
        return results;
    }
    void run() {
        auto page=extract();
        std::vector<SearchResult> entries;
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (!extracted) {
                next=first;
                status=L"Falha na busca. Tente novamente.";
                dirty=true; return;
            }
            next=returned>=15 ? first+15 : 0;
            for (auto& row:page) {
                if (kind==SearchKind::Uploads || kind==SearchKind::Streams || kind==SearchKind::ChannelSearch) {
                    if (row.author.empty()) row.author=header.name;
                    if (row.channel.empty()) row.channel=query.substr(0,24);
                }
                if (kind==SearchKind::SavedVideos || std::none_of(items.begin(),items.end(),[&](const SearchResult& item){return item.id==row.id;}))
                    items.push_back(std::move(row));
            }
            entries=items;
            status=items.empty() ? L"Nenhum resultado encontrado." :
                fmt(L"%u resultados no YouTube",static_cast<unsigned>(items.size()));
            dirty=true;
        }
        if (cancelled() || entries.empty() && (header.name.empty() || kind==SearchKind::SavedVideos || kind==SearchKind::Connect || kind==SearchKind::Subscriptions || kind==SearchKind::ForYou)) return;
        HRESULT co=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
        if (FAILED(co)) return;
        ComPtr<IWICImagingFactory> factory;
        if (SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory)))) {
            HINTERNET http=WinHttpOpen(L"AtomTube/0.7",WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0);
            if (http) {
                DWORD tls=WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2;
                WinHttpSetOption(http,WINHTTP_OPTION_SECURE_PROTOCOLS,&tls,sizeof(tls));
                WinHttpSetTimeouts(http,2500,2500,2500,2500);
                {
                    auto banner=thumbnail(http,factory.Get(),header.bannerUrl,800,92,true);
                    auto avatar=thumbnail(http,factory.Get(),header.avatarUrl,80,80);
                    if (banner || avatar) {
                        std::lock_guard<std::mutex> lock(mutex);
                        header.banner=banner; header.avatar=avatar; dirty=true;
                    }
                    unsigned failures=0;
                    for (size_t i=0;i<entries.size() && !cancelled() && failures<3;++i) {
                        if (entries[i].thumbnail) continue;
                        bool channel=kind==SearchKind::Channels;
                        auto url=channel || playlistRows(kind) ? entries[i].imageUrl :
                            L"https://i.ytimg.com/vi/"+entries[i].id+L"/mqdefault.jpg";
                        auto bitmap=thumbnail(http,factory.Get(),url,channel ? 64 : 160,channel ? 64 : 90);
                        if (!bitmap) { ++failures; continue; }
                        failures=0;
                        auto icon=avatar && entries[i].channel==query.substr(0,24) ? avatar :
                            thumbnail(http,factory.Get(),entries[i].avatarUrl,32,32);
                        std::lock_guard<std::mutex> lock(mutex);
                        items[i].thumbnail=std::move(bitmap); items[i].avatar=icon; dirty=true;
                    }
                }
                WinHttpCloseHandle(http);
            }
        }
        factory.Reset();
        CoUninitialize();
    }
    static unsigned __stdcall worker(void* parameter) {
        std::unique_ptr<std::shared_ptr<Task>> owner(static_cast<std::shared_ptr<Task>*>(parameter));
        Task& self=**owner;
        SetThreadPriority(GetCurrentThread(),THREAD_PRIORITY_BELOW_NORMAL);
        try { self.run(); }
        catch (...) {
            std::lock_guard<std::mutex> lock(self.mutex);
            self.status=L"Falha ao pesquisar no YouTube."; self.dirty=true;
        }
        std::lock_guard<std::mutex> lock(self.mutex);
        self.done=true;
        return 0;
    }
};
void YoutubeSearch::start(SearchKind kind,const wstring& query,unsigned first,const std::vector<SearchResult>& previous,const wchar_t* language) {
    cancel();
    task=std::make_shared<Task>();
    task->query=query;
    task->kind=kind;
    task->language=language;
    task->first=first; task->next=first; task->items=previous;
    if (!task->stop) { task.reset(); throw Problem{E_FAIL,L"Iniciar pesquisa"}; }
    auto parameter=new std::shared_ptr<Task>(task);
    auto thread=_beginthreadex(nullptr,0,Task::worker,parameter,0,nullptr);
    if (!thread) { delete parameter; task.reset(); throw Problem{E_FAIL,L"Iniciar pesquisa"}; }
    CloseHandle(reinterpret_cast<HANDLE>(thread));
}
void YoutubeSearch::cancel() {
    if (task) { SetEvent(task->stop); task.reset(); }
}
bool YoutubeSearch::poll(std::vector<SearchResult>& results,wstring& status,unsigned& next,ChannelHeader* header) {
    if (!task) return false;
    auto current=task;
    std::lock_guard<std::mutex> lock(current->mutex);
    if (current->dirty) { results=current->items; status=current->status; next=current->next;
        if (header) *header=current->header;
        current->dirty=false; }
    if (current->done) task.reset();
    return !current->done;
}
