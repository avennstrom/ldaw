#include "watcher.hpp"
#include "ldaw.hpp"

extern "C" {
#include "md5.h"
}

#include <unordered_set>
#include <fstream>

#include <wmcodecdsp.h>

std::string GetLastErrorAsString()
{
    //Get the error message ID, if any.
    DWORD errorMessageID = ::GetLastError();
    if (errorMessageID == 0) {
        return std::string(); //No error message has been recorded
    }

    LPSTR messageBuffer = nullptr;

    //Ask Win32 to give us the string version of that message ID.
    //The parameters we pass in, tell Win32 to create the buffer that holds the message for us (because we don't yet know how long the message string will be).
    size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, errorMessageID, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);

    //Copy the error message into a std::string.
    std::string message(messageBuffer, size);

    //Free the Win32's string's buffer.
    LocalFree(messageBuffer);

    return message;
}

static int writeBatchFile(std::wstring path, std::wstring dllname, std::wstring songName) {
    const std::string vcvars = "C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\VC\\Auxiliary\\Build\\vcvars64.bat";
    const std::wstring exedir = getExecutableDirectory();

    std::ofstream f(path);
    if (!f.is_open()) {
        return 1;
    }

    f << "@echo off\n";
    f << "chcp 65001\n";
    f << "call \"" << vcvars << "\"\n";
    f << "cl /Od /Zi /LD -I\"" << wstringToUtf8(exedir) << "\" ..\\" << wstringToUtf8(songName) << ".c /link /OUT:" << wstringToUtf8(dllname) << ".dll\n";

    return 0;
}

static int compileSong(Song* song, std::wstring name, std::vector<std::string>& errors, std::vector<std::string>& messages)
{
    UUID uuid;
    if (UuidCreate(&uuid) != RPC_S_OK) {
        return 1;
    }

    RPC_WSTR uuidstr;
    if (UuidToString(&uuid, &uuidstr) != RPC_S_OK) {
        return 1;
    }

    const std::wstring intdir = getIntermediateDirectory();
    const std::wstring bat = intdir + L'\\' + name + L".bat";
    const std::wstring dllname = name + L"." + uuidstr;
    const std::wstring dllpath = intdir + L"\\" + dllname + L".dll";

    messages.push_back(wstringToUtf8(name) + ".c -> " + wstringToUtf8(dllname) + ".dll");

    if (writeBatchFile(bat, dllname, name) != 0) {
        return 1;
    }

    HANDLE hChildStdOutRead;
    HANDLE hChildStdOutWrite;

    SECURITY_ATTRIBUTES sa;
    ZeroMemory(&sa, sizeof(sa));
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;

    if (!CreatePipe(&hChildStdOutRead, &hChildStdOutWrite, &sa, 0)) {
        return 1;
    }

    if (!SetHandleInformation(hChildStdOutRead, HANDLE_FLAG_INHERIT, 0)) {
        return 1;
    }

    STARTUPINFO si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.hStdError = hChildStdOutWrite;
    si.hStdOutput = hChildStdOutWrite;
    si.dwFlags |= STARTF_USESTDHANDLES;

    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));

    WCHAR cmdline[256];
    wsprintf(cmdline, L"/C \"%ws\"", bat.c_str());

    wchar_t cmd[MAX_PATH];
    DWORD r = GetEnvironmentVariable(TEXT("COMSPEC"), cmd, _countof(cmd));
    if (r == 0 || r > _countof(cmd)) {
        return 1;
    }

    if (!CreateProcess(cmd, cmdline, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, intdir.c_str(), &si, &pi)) {
        printf("%s\n", GetLastErrorAsString().c_str());
        return 1;
    }
    
    r = WaitForSingleObject(pi.hProcess, 10 * 1000);
    if (r != WAIT_OBJECT_0) {
        //errors.push_back("Internal error: timeout while waiting for compiler");
        return 1;
    }

    DWORD exitCode;
    if (!GetExitCodeProcess(pi.hProcess, &exitCode)) {
        return 1;
    }

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    CloseHandle(hChildStdOutWrite);

    std::string output;
    output.reserve(4096);

    while (true)
    {
        char buf[1024];
        DWORD len;
        if (!ReadFile(hChildStdOutRead, buf, sizeof(buf), &len, NULL) || len == 0) {
            break;
        }

        output.insert(output.end(), buf, buf + len);
    }

    if (!output.empty()) {
        const char* ptr = output.c_str();

        while (ptr != nullptr) {
            const char* err = strstr(ptr, "error");
            if (err == nullptr) {
                break;
            }

            const char* nl = strstr(err, "\r\n");
            if (nl == nullptr) {
                break;
            }

            const ptrdiff_t len = std::distance(err, nl);
            errors.push_back(std::string(err, len));

            ptr = nl + 2;
        }
    }

    if (exitCode != 0) {
        errors.push_back(wstringToUtf8(bat) + " failed with code " + std::to_string(exitCode));
        return 1;
    }

    song->name = name;
    song->filename = dllname;

    song->hModule = LoadLibrary(dllpath.c_str());
    if (song->hModule == NULL) {
        errors.push_back("Failed to load module!");
        return 1;
    }

    song->getInfo = (GetInfo)GetProcAddress(song->hModule, "info");

    song->play = (Play)GetProcAddress(song->hModule, "play");
    if (song->play == nullptr) {
        errors.push_back("Song is missing entry point.");
        return 1;
    }

    return 0;
}

