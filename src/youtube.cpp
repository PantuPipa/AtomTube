#include "youtube.h"
#include "metadata.h"
#include <algorithm>
#include <cwctype>
#include <vector>

static bool validId(const wstring& id) {
    return id.size()==11 && id.find_first_not_of(L"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-")==wstring::npos;
}
bool youtubeChannelId(const wstring& id) {
    return id.size()==24 && id.compare(0,2,L"UC")==0 &&
        id.find_first_not_of(L"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-")==wstring::npos;
}
wstring youtubeVideoId(wstring link) {
    size_t first=link.find_first_not_of(L" \r\n\t");
    if (first==wstring::npos) return {};
    link=link.substr(first,link.find_last_not_of(L" \r\n\t")-first+1);
    size_t slash=link.find(L"://");
    if (slash!=wstring::npos) {
        wstring scheme=link.substr(0,slash);
        if (_wcsicmp(scheme.c_str(),L"https") && _wcsicmp(scheme.c_str(),L"http")) return {};
        link.erase(0,slash+3);
    }
    slash=link.find(L'/');
    if (slash==wstring::npos) return {};
    wstring host=link.substr(0,slash);
    std::transform(host.begin(),host.end(),host.begin(),towlower);
    wstring path=link.substr(slash+1);
    path=path.substr(0,path.find(L'#'));
    wstring id;
    if (host==L"youtu.be") id=path.substr(0,path.find(L'?'));
    else if (host==L"youtube.com" || host==L"www.youtube.com" || host==L"m.youtube.com" || host==L"music.youtube.com") {
        if (path.compare(0,6,L"watch?")==0) {
            for (size_t i=6;i<path.size();) {
                size_t end=path.find(L'&',i);
                wstring pair=path.substr(i,end-i);
                if (pair.compare(0,2,L"v=")==0) { id=pair.substr(2); break; }
                if (end==wstring::npos) break;
                i=end+1;
            }
        } else if (path.compare(0,7,L"shorts/")==0 || path.compare(0,6,L"embed/")==0 || path.compare(0,5,L"live/")==0) {
            wstring segment=path.substr(path.find(L'/')+1);
            id=segment.substr(0,segment.find(L'?'));
        }
    }
    return validId(id) ? id : L"";
}
static void winCheck(BOOL ok,const wchar_t* what) {
    if (!ok) throw Problem{HRESULT_FROM_WIN32(GetLastError()),what};
}
static void closeHandle(HANDLE& h) {
    if (h && h!=INVALID_HANDLE_VALUE) CloseHandle(h);
    h=nullptr;
}
static bool regularFile(const wstring& path) {
    DWORD a=GetFileAttributesW(path.c_str());
    return a!=INVALID_FILE_ATTRIBUTES && !(a&FILE_ATTRIBUTE_DIRECTORY);
}
static void removeMedia(const wstring& dir) {
    WIN32_FIND_DATAW f;
    HANDLE search=FindFirstFileW((dir+L"\\video.*").c_str(),&f);
    if (search==INVALID_HANDLE_VALUE) return;
    do {
        if (!(f.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY))
            DeleteFileW((dir+L"\\"+f.cFileName).c_str());
    } while (FindNextFileW(search,&f));
    FindClose(search);
}
static void cleanOldSessions(const wstring& root) {
    WIN32_FIND_DATAW f;
    HANDLE search=FindFirstFileW((root+L"\\{*}").c_str(),&f);
    if (search==INVALID_HANDLE_VALUE) return;
    do {
        GUID id;
        if (!(f.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY) || (f.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT)
            || wcslen(f.cFileName)!=38 || FAILED(CLSIDFromString(f.cFileName,&id))) continue;
        wstring dir=root+L"\\"+f.cFileName, marker=dir+L"\\session.lock";
        HANDLE stale=CreateFileW(marker.c_str(),DELETE,0,nullptr,OPEN_EXISTING,FILE_FLAG_DELETE_ON_CLOSE,nullptr);
        if (stale==INVALID_HANDLE_VALUE) continue; // another instance owns it
        removeMedia(dir); CloseHandle(stale); RemoveDirectoryW(dir.c_str());
    } while (FindNextFileW(search,&f));
    FindClose(search);
}
static void cleanChallengeCache(const wstring& root) {
    struct Entry { wstring path; FILETIME written; ULONGLONG bytes; };
    std::vector<Entry> files;
    ULONGLONG total=0;
    WIN32_FIND_DATAW f;
    wstring folder=root+L"\\extrator\\challenge-solver";
    HANDLE search=FindFirstFileW((folder+L"\\player*.json").c_str(),&f);
    if (search==INVALID_HANDLE_VALUE) return;
    do {
        if (f.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT)) continue;
        ULONGLONG size=(static_cast<ULONGLONG>(f.nFileSizeHigh)<<32)|f.nFileSizeLow;
        files.push_back({folder+L"\\"+f.cFileName,f.ftLastWriteTime,size}); total+=size;
    } while (FindNextFileW(search,&f));
    FindClose(search);
    std::sort(files.begin(),files.end(),[](const Entry& a,const Entry& b) {
        return CompareFileTime(&a.written,&b.written)<0;
    });
    size_t first=0;
    while (first<files.size() && (files.size()-first>2 || total>12ull*1024*1024)) {
        if (DeleteFileW(files[first].path.c_str())) total-=files[first].bytes;
        ++first;
    }
}
void YoutubeDownload::start(const wstring& id,unsigned height,bool stream,PlaybackOptions settings) {
    options=settings;
    begin(id,height,stream);
}
void YoutubeDownload::begin(const wstring& id,unsigned height,bool stream) {
    if (!validId(id) || (height!=720 && height!=600 && height!=480 && height!=360))
        throw Problem{E_INVALIDARG,L"Link ou qualidade inválidos"};
    clear();
    progressive=stream;
    requestedId=id; requestedHeight=height;
    started=GetTickCount64();
    wchar_t module[MAX_PATH],temp[MAX_PATH];
    DWORD n=GetModuleFileNameW(nullptr,module,MAX_PATH);
    if (!n || n>=MAX_PATH) throw Problem{E_FAIL,L"Localizar pasta do AtomTube"};
    wstring dir(module);
    dir=dir.substr(0,dir.find_last_of(L'\\'))+L"\\youtube";
    wstring exe=dir+L"\\yt-dlp.exe", qjs=dir+L"\\qjs.exe", ff=dir+L"\\ffmpeg";
    wstring plugin=dir+L"\\yt-dlp-plugins\\atomtube\\yt_dlp_plugins\\extractor\\atomtube_cache.py";
    if (!regularFile(exe) || !regularFile(qjs) || !regularFile(ff+L"\\ffmpeg.exe") ||
        !regularFile(ff+L"\\ffprobe.exe") || !regularFile(plugin))
        throw Problem{HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND),L"Extraia a pasta youtube do kit ao lado de AtomTube.exe"};
    n=GetTempPathW(MAX_PATH,temp);
    if (!n || n>=MAX_PATH-80) throw Problem{E_FAIL,L"Localizar pasta temporária"};
    wstring root=wstring(temp)+L"AtomTube-video";
    if (!CreateDirectoryW(root.c_str(),nullptr) && GetLastError()!=ERROR_ALREADY_EXISTS)
        winCheck(FALSE,L"Criar cache de vídeo");
    DWORD attributes=GetFileAttributesW(root.c_str());
    if (attributes==INVALID_FILE_ATTRIBUTES || !(attributes&FILE_ATTRIBUTE_DIRECTORY) || (attributes&FILE_ATTRIBUTE_REPARSE_POINT))
        throw Problem{E_FAIL,L"Abrir pasta temporária de vídeo"};
    cleanOldSessions(root);
    cleanChallengeCache(root);
    GUID guid; wchar_t guidText[40];
    check(CoCreateGuid(&guid),L"Identificar download");
    StringFromGUID2(guid,guidText,40);
    wstring fresh=root+L"\\"+guidText;
    winCheck(CreateDirectoryW(fresh.c_str(),nullptr),L"Criar pasta desta reprodução");
    directory=fresh;
    try {
        lock=CreateFileW((directory+L"\\session.lock").c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
        winCheck(lock!=INVALID_HANDLE_VALUE,L"Reservar pasta temporária");
        if (stream) {
            launch(quickQuery,exe,qjs,ff,root,true,false);
            launch(fullQuery,exe,qjs,ff,root,false,true);
        } else launch(fullQuery,exe,qjs,ff,root,false,false);
    } catch (...) {
        clear(); throw;
    }
    status=L"Obtendo as faixas do YouTube…";
    bool dllDirectories=GetProcAddress(GetModuleHandleW(L"kernel32.dll"),"AddDllDirectory")!=nullptr;
    journal.add(fmt(L"Preferências | imagem %d | FPS alvo %u (0 = fonte) | áudio %s",static_cast<int>(options.image),options.fps,options.audioLanguage()));
    const wchar_t* strategy=stream ? L"rápida + completa em paralelo" : L"única";
    const wchar_t* dll=dllDirectories ? L"presente" : L"ausente";
    journal.add(height==600 ?
        fmt(L"YouTube | início | limite de tela 1024 × 600 | H.264 + AAC | consulta %s | AddDllDirectory no Windows: %s",
            strategy,dll) :
        fmt(L"YouTube | início | limite %up | H.264 + AAC | consulta %s | AddDllDirectory no Windows: %s",
            height,strategy,dll));
}
void YoutubeDownload::launch(Query& query,const wstring& exe,const wstring& qjs,const wstring& ff,
                             const wstring& root,bool fast,bool background) {
    HANDLE writer=nullptr,input=INVALID_HANDLE_VALUE;
    PROCESS_INFORMATION pi={};
    try {
        SECURITY_ATTRIBUTES sa={sizeof(sa),nullptr,TRUE};
        winCheck(CreatePipe(&query.pipe,&writer,&sa,0),L"Abrir canal de progresso");
        winCheck(SetHandleInformation(query.pipe,HANDLE_FLAG_INHERIT,0),L"Configurar canal de progresso");
        input=CreateFileW(L"NUL",GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,&sa,OPEN_EXISTING,0,nullptr);
        winCheck(input!=INVALID_HANDLE_VALUE,L"Configurar entrada do extrator");
        query.job=CreateJobObjectW(nullptr,nullptr);
        winCheck(query.job!=nullptr,L"Criar grupo do download");
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION limit={};
        limit.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE|
            (background ? JOB_OBJECT_LIMIT_PRIORITY_CLASS : 0);
        limit.BasicLimitInformation.PriorityClass=background ? BELOW_NORMAL_PRIORITY_CLASS : 0;
        winCheck(SetInformationJobObject(query.job,JobObjectExtendedLimitInformation,&limit,sizeof(limit)),L"Configurar cancelamento");
        unsigned sourceHeight=requestedHeight==600 ? 720 : requestedHeight;
        wstring video=L"[ext=mp4][vcodec^=avc1][width<=1280][height<="+
            std::to_wstring(sourceHeight)+L"]"+(options.fps && options.fps<=30 ? L"[fps<=30]" : L"")+L"[protocol=https]";
        wstring audio=L"ba[ext=m4a][acodec^=mp4a][protocol=https]";
        wstring pair=L"bv"+video+L"+"+audio;
        wstring mux=L"b"+video+L"[acodec^=mp4a]";
        wstring exact=wstring(L"[language=")+options.audioLanguage()+L"]";
        wstring regional=options.language==Language::Portuguese ? L"[language=pt]" :
            wstring(L"[language^=")+options.audioLanguage()+L"]";
        wstring format=pair+exact+L"/"+pair+regional+L"/"+mux+exact+L"/"+mux+regional+L"/"+pair+L"/"+mux;
        // Input becomes a canonical 11-character ID. Paths come from Win32 and cannot contain quotes.
        wstring cmd=L"\""+exe+L"\" --ignore-config "+(fast ? L"--no-plugin-dirs " : L"")+
            L"--no-playlist --no-js-runtimes --js-runtimes \"quickjs:"+qjs+L"\" --ffmpeg-location \""+ff+L"\" "
            L"--cache-dir \""+root+L"\\extrator\" --extractor-args \"youtube:skip=hls;lang="+wstring(options.catalogLanguage())+L";player_skip="+
            (fast ? L"js,initial_data" : L"initial_data")+L"\" "
            L"--socket-timeout 20 --retries 2 --fragment-retries 2 --match-filters \"!is_live & live_status!=is_upcoming\" ";
        cmd+=L"--encoding utf-8 --check-formats --print \"ATC %(channel_id)s\" "
             L"--print \"ATT %(title)j\" --print \"ATA %(channel)j\" --print \"ATL %(requested_formats.1.language,language)s\" ";
        cmd+=progressive ? L"--get-url --print \"ATS %(width)s %(height)s %(fps)s %(duration)s\" " :
            L"--no-simulate --no-mtime --color never --newline --progress --progress-delta 1 "
            L"--progress-template \"download:ATPC %(progress._percent_str)s\" --merge-output-format mp4 ";
        // Resolution and cadence precede bitrate in every profile.
        if (options.image==ImageQuality::High) cmd+=L"-S res,fps,vbr ";
        if (options.image==ImageQuality::Low) cmd+=L"-S res,fps,vbr:650 ";
        if (options.image==ImageQuality::Potato) cmd+=L"-S res,fps,+vbr ";
        cmd+=L"-f \""+format+L"\" -o video.%(ext)s -- https://www.youtube.com/watch?v="+requestedId;
        STARTUPINFOW si={}; si.cb=sizeof(si); si.dwFlags=STARTF_USESTDHANDLES;
        si.hStdInput=input; si.hStdOutput=writer; si.hStdError=writer;
        DWORD flags=CREATE_NO_WINDOW|CREATE_SUSPENDED;
        winCheck(CreateProcessW(exe.c_str(),&cmd[0],nullptr,nullptr,TRUE,flags,
            nullptr,directory.c_str(),&si,&pi),L"Iniciar extrator x86");
        winCheck(AssignProcessToJobObject(query.job,pi.hProcess),L"Associar extrator ao cancelamento");
        winCheck(ResumeThread(pi.hThread)!=DWORD(-1),L"Executar extrator");
        query.process=pi.hProcess; pi.hProcess=nullptr;
        closeHandle(pi.hThread); closeHandle(writer); closeHandle(input);
    } catch (...) {
        if (pi.hProcess) { TerminateProcess(pi.hProcess,1); WaitForSingleObject(pi.hProcess,2000); }
        closeHandle(pi.hThread); closeHandle(pi.hProcess); closeHandle(input); closeHandle(writer);
        release(query,true); throw;
    }
}
void YoutubeDownload::line(Query& query,const std::string& s) {
    if (s.compare(0,4,"ATL ")==0) { query.language=utf8(s.substr(4,32)); return; }
    if (s.compare(0,4,"ATT ")==0 || s.compare(0,4,"ATA ")==0) {
        (s[2]=='T' ? query.title : query.author)=titleFromJson(utf8(s.substr(4))); return;
    }
    if (s.compare(0,4,"ATC ")==0) {
        wstring channel(s.begin()+4,s.end());
        if (youtubeChannelId(channel)) query.channelId=channel;
        return;
    }
    if (s.compare(0,4,"ATS ")==0) {
        unsigned width=0,height=0;
        double fps=0,duration=0;
        if (sscanf_s(s.c_str()+4,"%u %u %lf %lf",&width,&height,&fps,&duration)==4 &&
            width && width<=1920 && height && height<=1080 && fps>0 && fps<=240) {
            query.videoWidth=width; query.videoHeight=height; query.videoFps=fps;
            query.durationSeconds=static_cast<unsigned>(duration+.5);
        }
        return;
    }
    if (progressive && s.compare(0,8,"https://")==0) {
        size_t end=s.find('/',8);
        std::string host=s.substr(8,end==s.npos ? s.npos : end-8);
        const std::string suffix=".googlevideo.com";
        if (end!=s.npos && host.size()>suffix.size() && host.compare(host.size()-suffix.size(),suffix.size(),suffix)==0 &&
            host.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789.-")==s.npos && s.size()<65536) {
            if (query.videoUrl.empty()) query.videoUrl=s;
            else if (query.audioUrl.empty()) query.audioUrl=s;
        }
        return;
    }
    if (s.compare(0,5,"ATPC ")==0) {
        const char* percent=s.c_str()+5;
        char* end=nullptr;
        double v=strtod(percent,&end);
        if (end!=percent && *end=='%' && v>=0 && v<=100) status=fmt(L"Baixando faixa do YouTube · %.0f%%",v);
    } else if (s.compare(0,8,"[Merger]")==0) status=L"Juntando imagem e áudio…";
    if (s.find("Requested format is not available")!=s.npos || s.find("No video formats found")!=s.npos) {
        query.failure=L"Sem H.264 e AAC compatíveis na qualidade escolhida."; query.missingFormat=true;
    }
    else if (s.find("does not pass filter")!=s.npos) query.failure=L"Transmissão ao vivo ou agendada. Escolha um vídeo gravado.";
    else if (s.find("Sign in")!=s.npos || s.find("Private video")!=s.npos) query.failure=L"O YouTube restringiu o acesso a este vídeo.";
    else if (s.find("HTTP Error 403")!=s.npos || s.find("HTTP Error 429")!=s.npos) query.failure=L"O YouTube recusou a transferência.";
    else if (s.find("CERTIFICATE_VERIFY_FAILED")!=s.npos) query.failure=L"Falha na verificação do certificado HTTPS.";
    else if (s.find("No space left")!=s.npos) query.failure=L"Espaço em disco insuficiente para o vídeo.";
    else if (s.find("Failed to load Python DLL")!=s.npos || s.find("Failed to start embedded python")!=s.npos)
        query.failure=L"O Windows não conseguiu carregar o Python x86 incluído no kit.";
    else if (s.find("ffmpeg not found")!=s.npos || s.find("ffmpeg is not installed")!=s.npos)
        query.failure=L"O Windows não conseguiu iniciar o FFmpeg do kit.";
    else if (s.find("timed out")!=s.npos) query.failure=L"Tempo de conexão esgotado.";
}
void YoutubeDownload::readOutput(Query& query) {
    char buf[4096];
    DWORD available=0,got=0;
    while (query.pipe && PeekNamedPipe(query.pipe,nullptr,0,nullptr,&available,nullptr) && available) {
        if (!ReadFile(query.pipe,buf,std::min<DWORD>(sizeof(buf),available),&got,nullptr) || !got) break;
        for (DWORD i=0;i<got;++i) {
            if (buf[i]=='\n' || buf[i]=='\r') {
                if (!query.pending.empty()) line(query,query.pending);
                query.pending.clear();
            } else if (query.pending.size()<65536) query.pending+=buf[i];
        }
    }
}
void YoutubeDownload::release(Query& query,bool terminate) {
    if (terminate && query.process) {
        TerminateJobObject(query.job,ERROR_CANCELLED);
        WaitForSingleObject(query.process,2000);
    }
    closeHandle(query.job); closeHandle(query.process); closeHandle(query.pipe);
}
bool YoutubeDownload::finish(Query& query) {
    if (!query.process) return false;
    readOutput(query);
    if (WaitForSingleObject(query.process,0)!=WAIT_OBJECT_0) return false;
    readOutput(query);
    if (!query.pending.empty()) { line(query,query.pending); query.pending.clear(); }
    query.code=1; GetExitCodeProcess(query.process,&query.code);
    release(query); query.done=true;
    return true;
}
bool YoutubeDownload::usable(const Query& query) const {
    if (!query.done || query.code) return false;
    return progressive ? !query.videoUrl.empty() && query.videoWidth && query.videoHeight : regularFile(videoPath());
}
void YoutubeDownload::select(const Query& query) {
    audioLanguage=query.language;
    journal.add(L"YouTube | idioma da faixa de áudio: "+query.language);
    channelId=query.channelId;
    title=query.title; author=query.author;
    videoUrl=query.videoUrl; audioUrl=query.audioUrl;
    durationSeconds=query.durationSeconds; videoWidth=query.videoWidth;
    videoHeight=query.videoHeight; videoFps=query.videoFps;
}
bool YoutubeDownload::complete() {
    status=progressive ? L"Abrindo transmissão…" : L"Download concluído.";
    journal.add(fmt(L"YouTube | %s | %.1f s",progressive ? L"consulta concluída" : L"MP4 pronto",
        (GetTickCount64()-started)/1000.0));
    return true;
}
bool YoutubeDownload::poll() {
    if (!active()) return false;
    bool quickFinished=finish(quickQuery);
    bool fullFinished=finish(fullQuery);
    bool quickOk=usable(quickQuery), fullOk=usable(fullQuery);
    if (progressive) {
        if (quickOk && quickQuery.videoHeight>=requestedHeight && quickQuery.language==options.audioLanguage() &&
            options.image==ImageQuality::Medium && options.fps && options.fps<=30) {
            select(quickQuery); release(fullQuery,true);
            journal.add(fmt(L"YouTube | busca rápida suficiente | %up",quickQuery.videoHeight));
            return complete();
        }
        if (quickFinished && fullQuery.process) {
            status=L"Consultando formatos adicionais…";
            JOBOBJECT_EXTENDED_LIMIT_INFORMATION limit={};
            limit.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE|JOB_OBJECT_LIMIT_PRIORITY_CLASS;
            limit.BasicLimitInformation.PriorityClass=NORMAL_PRIORITY_CLASS;
            SetInformationJobObject(fullQuery.job,JobObjectExtendedLimitInformation,&limit,sizeof(limit));
            journal.add(quickQuery.videoHeight ?
                fmt(L"YouTube | consulta ampliada já em andamento | busca rápida retornou %up, limite solicitado %up",
                    quickQuery.videoHeight,requestedHeight) :
                L"YouTube | consulta ampliada já em andamento | formato não encontrado na busca rápida");
        }
        if (fullOk) {
            select(fullQuery); release(quickQuery,true);
            journal.add(fmt(L"YouTube | consulta completa selecionada | %up | cache persistente do player ativo",
                fullQuery.videoHeight));
            return complete();
        }
        if (quickQuery.done && (!fullQuery.process || fullQuery.done)) {
            if (quickOk) {
                select(quickQuery);
                journal.add(fmt(L"YouTube | consulta completa indisponível; usando busca rápida | %up",quickQuery.videoHeight));
                return complete();
            }
        } else if (fullFinished || quickFinished) return false;
        else return false;
    } else if (fullOk) { select(fullQuery); return complete(); }

    if (active()) return false;
    const Query& failed=fullQuery.done ? fullQuery : quickQuery;
    wstring failure=failed.failure;
    if (failed.code==0xC0000139) failure=L"O Windows não encontrou uma função exigida pelo extrator.";
    if (failed.code==0xC0000135) failure=L"O Windows não encontrou uma DLL exigida pelo extrator.";
    if (failure.empty()) failure=L"Não foi possível preparar o vídeo. Verifique a conexão e a disponibilidade.";
    status=failure;
    journal.add(fmt(L"YouTube | %s | código do extrator 0x%08lX",status.c_str(),failed.code));
    clear(); return false;
}
void YoutubeDownload::clear() {
    bool wasActive=active();
    release(quickQuery,true); release(fullQuery,true);
    if (wasActive) journal.add(L"YouTube | download cancelado");
    if (!directory.empty()) {
        removeMedia(directory); closeHandle(lock);
        DeleteFileW((directory+L"\\session.lock").c_str());
        RemoveDirectoryW(directory.c_str()); directory.clear();
    }
    quickQuery=Query{}; fullQuery=Query{};
    channelId.clear();
    title.clear(); author.clear(); audioLanguage.clear();
    videoUrl.clear(); audioUrl.clear(); durationSeconds=0; videoWidth=0; videoHeight=0; videoFps=0;
}
