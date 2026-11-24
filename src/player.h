#pragma once
#include "common.h"
#include "stream.h"
enum class State { Idle, Opening, Starting, Playing, Pausing, Paused, Ended, Failed };
class Player {
    ComPtr<IMFMediaSession> session;
    ComPtr<IMFMediaSource> source;
    ComPtr<IMFVideoDisplayControl> display;
    ComPtr<IQualProp> quality;
    Resources resources;
    ULONGLONG sampled=0;
    int lastDrawn=-1;
    wstring lastFrames;
    Stream stream;
    ULONGLONG streamStart=0, firstFrame=0, openTime=0;
    double streamSourceFps=0;
    LONGLONG durationMs=0, streamOffset=0, pendingPosition=-1;
    int volumePercent=100;
    bool pauseOnStart=false;
    ComPtr<IMFSimpleAudioVolume> audioVolume;
    void pollStream();
    void ready();
    void event(IMFMediaEvent* e);
    void start(bool rewind);
    void measure();
    void branch(IMFTopology* topology, IMFPresentationDescriptor* pd, IMFStreamDescriptor* stream,
                IMFActivate* sink);
public:
    PlaybackOptions options;
    bool localStream=false;
    bool localSmooth=true;
    HWND video=nullptr;
    State state=State::Idle;
    wstring status=L"Abra um MP4 para iniciar.", media, stats;
    unsigned revision=0;
    void close();
    void open(const wchar_t* path, bool cpu);
    void openStream(const std::string& videoUrl,const std::string& audioUrl,bool cpu,ULONGLONG requested,
                    unsigned width,unsigned height,double sourceFps,bool smoothEdges=true,
                    LONGLONG duration=0,LONGLONG offset=0,bool paused=false);
    void poll();
    void toggle();
    void seekLocal(LONGLONG milliseconds);
    LONGLONG position();
    LONGLONG duration() const { return durationMs; }
    void setVolume(int percent);
    void setSmoothing(bool enabled);
    void resize();
    void repaint();
    void fail(Problem p);
    bool active() const { return session != nullptr || stream.active(); }
};