static void writeWavChunkHeader(std::ostream& stream, std::string id, uint32_t size)
{
    stream.write(id.c_str(), 4);
    stream.write((const char*)&size, sizeof(size));
}

static int renderSong(std::wstring name, const Song& song, std::vector<std::string>& errors, std::vector<std::string>& messages)
{
    const size_t sampleRate = 44100;
    const size_t seconds = 120 * 2;
    const size_t sampleCount = sampleRate * seconds;
    const size_t channelCount = 1;
    const size_t dataSize = channelCount * sampleCount * sizeof(uint16_t);

    std::vector<int16_t> samples(channelCount * sampleCount);
    song.play(samples.data(), sampleCount, sampleRate, 0);

    if (true) {
        //CLSID_MP3ACMCodecWrapper
    } 
    else {
        struct RiffHeader
        {
            char typeId[4];
        };

        struct FmtHeader
        {
            uint16_t formatTag;
            uint16_t channelCount;
            uint32_t sampleRate;
            uint32_t byteRate;
            uint16_t blockAlign;
            uint16_t bitsPerSample;
        };

        RiffHeader riffHeader;
        memcpy(riffHeader.typeId, "WAVE", 4);

        FmtHeader fmtHeader;
        fmtHeader.formatTag = 1;
        fmtHeader.channelCount = channelCount;
        fmtHeader.sampleRate = sampleRate;
        fmtHeader.byteRate = channelCount * sampleRate * sizeof(uint16_t);
        fmtHeader.blockAlign = channelCount * sizeof(uint16_t);
        fmtHeader.bitsPerSample = sizeof(uint16_t) * 8;

        const std::wstring filename = name + L".wav";
        std::ofstream f(filename, std::ios::binary);
        if (!f.is_open())
        {
            errors.push_back("Failed to open destination: " + wstringToUtf8(filename));
            return 1;
        }

        writeWavChunkHeader(f, "RIFF", 36 + dataSize);
        f.write((const char*)&riffHeader, sizeof(riffHeader));

        writeWavChunkHeader(f, "fmt ", 16);
        f.write((const char*)&fmtHeader, sizeof(fmtHeader));

        writeWavChunkHeader(f, "data", dataSize);
        f.write((const char*)samples.data(), samples.size());

        f.close();

        messages.push_back("Done! " + wstringToUtf8(filename));
    }

    return 0;
}

