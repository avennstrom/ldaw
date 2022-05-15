#include "streaming.hpp"

#include <dsound.h>

struct BufferCreateInfo
{
    uint8_t sampleSize;
    uint32_t sampleRate;
    uint32_t sampleCount;
};

static HRESULT CreateBasicBuffer(LPDIRECTSOUND8 lpDirectSound, const BufferCreateInfo& info, LPDIRECTSOUNDBUFFER8* ppDsb8)
{
    LPDIRECTSOUNDBUFFER pDsb = NULL;
    HRESULT hr;

    WAVEFORMATEX wfx;
    memset(&wfx, 0, sizeof(WAVEFORMATEX));
    wfx.wFormatTag = WAVE_FORMAT_PCM;
    wfx.nChannels = 1;
    wfx.nSamplesPerSec = info.sampleRate;
    wfx.nBlockAlign = 2;
    wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;
    wfx.wBitsPerSample = info.sampleSize * 8;

    DSBUFFERDESC dsbdesc;
    memset(&dsbdesc, 0, sizeof(DSBUFFERDESC));
    dsbdesc.dwSize = sizeof(DSBUFFERDESC);
    dsbdesc.dwFlags = DSBCAPS_GLOBALFOCUS | DSBCAPS_CTRLPOSITIONNOTIFY | DSBCAPS_CTRLVOLUME;
    dsbdesc.dwBufferBytes = info.sampleCount * info.sampleSize * 2; // double buffered
    dsbdesc.lpwfxFormat = &wfx;

    hr = lpDirectSound->CreateSoundBuffer(&dsbdesc, &pDsb, NULL);
    if (SUCCEEDED(hr))
    {
        hr = pDsb->QueryInterface(IID_IDirectSoundBuffer8, (LPVOID*)ppDsb8);
        pDsb->Release();
    }
    return hr;
}

