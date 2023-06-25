#pragma once

#include "ldaw.hpp"
#include "event_queue.hpp"

enum class ExportFormat {
    Wav,
    Wasm,

    Count,
};

enum class FileWatcherEventType {
    Exit,
    SetSong,
    ExportSong,
};

struct FileWatcherEventData_SetSong {
    std::wstring song;
};

struct FileWatcherEventData_Export {
    ExportFormat format;
    size_t durationInSeconds; // not applicable to Wasm
};

struct FileWatcherEvent {
    FileWatcherEventType type;
    struct {
        FileWatcherEventData_SetSong setSong;
        FileWatcherEventData_Export exportSong;
    } data;
};

struct FileWatcherThreadContext
{
    EventQueue<FileWatcherEvent> queue;
    UiEventQueue* feedback;

    const wchar_t* cwd;
    const wchar_t* intdir;
};

int fileWatcherThreadEntry(FileWatcherThreadContext& ctx);