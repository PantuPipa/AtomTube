#pragma once
#include "common.h"
#include "options.h"

// The UI thread polls the child. Only fixed status messages are exported.
wstring youtubeVideoId(wstring link);
bool youtubeChannelId(const wstring& id);
class YoutubeDownload {
    PlaybackOptions options;
    struct Query {
        HANDLE process=nullptr, job=nullptr, pipe=nullptr;
        std::string pending, videoUrl, audioUrl;
        wstring failure,channelId,title,author;
        wstring language;
        unsigned durationSeconds=0, videoWidth=0, videoHeight=0;
        double videoFps=0;
        DWORD code=STILL_ACTIVE;
        bool missingFormat=false, done=false;
    } quickQuery, fullQuery;
    HANDLE lock=INVALID_HANDLE_VALUE;
    wstring directory;
    ULONGLONG started=0;
    bool progressive=false;
    wstring requestedId;
    unsigned requestedHeight=0;
    void begin(const wstring& id,unsigned height,bool stream);
    void launch(Query& query,const wstring& exe,const wstring& qjs,const wstring& ff,
                const wstring& root,bool fast,bool background);
    void line(Query& query,const std::string& text);
    void readOutput(Query& query);
    void release(Query& query,bool terminate=false);
    bool finish(Query& query);
    bool usable(const Query& query) const;
    void select(const Query& query);
    bool complete();
public:
    wstring status,channelId,title,author,audioLanguage;
    std::string videoUrl, audioUrl;
    unsigned durationSeconds=0, videoWidth=0, videoHeight=0;
    double videoFps=0;
    ULONGLONG startTime() const { return started; }
    bool active() const { return quickQuery.process!=nullptr || fullQuery.process!=nullptr; }
    wstring videoPath() const { return directory+L"\\video.mp4"; }
    void start(const wstring& id, unsigned height, bool stream=true,PlaybackOptions settings={});
    bool poll(); // true when the completed MP4 is ready
    void clear(); // call after closing the player that owns the cached MP4
    ~YoutubeDownload() { clear(); }
};
