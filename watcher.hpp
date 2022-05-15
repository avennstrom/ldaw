#pragma once

#include "ldaw.hpp"
#include "event_queue.hpp"

enum class FileWatcherEventType {
    Exit,
    SetSong,
    RenderSong,
};

struct FileWatcherEvent {
    FileWatcherEventType type;

    std::wstring song;

    size_t renderSeconds;
};

struct FileWatcherThreadContext
{
    EventQueue<FileWatcherEvent> queue;
    UiEventQueue* feedback;
};

int fileWatcherThreadEntry(FileWatcherThreadContext& ctx);