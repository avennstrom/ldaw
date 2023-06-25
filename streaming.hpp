#pragma once

#include "ldaw.hpp"
#include "event_queue.hpp"

struct StreamingThreadFeedback {
    uint64_t playbackPositionInSeconds;
};

enum class StreamingEventType {
    Exit,
    SetSong,
    SetVolume,
    PlaybackScan,
    DebugSong,
};

struct StreamingEvent {
    StreamingEventType type;

    Song song;
    bool resetPlayback = false;
    int volume;

    int64_t scanOffsetInSeconds;
};

struct StreamingThreadContext {
    EventQueue<StreamingEvent> queue;
    UiEventQueue* feedback;
    HWND hWnd;
    uint32_t sampleRate;
    std::vector<int16_t> waveformCapture;
    std::vector<uint8_t> stateCapture;
};

int streamingThreadEntry(StreamingThreadContext& ctx);