int streamingThreadEntry(StreamingThreadContext& ctx)
{
    HRESULT hr;
    
    IDirectSound8* ds;
    hr = DirectSoundCreate8(NULL, &ds, NULL);
    if (FAILED(hr)) {
        fprintf(stderr, "Failed to initialize DirectSound!\n");
        return 1;
    }

    hr = ds->SetCooperativeLevel(ctx.hWnd, DSSCL_PRIORITY);
    if (FAILED(hr)) {
        return 1;
    }

    BufferCreateInfo bufferInfo;
    bufferInfo.sampleSize   = sizeof(uint16_t);
    bufferInfo.sampleRate   = ctx.sampleRate;
    bufferInfo.sampleCount  = 4096;

    IDirectSoundBuffer8* buffer;
    hr = CreateBasicBuffer(ds, bufferInfo, &buffer);
    if (FAILED(hr)) {
        return 1;
    }

    

    IDirectSoundNotify8* notify;
    hr = buffer->QueryInterface(IID_IDirectSoundNotify8, (void**)&notify);
    if (FAILED(hr)) {
        return 1;
    }

    HANDLE notifyHandles[2];
    memset(notifyHandles, 0, sizeof(notifyHandles));
    notifyHandles[0] = CreateEvent(nullptr, FALSE, FALSE, TEXT("BufferStart"));
    notifyHandles[1] = CreateEvent(nullptr, FALSE, FALSE, TEXT("BufferMiddle"));

    DSBPOSITIONNOTIFY notifyPositions[2];
    memset(notifyPositions, 0, sizeof(notifyPositions));
    notifyPositions[0].dwOffset         = 0;
    notifyPositions[0].hEventNotify     = notifyHandles[0];
    notifyPositions[1].dwOffset         = bufferInfo.sampleCount * sizeof(uint16_t);
    notifyPositions[1].hEventNotify     = notifyHandles[1];

    hr = notify->SetNotificationPositions(_countof(notifyPositions), notifyPositions);
    if (FAILED(hr)) {
        return 1;
    }

    notify->Release();

    void* bufferMemory;
    DWORD audioBytes;
    hr = buffer->Lock(0, 0, &bufferMemory, &audioBytes, nullptr, nullptr, DSBLOCK_ENTIREBUFFER);
    if (FAILED(hr)) {
        return 1;
    }

    if (audioBytes != bufferInfo.sampleCount * bufferInfo.sampleSize * 2) {
        return 1;
    }

    int16_t* const bufferSamples = (int16_t*)bufferMemory;

    memset(bufferSamples, 0, sizeof(bufferInfo.sampleCount * sizeof(uint16_t) * 2));
    buffer->Play(0, 0, DSBPLAY_LOOPING);

    LARGE_INTEGER counterFreq;
    QueryPerformanceFrequency(&counterFreq);

    const size_t waveformBufferSize = bufferInfo.sampleCount * sizeof(int16_t);
    ctx.waveformBuffer = (int16_t*)malloc(waveformBufferSize);
    memset(ctx.waveformBuffer, 0, waveformBufferSize);

    Song song;
    uint64_t baseSample = 0;
    bool debugSong = false;
    int volume = DSBVOLUME_MAX;

    while (true)
    {
        const HANDLE waitHandles[] = { notifyHandles[0], notifyHandles[1], ctx.queue.getHandle() };
        const DWORD r = WaitForMultipleObjects(_countof(waitHandles), waitHandles, FALSE, INFINITE);

        {
            StreamingEvent event;
            while (ctx.queue.pop(event)) {
                switch (event.type) {
                    case StreamingEventType::Exit: 
                        return 0;
                    case StreamingEventType::SetSong:
                        if (song.hModule != nullptr) {
                            FreeModule(song.hModule);
                            DeleteFile((getIntermediateDirectory() + L'\\' + song.filename + L".dll").c_str());
                            DeleteFile((getIntermediateDirectory() + L'\\' + song.filename + L".ilk").c_str());
                            DeleteFile((getIntermediateDirectory() + L'\\' + song.filename + L".pdb").c_str());
                        }
                        song = event.song;
                        if (event.resetPlayback) {
                            baseSample = 0;
                        }
                        break;
                    case StreamingEventType::DebugSong: 
                        debugSong = true;
                        break;
                    case StreamingEventType::SetVolume: {
                        volume = event.volume;
                        break;
                    }
                    case StreamingEventType::PlaybackScan: {
                        const int64_t scan = -min((int64_t)baseSample, -event.scanOffsetInSeconds * (int64_t)bufferInfo.sampleRate);
                        baseSample += scan;
                        break;
                    }
                }
            }
        }

        buffer->SetVolume(volume);

        if (r != WAIT_OBJECT_0 && r != (WAIT_OBJECT_0 + 1)) {
            continue;
        }

        if (song.play == nullptr) {
            continue;
        }

        const size_t writeOffset = (r == WAIT_OBJECT_0) ? bufferInfo.sampleCount : 0;

        if (debugSong) {
            debugSong = false;
            DebugBreak();
        }

        song.play(bufferSamples + writeOffset, bufferInfo.sampleCount, bufferInfo.sampleRate, baseSample);

        memcpy(ctx.waveformBuffer, bufferSamples + writeOffset, bufferInfo.sampleCount* bufferInfo.sampleSize);

        //LARGE_INTEGER endTime;
        //QueryPerformanceCounter(&endTime);

        //const double duration = ((endTime.QuadPart - startTime.QuadPart) * 1000)  / (double)counterFreq.QuadPart;
        //printf("Duration: %f\n", duration);

        baseSample += bufferInfo.sampleCount;

        {
            UiEvent event;
            event.type = UiEventType::Playback;
            event.playback.playing = true;
            event.playback.samples = baseSample;
            event.playback.seconds = (baseSample / bufferInfo.sampleRate);
            ctx.feedback->push(event);
        }
    }

    buffer->Release();
    ds->Release();

    return 0;
}