int fileWatcherThreadEntry(FileWatcherThreadContext& ctx)
{
    const std::wstring cwd = getWorkingDirectory();

    std::unordered_set<std::wstring> songs;

    {
        const std::wstring pattern = cwd + L"\\*";

        WIN32_FIND_DATA findData;
        HANDLE hFindFile = FindFirstFile(pattern.c_str(), &findData);
        do {
            if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                continue;
            }

            const std::wstring filename = findData.cFileName;
            if (filename[filename.length() - 1] != 'c' || filename[filename.length() - 2] != '.') {
                continue;
            }

            songs.emplace(filename.substr(0, filename.length() - 2));
        } while (FindNextFile(hFindFile, &findData));

        UiEvent event;
        event.type = UiEventType::Songs;
        for (const std::wstring& song : songs) {
            event.songs.push_back(song);
        }
        ctx.feedback->push(event);
    }

    struct FileHash {
        uint8_t data[16];
    };

    HANDLE hDir = CreateFile(cwd.c_str(), FILE_LIST_DIRECTORY, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, NULL);
    if (hDir == INVALID_HANDLE_VALUE) {
        return 1;
    }

    OVERLAPPED overlapped;
    ZeroMemory(&overlapped, sizeof(overlapped));
    overlapped.hEvent = CreateEvent(NULL, FALSE, FALSE, TEXT("DirectoryChanges"));

    bool readChanges = true;
    char changeBuffer[4 * 1024];

    std::wstring currentSong;
    Song song;

    const auto compileCurrentSong = [&]() {
        {
            UiEvent event;
            event.type = UiEventType::SongStatus;
            event.isCompiling = true;
            ctx.feedback->push(event);
        }

        UiEvent event;
        event.isCompiling = false;
        event.type = UiEventType::SongStatus;

        song = Song();
        if (compileSong(&song, currentSong, event.songErrors, event.songMessages) == 0) {
            event.song = song;
            event.songName = currentSong;
        }

        ctx.feedback->push(event);
    };

    const auto renderCurrentSong = [&](uint64_t durationInSeconds) {
        {
            UiEvent event;
            event.type = UiEventType::RenderStatus;
            event.isRendering = true;
            ctx.feedback->push(event);
        }

        UiEvent event;
        event.type = UiEventType::RenderStatus;
        event.isRendering = false;
        renderSong(currentSong, song, event.renderErrors, event.renderMessages);

        ctx.feedback->push(event);
    };

    while (true) {
        if (readChanges) {
            readChanges = false;
            BOOL asd = ReadDirectoryChangesW(hDir, changeBuffer, sizeof(changeBuffer), FALSE, FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_FILE_NAME, NULL, &overlapped, NULL);
        }

        const HANDLE events[] = { overlapped.hEvent, ctx.queue.getHandle() };
        DWORD r = WaitForMultipleObjects(_countof(events), events, FALSE, INFINITE);

        FileWatcherEvent event;
        while (ctx.queue.pop(event)) {
            switch (event.type) {
                case FileWatcherEventType::Exit:
                    return 0;
                case FileWatcherEventType::SetSong:
                    currentSong = event.song;
                    compileCurrentSong();
                    break;
                case FileWatcherEventType::RenderSong:
                    renderCurrentSong(event.renderSeconds);
                    break;
            }
        }

        DWORD changeBytes;
        if (!GetOverlappedResult(hDir, &overlapped, &changeBytes, FALSE)) {
            continue;
        }

        readChanges = true;

        bool hasSongChanged = false;

        // abomination
        for (const FILE_NOTIFY_INFORMATION* notification = (FILE_NOTIFY_INFORMATION*)changeBuffer; notification != nullptr; notification = (notification->NextEntryOffset == 0 ? nullptr : (FILE_NOTIFY_INFORMATION*)(((char*)notification) + notification->NextEntryOffset))) {
            const std::wstring filename(notification->FileName, notification->FileNameLength / 2);
            if (filename.length() < 2) {
                continue;
            }

            if (filename[filename.length() - 1] != 'c' || filename[filename.length() - 2] != '.') {
                continue;
            }

            if (filename.substr(0, filename.length() - 2) != currentSong) {
                continue;
            }

            hasSongChanged = true;
        }

        if (hasSongChanged) {
            compileCurrentSong();
        }
    }

    return 0;
}