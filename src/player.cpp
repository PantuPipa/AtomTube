#include "player.h"
#include <vector>
static wstring attribute(IMFAttributes* attrs, REFGUID key) {
    UINT32 n; return SUCCEEDED(attrs->GetUINT32(key,&n)) ? std::to_wstring(n) : L"N/D";
}
void Player::close() {
    stream.close();
    localStream=false;
    if (session && state == State::Playing) measure();
    quality.Reset(); display.Reset();
    audioVolume.Reset();
    durationMs=0; streamOffset=0; pendingPosition=-1; pauseOnStart=false;
    if (session) {
        if (SUCCEEDED(session->Close())) {
            ULONGLONG deadline=GetTickCount64()+1500;
            while (GetTickCount64()<deadline) {
                ComPtr<IMFMediaEvent> e;
                HRESULT hr=session->GetEvent(MF_EVENT_FLAG_NO_WAIT,&e);
                if (hr==MF_E_NO_EVENTS_AVAILABLE) { Sleep(10); continue; }
                MediaEventType type;
                if (FAILED(hr) || FAILED(e->GetType(&type)) || type==MESessionClosed) break;
            }
        }
        session->Shutdown(); session.Reset();
    }
    if (source) { source->Shutdown(); source.Reset(); }
    state=State::Idle;
    if (video) InvalidateRect(video,nullptr,TRUE);
}
void Player::branch(IMFTopology* top, IMFPresentationDescriptor* pd, IMFStreamDescriptor* sd, IMFActivate* sink) {
    ComPtr<IMFTopologyNode> in,out;
    check(MFCreateTopologyNode(MF_TOPOLOGY_SOURCESTREAM_NODE,&in),L"Criar nó de entrada");
    check(in->SetUnknown(MF_TOPONODE_SOURCE,source.Get()),L"Definir fonte");
    check(in->SetUnknown(MF_TOPONODE_PRESENTATION_DESCRIPTOR,pd),L"Definir apresentação");
    check(in->SetUnknown(MF_TOPONODE_STREAM_DESCRIPTOR,sd),L"Definir faixa");
    check(MFCreateTopologyNode(MF_TOPOLOGY_OUTPUT_NODE,&out),L"Criar nó de saída");
    check(out->SetObject(sink),L"Conectar renderizador");
    check(top->AddNode(in.Get()),L"Adicionar entrada");
    check(top->AddNode(out.Get()),L"Adicionar saída");
    check(in->ConnectOutput(0,out.Get(),0),L"Conectar faixa");
}
void Player::open(const wchar_t* path, bool cpu) {
    close();
    status=L"Abrindo arquivo…"; media.clear(); stats.clear(); revision++;
    resources=Resources{}; sampled=0; lastDrawn=-1; lastFrames.clear();
    journal.add(cpu ? L"Novo ensaio | CPU | política DXVA_NONE" : L"Novo ensaio | DXVA solicitado | política DXVA_FULL");
    // A origem é um arquivo de disco. O resolvedor nunca recebe uma URL de rede.
    if (GetFileAttributesW(path)==INVALID_FILE_ATTRIBUTES)
        throw Problem{HRESULT_FROM_WIN32(GetLastError()),L"Abrir arquivo local"};
    ComPtr<IMFSourceResolver> resolver;
    ComPtr<IUnknown> object;
    MF_OBJECT_TYPE objectType;
    check(MFCreateSourceResolver(&resolver),L"Criar resolvedor de arquivo");
    check(resolver->CreateObjectFromURL(path,MF_RESOLUTION_MEDIASOURCE,nullptr,&objectType,&object),L"Ler contêiner MP4");
    check(object.As(&source),L"Obter fonte de mídia");
    ComPtr<IMFPresentationDescriptor> pd;
    check(source->CreatePresentationDescriptor(&pd),L"Ler apresentação");
    UINT64 duration=0;
    if (SUCCEEDED(pd->GetUINT64(MF_PD_DURATION,&duration))) durationMs=duration/10000;
    ComPtr<IMFTopology> top;
    check(MFCreateTopology(&top),L"Criar topologia");
    check(top->SetUINT32(MF_TOPOLOGY_DXVA_MODE,cpu ? MFTOPOLOGY_DXVA_NONE : MFTOPOLOGY_DXVA_FULL),L"Configurar DXVA");
    DWORD count=0;
    check(pd->GetStreamDescriptorCount(&count),L"Contar faixas");
    bool audio=false, picture=false;
    UINT32 width=0,height=0; double sourceRate=0;
    for (DWORD index=0;index<count;++index) {
        BOOL selected; ComPtr<IMFStreamDescriptor> sd;
        check(pd->GetStreamDescriptorByIndex(index,&selected,&sd),L"Ler faixa");
        if (!selected) continue;
        ComPtr<IMFMediaTypeHandler> handler;
        check(sd->GetMediaTypeHandler(&handler),L"Ler formatos");
        GUID major;
        check(handler->GetMajorType(&major),L"Identificar faixa");
        ComPtr<IMFActivate> sink;
        if (major==MFMediaType_Video && !picture) {
            ComPtr<IMFMediaType> type;
            check(handler->GetCurrentMediaType(&type),L"Ler formato de origem");
            GUID codec;
            check(type->GetGUID(MF_MT_SUBTYPE,&codec),L"Identificar codec");
            if (codec!=MFVideoFormat_H264) throw Problem{MF_E_INVALIDMEDIATYPE,L"Escolha um arquivo com vídeo H.264"};
            UINT32 w=0,h=0,n=0,d=0;
            wstring size=SUCCEEDED(MFGetAttributeSize(type.Get(),MF_MT_FRAME_SIZE,&w,&h)) ? fmt(L"%u × %u",w,h) : L"N/D";
            wstring fps=SUCCEEDED(MFGetAttributeRatio(type.Get(),MF_MT_FRAME_RATE,&n,&d)) && d ? fmt(L"%.3f",double(n)/d) : L"N/D";
            width=w; height=h; sourceRate=d ? double(n)/d : 0;
            media=L"H.264 | "+size+L" | "+fps+L" fps de origem";
            journal.add(media+L" | perfil "+attribute(type.Get(),MF_MT_MPEG2_PROFILE)+L" | nível "+attribute(type.Get(),MF_MT_MPEG2_LEVEL)
                +L" | bitrate informado (bit/s) "+attribute(type.Get(),MF_MT_AVG_BITRATE));
            check(MFCreateVideoRendererActivate(video,&sink),L"Criar EVR");
            picture=true;
        } else if (major==MFMediaType_Audio && !audio) {
            check(MFCreateAudioRendererActivate(&sink),L"Criar saída de áudio");
            audio=true;
        } else {
            check(pd->DeselectStream(index),L"Desmarcar faixa adicional");
            continue;
        }
        branch(top.Get(),pd.Get(),sd.Get(),sink.Get());
    }
    if (!picture) throw Problem{MF_E_INVALIDMEDIATYPE,L"Arquivo sem faixa H.264 selecionada"};
    if ((options.fps && sourceRate>options.fps+0.01) ||
        options.image==ImageQuality::High || options.image==ImageQuality::Low || options.image==ImageQuality::Potato) {
        top.Reset(); pd.Reset(); source->Shutdown(); source.Reset(); object.Reset();
        localStream=true; streamSourceFps=sourceRate;
        streamStart=openTime=GetTickCount64(); firstFrame=0;
        stream.open(video,{},{},cpu,defaultVideoAdapter().rgbOutput,width,height,sourceRate,localSmooth,0,options,path);
        setVolume(volumePercent); state=State::Opening; return;
    }
    journal.add(audio ? L"Saídas solicitadas: vídeo EVR e áudio do Windows." : L"Saída solicitada: EVR; arquivo sem áudio selecionado.");
    check(MFCreateMediaSession(nullptr,&session),L"Criar sessão");
    check(session->SetTopology(0,top.Get()),L"Resolver decodificadores e saídas");
    state=State::Opening;
}
void Player::start(bool rewind) {
    PROPVARIANT pos={};
    if (rewind) { pos.vt=VT_I8; pos.hVal.QuadPart=0; }
    check(session->Start(&GUID_NULL,&pos),L"Iniciar reprodução");
    state=State::Starting; status=L"Iniciando…"; revision++;
}
void Player::ready() {
    MFGetService(session.Get(),MR_POLICY_VOLUME_SERVICE,IID_PPV_ARGS(&audioVolume));
    setVolume(volumePercent);
    check(MFGetService(session.Get(),MR_VIDEO_RENDER_SERVICE,IID_PPV_ARGS(&display)),L"Obter janela EVR");
    MFGetService(session.Get(),MR_VIDEO_RENDER_SERVICE,IID_IQualProp,reinterpret_cast<void**>(quality.GetAddressOf()));
    if (!quality) display->QueryInterface(IID_IQualProp,reinterpret_cast<void**>(quality.GetAddressOf()));
    resize();
    ComPtr<IMFTopology> resolved;
    if (SUCCEEDED(session->GetFullTopology(MFSESSION_GETFULLTOPOLOGY_CURRENT,0,&resolved))) {
        WORD count=0; resolved->GetNodeCount(&count);
        for (WORD i=0;i<count;++i) {
            ComPtr<IMFTopologyNode> node;
            MF_TOPOLOGY_TYPE kind;
            if (FAILED(resolved->GetNode(i,&node)) || FAILED(node->GetNodeType(&kind)) || kind!=MF_TOPOLOGY_TRANSFORM_NODE) continue;
            GUID clsid;
            wchar_t guid[48]=L"N/D";
            wstring name;
            if (SUCCEEDED(node->GetGUID(MF_TOPONODE_TRANSFORM_OBJECTID,&clsid))) {
                StringFromGUID2(clsid,guid,48);
                LPWSTR friendly=nullptr;
                if (SUCCEEDED(MFTGetInfo(clsid,&friendly,nullptr,nullptr,nullptr,nullptr,nullptr)) && friendly)
                    name=wstring(friendly)+L" ";
                CoTaskMemFree(friendly);
            }
            ComPtr<IUnknown> obj; ComPtr<IMFTransform> transform; ComPtr<IMFMediaType> type;
            GUID major,sub;
            if (SUCCEEDED(node->GetObject(&obj)) && SUCCEEDED(obj.As(&transform)) && SUCCEEDED(transform->GetOutputCurrentType(0,&type)) &&
                SUCCEEDED(type->GetGUID(MF_MT_MAJOR_TYPE,&major)) && major==MFMediaType_Video) {
                wstring format=L"N/D";
                if (SUCCEEDED(type->GetGUID(MF_MT_SUBTYPE,&sub))) {
                    wchar_t id[48]; StringFromGUID2(sub,id,48); format=id;
                    if (sub==MFVideoFormat_NV12) format=L"NV12";
                    if (sub==MFVideoFormat_YUY2) format=L"YUY2";
                }
                journal.add(L"Transformação de vídeo negociada "+name+guid+L" | saída "+format);
            }
        }
    }
    journal.add(L"EVR negociado | DXVA ativo no decodificador: N/D | buffer, tempo por quadro e desvio A/V: N/D");
    start(false);
}
void Player::measure() {
    ULONGLONG now=GetTickCount64();
    wstring fps=L"N/D",drawn=L"N/D",dropped=L"N/D";
    if (quality) {
        int n=0;
        if (SUCCEEDED(quality->get_FramesDrawn(&n))) {
            drawn=std::to_wstring(n);
            if (sampled && now>sampled && lastDrawn>=0 && n>=lastDrawn)
                fps=fmt(L"%.2f",1000.0*(n-lastDrawn)/(now-sampled));
            lastDrawn=n;
        }
        if (SUCCEEDED(quality->get_FramesDroppedInRenderer(&n))) dropped=std::to_wstring(n);
    }
    sampled=now;
    wstring frames=L"FPS apresentados "+fps+L" | exibidos "+drawn+L" | perdidos no EVR "+dropped;
    if (drawn!=L"N/D") lastFrames=frames;
    else if (!lastFrames.empty()) frames=L"Última leitura válida: "+lastFrames;
    stats=frames+L"\r\n"+resources.sample();
    journal.add(stats); revision++;
}
void Player::event(IMFMediaEvent* e) {
    MediaEventType type; HRESULT hr;
    check(e->GetType(&type),L"Identificar evento");
    check(e->GetStatus(&hr),L"Ler estado do evento");
    if (FAILED(hr)) {
        journal.add(fmt(L"Evento Media Foundation %u",type));
        throw Problem{hr,L"Executar sessão de mídia"};
    }
    if (type==MEError) throw Problem{E_FAIL,L"Evento de erro da sessão"};
    if (type==MESessionTopologyStatus && MFGetAttributeUINT32(e,MF_EVENT_TOPOLOGY_STATUS,0)==MF_TOPOSTATUS_READY) ready();
    if (type==MESessionStarted) {
        pendingPosition=-1;
        state=State::Playing; status=L"Reproduzindo"; sampled=0; lastDrawn=-1; lastFrames.clear(); measure();
        if (pauseOnStart) { pauseOnStart=false; toggle(); }
    } else if (type==MESessionPaused || type==MESessionEnded) {
        measure(); state=type==MESessionEnded ? State::Ended : State::Paused;
        status=type==MESessionEnded ? L"Concluído" : L"Em pausa";
    } else if (type==MEBufferingStarted || type==MEBufferingStopped) {
        journal.add(type==MEBufferingStarted ? L"Buffer: aguardando dados" : L"Buffer: retomada");
    } else return;
    journal.add(status); revision++;
}
void Player::poll() {
    if (stream.active()) { pollStream(); return; }
    if (!session) return;
    for (;;) {
        ComPtr<IMFMediaEvent> e;
        HRESULT hr=session->GetEvent(MF_EVENT_FLAG_NO_WAIT,&e);
        if (hr==MF_E_NO_EVENTS_AVAILABLE) break;
        check(hr,L"Ler eventos da sessão"); event(e.Get());
    }
    if (state==State::Playing && GetTickCount64()-sampled>=5000) measure();
}
void Player::toggle() {
    if (stream.active()) {
        if (localStream && state==State::Ended) { stream.replay(); state=State::Starting; revision++; }
        if (state==State::Playing || state==State::Paused) stream.pause(state==State::Playing);
        return;
    }
    if (state==State::Playing) {
        check(session->Pause(),L"Pausar"); state=State::Pausing;
    } else if (state==State::Paused || state==State::Ended) start(state==State::Ended);
    revision++;
}
void Player::openStream(const std::string& videoUrl,const std::string& audioUrl,bool cpu,ULONGLONG requested,
                        unsigned width,unsigned height,double sourceFps,bool smoothEdges,
                        LONGLONG duration,LONGLONG offset,bool paused) {
    close();
    media.clear(); stats.clear(); resources=Resources{};
    streamSourceFps=sourceFps;
    streamStart=requested; openTime=GetTickCount64(); firstFrame=0; sampled=0; lastDrawn=-1;
    journal.add(cpu ? L"Streaming H.264 + AAC | CPU" : L"Streaming H.264 + AAC | preferência DXVA2");
    durationMs=duration; streamOffset=offset; pauseOnStart=paused;
    stream.open(video,videoUrl,audioUrl,cpu,defaultVideoAdapter().rgbOutput,width,height,sourceFps,smoothEdges,offset,options);
    setVolume(volumePercent);
    status=L"Carregando o início do vídeo…"; state=State::Opening; revision++;
}
void Player::pollStream() {
    int s=stream.state();
    ULONGLONG now=GetTickCount64();
    DWORD code=stream.transferCode();
    if (s==7 || s==5 || (code!=STILL_ACTIVE && code!=0) || (!firstFrame && now-openTime>60000)) {
        journal.add(fmt(L"Streaming | estado libVLC %d | código da transferência 0x%08lX",s,code));
        throw Problem{E_FAIL,L"Falha ao abrir a transmissão. Tente novamente ou escolha MP4 completo"};
    }
    if (s==4 && state!=State::Paused) { state=State::Paused; status=L"Em pausa"; revision++; }
    if (s==3 && state!=State::Playing) {
        state=State::Playing; status=L"Reproduzindo"; revision++;
        if (localStream && pendingPosition>=0) { stream.seek(pendingPosition); pendingPosition=-1; }
        if (!firstFrame) setVolume(volumePercent);
        if (pauseOnStart) { pauseOnStart=false; stream.pause(true); }
    }
    if (s==2 && state!=State::Starting) { state=State::Starting; status=L"Carregando dados…"; revision++; }
    if (firstFrame && now-sampled<5000 && s!=6) return;
    StreamStats values={};
    if (stream.stats(values) && (values.displayedPictures>0 || firstFrame)) {
        int presented=stream.presentationLimited() ? stream.presentationFrames() : values.displayedPictures;
        if (!firstFrame) {
            firstFrame=now;
            unsigned w=0,h=0; stream.size(w,h);
            if (w && h) {
                media=fmt(L"H.264 | %u × %u | fonte %.3f fps",w,h,streamSourceFps);
                if (stream.presentationLimited()) media+=fmt(L" | apresentação %u fps",stream.fpsLimit());
                journal.add(media);
            }
            journal.add(fmt(L"YouTube | primeiro quadro observado %.2f s desde Assistir | %.2f s após consulta | bytes lidos pelo player %d | transferência %s",
                (now-streamStart)/1000.0,(now-openTime)/1000.0,values.readBytes,stream.transferActive() ? L"em andamento" : L"encerrada"));
        }
        wstring fps=sampled && now>sampled ? fmt(L"%.2f",1000.0*(presented-lastDrawn)/(now-sampled)) : L"N/D";
        stats=fmt(L"FPS apresentados %s | exibidos %d | perdidos %d | buffers de áudio %d",fps.c_str(),
            presented,values.lostPictures,values.playedAudioBuffers)+L"\r\n"+resources.sample();
        journal.add(stats); sampled=now; lastDrawn=presented; revision++;
        if (!localStream && s==3 && now-firstFrame>6000 && stream.audioTrack()<0)
            throw Problem{E_FAIL,L"Áudio indisponível no streaming. Escolha MP4 completo"};
    }
    if (s==6 && state!=State::Ended) {
        if (!firstFrame) throw Problem{E_FAIL,L"Transmissão encerrada sem exibir vídeo. Escolha MP4 completo"};
        state=State::Ended; status=L"Concluído"; journal.add(status); revision++;
    }
}
void Player::resize() {
    if (stream.active()) { stream.resize(); return; }
    RECT r;
    if (display && GetClientRect(video,&r) && r.right && r.bottom) display->SetVideoPosition(nullptr,&r);
}
void Player::setSmoothing(bool enabled) { if (stream.active()) stream.setSmoothing(enabled); }
void Player::setVolume(int percent) {
    volumePercent=percent;
    if (stream.active()) stream.volume(percent);
    if (audioVolume) audioVolume->SetMasterVolume(percent/100.0f);
}
LONGLONG Player::position() {
    if (state==State::Ended && durationMs>0) return durationMs;
    if (pendingPosition>=0) return pendingPosition;
    if (stream.active()) {
        auto elapsed=stream.position();
        return streamOffset+(elapsed>0 ? elapsed : 0);
    }
    ComPtr<IMFClock> clock;
    ComPtr<IMFPresentationClock> presentation;
    LONGLONG time=0;
    if (session && SUCCEEDED(session->GetClock(&clock)) && SUCCEEDED(clock.As(&presentation)))
        presentation->GetTime(&time);
    return time/10000;
}
void Player::seekLocal(LONGLONG milliseconds) {
    if (localStream && stream.active()) {
        if (state==State::Ended) { pendingPosition=milliseconds; stream.replay(); state=State::Starting; revision++; }
        else stream.seek(milliseconds);
        return;
    }
    if (!session || (state!=State::Playing && state!=State::Paused && state!=State::Ended)) return;
    PROPVARIANT pos={};
    pos.vt=VT_I8; pos.hVal.QuadPart=milliseconds*10000;
    check(session->Start(&GUID_NULL,&pos),L"Mover posição do vídeo");
    pauseOnStart=state==State::Paused;
    pendingPosition=milliseconds; state=State::Starting; status=L"Buscando posição…"; revision++;
}
void Player::repaint() {
    if (stream.active()) stream.repaint(video);
    else if (display) display->RepaintVideo();
}
void Player::fail(Problem p) {
    status=fmt(L"%s (0x%08lX).",p.operation,static_cast<unsigned long>(p.hr));
    journal.add(L"ERRO | "+status);
    close();
    state=State::Failed; revision++;
}
