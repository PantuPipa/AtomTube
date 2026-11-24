#pragma once
#include "common.h"
#include "options.h"
#include <d3d9.h>
#include <vector>

struct StreamStats {
    int readBytes; float inputBitrate;
    int demuxReadBytes; float demuxBitrate; int demuxCorrupted, demuxDiscontinuity;
    int decodedVideo, decodedAudio, displayedPictures, lostPictures;
    int playedAudioBuffers, lostAudioBuffers, sentPackets, sentBytes; float sendBitrate;
};

// Only used for online playback. Local MP4 playback uses the Windows media session.
class Stream {
    HMODULE dll=nullptr;
    HANDLE transfer=nullptr, pipe=nullptr, job=nullptr;
    HWND output=nullptr;
    SRWLOCK frameLock=SRWLOCK_INIT;
    unsigned char* frame=nullptr;
    unsigned frameWidth=0, frameHeight=0, framePitch=0;
    bool frameReady=false, memoryOutput=false;
    bool limitPresentation=false, smoothEdges=true;
    unsigned targetFps=24;
    IDirect3D9* presenterD3d=nullptr;
    IDirect3DDevice9* presenterDevice=nullptr;
    IDirect3DSurface9* presenterUpload=nullptr;
    IDirect3DSurface9* presenterVideo=nullptr;
    D3DPRESENT_PARAMETERS presenterParameters={};
    LARGE_INTEGER clockFrequency={}, nextPresentation={};
    volatile LONG presentedFrames=0;
    void connect(const wstring& exe,const std::string& video,const std::string& audio,LONGLONG offset);
    static intptr_t read(void* opaque,unsigned char* bytes,size_t length);
    static void* lock(void* opaque,void** planes);
    static void unlock(void* opaque,void* picture,void* const* planes);
    static void display(void* opaque,void* picture);
    bool createPresenter();
    bool resetPresenter();
    bool repaintPresenter();
    void releasePresenterSurfaces();
    void releasePresenter();
    void *instance=nullptr, *media=nullptr, *player=nullptr;
    template<typename T> T symbol(const char* name) {
        auto ptr=GetProcAddress(dll,name);
        if (!ptr) throw Problem{E_NOINTERFACE,L"Versão incompatível da biblioteca de streaming"};
        return reinterpret_cast<T>(ptr);
    }
public:
    bool active() const { return player!=nullptr; }
    void open(HWND window,const std::string& video,const std::string& audio,bool cpu,bool rgbOutput,
              unsigned width,unsigned height,double sourceFps,bool smooth=true,LONGLONG offset=0,
              PlaybackOptions settings={},const wchar_t* localPath=nullptr);
    void seek(LONGLONG ms);
    void replay();
    unsigned fpsLimit() const { return targetFps; }
    void close();
    int state();
    bool stats(StreamStats& stats);
    void size(unsigned& width,unsigned& height);
    int audioTrack();
    bool transferActive() const { return transfer && WaitForSingleObject(transfer,0)==WAIT_TIMEOUT; }
    DWORD transferCode() const;
    void pause(bool paused);
    LONGLONG position();
    void volume(int percent);
    void setSmoothing(bool enabled);
    void resize();
    void repaint(HWND window);
    bool presentedPixels(const POINT* clientPoints,COLORREF* colors,unsigned count);
    int presentationFrames() const { return presentedFrames; }
    bool presentationLimited() const { return limitPresentation; }
};

