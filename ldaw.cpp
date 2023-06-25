#include "ldaw.hpp"
#include "watcher.hpp"
#include "streaming.hpp"

#include "simple_fft/fft.hpp"

#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl2.h"
#include "imgui/implot.h"

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include <math.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdio.h>
#include <dsound.h>
#include <Shlwapi.h>
#include <ShlObj.h>
#include <Windows.h>

#include <thread>
#include <fstream>
#include <unordered_map>
#include <unordered_set>
#include <queue>
#include <mutex>
#include <format>
#include <iostream>

#pragma comment(lib, "opengl32")
#pragma comment(lib, "dsound")
#pragma comment(lib, "dxguid")
#pragma comment(lib, "shlwapi")
#pragma comment(lib, "rpcrt4")

bool getExecutableDirectory(wchar_t* dir, size_t len) {
    if (GetModuleFileName(NULL, dir, MAX_PATH) == 0) {
        return false;
    }

    if (!PathRemoveFileSpec(dir)) {
        return false;
    }

    return true;
}

bool getWorkingDirectory(wchar_t* dir, size_t len) {
    GetCurrentDirectory(MAX_PATH, dir);
    return true;
}

bool getIntermediateDirectory(wchar_t* dir, size_t len) {
    if (!getWorkingDirectory(dir, len)) {
        return false;
    }

    wsprintf(dir, L"%s\\.ldaw", dir);
    return true;
}

std::string wstringToUtf8(const std::wstring& s) {
    const int len = WideCharToMultiByte(CP_UTF8, 0, s.c_str(), (int)s.length(), nullptr, 0, nullptr, nullptr);
    std::vector<char> buf(len);
    WideCharToMultiByte(CP_UTF8, 0, s.c_str(), (int)s.length(), buf.data(), len, nullptr, nullptr);
    return std::string(buf);
}

bool wstringToUtf8(char* target, size_t target_len, const wchar_t* source, size_t source_len) {
    const int len = WideCharToMultiByte(CP_UTF8, 0, source, (int)source_len, nullptr, 0, nullptr, nullptr);
    if (len > target_len) {
        return false;
    }
    WideCharToMultiByte(CP_UTF8, 0, source, (int)source_len, target, len, nullptr, nullptr);
    return true;
}

float waveform_getter(void* data, int index)
{
    return ((uint16_t*)data)[index] / (float)INT16_MAX + 1.0f;
}

static void doMenu(GLFWwindow* window, UiEventQueue* events, StreamingThreadContext* streaming, FileWatcherThreadContext* files)
{
    ImGuiIO& io = ImGui::GetIO();

    ImVector<ImWchar> ranges;
    ImFontGlyphRangesBuilder builder;
    builder.AddRanges(io.Fonts->GetGlyphRangesDefault());
    builder.AddRanges(io.Fonts->GetGlyphRangesJapanese());
    builder.AddRanges(io.Fonts->GetGlyphRangesCyrillic());
    builder.BuildRanges(&ranges);

    ImFontConfig fontConfig;
    fontConfig.GlyphRanges = ranges.Data;

    ImFont* font = io.Fonts->AddFontDefault(&fontConfig);
    io.Fonts->Build();

    ImPlot::CreateContext();


    std::wstring selectedSong;
    std::vector<std::wstring> songs;

    bool isPlaying = false;
    std::string currentSongName;
    uint64_t playbackSamples = 0;
    uint64_t playbackSeconds = 0;
    int volume = DSBVOLUME_MAX;

    int songStateGroupSize = 1;

    bool isCompiling = false;
    std::vector<std::string> compileMessages;
    std::vector<std::string> compileErrors;

    bool isExporting = false;
    std::vector<std::string> exportMessages;
    std::vector<std::string> exportErrors;

    int exportDurationInSeconds = 60;

    ExportFormat exportFormat = ExportFormat::Wav;
    const char* exportOutputLabels[] = { ".wav", ".wasm" };
    static_assert(_countof(exportOutputLabels) == (size_t)ExportFormat::Count);

    const auto skipPlayback = [&](int64_t seconds) {
        StreamingEvent event;
        event.type = StreamingEventType::PlaybackScan;
        event.scanOffsetInSeconds = seconds;
        streaming->queue.push(event);
    };

    while (!glfwWindowShouldClose(window)) {
        {
            UiEvent event;
            while (events->pop(event)) {
                switch (event.type) {
                    case UiEventType::Songs: {
                        const auto& data = event.data.songs;
                        songs = data.songs;
                        break;
                    }
                    case UiEventType::SongStatus: {
                        const auto& data = event.data.songStatus;
                        isCompiling = data.isCompiling;
                        compileMessages = data.songMessages;
                        compileErrors = data.songErrors;
                        if (data.song.has_value()) {
                            currentSongName = wstringToUtf8(data.songName);

                            StreamingEvent outEvent;
                            outEvent.type = StreamingEventType::SetSong;
                            outEvent.song = data.song.value();
                            streaming->queue.push(outEvent);
                        }
                        break;
                    }
                    case UiEventType::ExportStatus: {
                        const auto& data = event.data.exportStatus;
                        isExporting = data.isExporting;
                        exportMessages = data.exportMessages;
                        exportErrors = data.exportErrors;
                        break;
                    }
                    case UiEventType::Playback: {
                        const auto& data = event.data.playback;
                        isPlaying = data.playing;
                        playbackSamples = data.samples;
                        playbackSeconds = data.seconds;
                        break;
                    }
                }
            }
        }

        glfwPollEvents();

        
        ImGui_ImplOpenGL2_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::ShowDemoWindow();
        ImPlot::ShowDemoWindow();

        if (ImGui::Begin("Songs"))
        {
            if (ImGui::BeginListBox("#Songs", ImVec2(-FLT_MIN, 5 * ImGui::GetTextLineHeightWithSpacing()))) {
                for (const std::wstring& song : songs) {
                    const bool selected = (song == selectedSong);

                    const std::string utf8 = wstringToUtf8(song);
                    ImGui::PushFont(font);
                    if (ImGui::Selectable(utf8.c_str(), selected)) {
                        selectedSong = song;
                    }
                    ImGui::PopFont();

                    if (selected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }

                ImGui::EndListBox();
            }

            ImGui::BeginDisabled(selectedSong.empty() || isCompiling);
            if (ImGui::Button("Play")) {
                isCompiling = true;
                FileWatcherEvent event;
                event.type = FileWatcherEventType::SetSong;
                event.data.setSong.song = selectedSong;
                files->queue.push(event);
            }
            ImGui::EndDisabled();
            
            if (ImGui::SliderInt("Volume", &volume, DSBVOLUME_MIN, DSBVOLUME_MAX, "%d ddB")) {
                StreamingEvent event;
                event.type = StreamingEventType::SetVolume;
                event.volume = volume;
                streaming->queue.push(event);
            }

            ImGui::BeginDisabled(!isPlaying || isCompiling);
            if (ImGui::Button("<<")) { skipPlayback(INT64_MIN); } // bold assumption
            ImGui::SameLine();
            if (ImGui::Button("< 1m")) { skipPlayback(-60); }
            ImGui::SameLine();
            if (ImGui::Button("< 10s")) { skipPlayback(-10); }
            ImGui::SameLine();
            if (ImGui::Button("< 1s")) { skipPlayback(-1); }
            ImGui::SameLine();
            if (ImGui::Button("1s >")) { skipPlayback(1); }
            ImGui::SameLine();
            if (ImGui::Button("10s >")) { skipPlayback(10); }
            ImGui::SameLine();
            if (ImGui::Button("1m >")) { skipPlayback(60); }
            ImGui::EndDisabled();

            if (isPlaying) {
                const uint64_t playbackMinutes = playbackSeconds / 60;
                const uint64_t playbackHours = playbackMinutes / 60;
                ImGui::Text("Now playing '%s'", currentSongName.c_str());
                ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "%02llu:%02llu:%02llu", playbackHours, playbackMinutes % 60, playbackSeconds % 60);
            }
            else {
                ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 0.5f), "Playback stopped.");
            }
        }
        ImGui::End();

        if (ImGui::Begin("Build")) {
            if (isCompiling) {
                ImGui::Text("Compiling...");
            }
            else {
                if (!compileErrors.empty()) {
                    ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Build failed with %llu error(s):", compileErrors.size());
                }
                else {
                    ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Build succeeded!");
                }
            }
            ImGui::Separator();

            for (const std::string& msg : compileMessages) {
                ImGui::TextWrapped(msg.c_str());
                ImGui::Separator();
            }

            for (const std::string& err : compileErrors) {
                ImGui::TextWrapped(err.c_str());
                ImGui::Separator();
            }
        }
        ImGui::End();

        if (ImGui::Begin("Export")) {
            ImGui::Combo("Format", (int*)&exportFormat, exportOutputLabels, _countof(exportOutputLabels));
            if (exportFormat == ExportFormat::Wav) {
                ImGui::InputInt("Seconds", &exportDurationInSeconds);
            }
            ImGui::Text("Export '%s'", currentSongName.c_str());
            if (ImGui::Button("Start")) {
                FileWatcherEvent event;
                event.type = FileWatcherEventType::ExportSong;
                event.data.exportSong.durationInSeconds = exportDurationInSeconds;
                event.data.exportSong.format = exportFormat;
                files->queue.push(event);
            }

            if (isExporting) {
                ImGui::Text("Exporting...");
            }

            for (const std::string& msg : exportMessages) {
                ImGui::TextWrapped(msg.c_str());
                ImGui::Separator();
            }

            for (const std::string& err : exportErrors) {
                ImGui::TextWrapped(err.c_str());
                ImGui::Separator();
            }
        }
        ImGui::End();

        ImGui::Begin("State");
        {
            ImGui::SliderInt("Group size", &songStateGroupSize, 0, 16);

            std::string hex;
            for (size_t i = 0; i < streaming->stateCapture.size(); ++i) {
                const uint8_t lo = streaming->stateCapture[i] & 0xf;
                const uint8_t hi = streaming->stateCapture[i] >> 4;

                hex += (hi >= 10) ? ('a' + (hi - 10)) : ('0' + hi);
                hex += (lo >= 10) ? ('a' + (lo - 10)) : ('0' + lo);

                if (i % songStateGroupSize == (songStateGroupSize - 1)) {
                    hex += '\n';
                }
            }

            ImGui::Text("%s", hex.c_str());
        }
        ImGui::End();

        ImGui::Begin("Waveform");
        {
            if (ImPlot::BeginPlot("Waveform", ImVec2(-1, -1), ImPlotFlags_NoFrame | ImPlotFlags_CanvasOnly | ImPlotFlags_AntiAliased)) {
                if (!streaming->waveformCapture.empty()) {
                    ImPlot::SetupAxis(ImAxis_X1, NULL, ImPlotAxisFlags_NoDecorations);
                    ImPlot::SetupAxis(ImAxis_Y1, NULL, ImPlotAxisFlags_NoDecorations);
                    ImPlot::SetupAxisLimits(ImAxis_X1, 0.0, 2048.0, ImPlotCond_Always);
                    ImPlot::SetupAxisLimits(ImAxis_Y1, (double)INT16_MIN, (double)INT16_MAX, ImPlotCond_Always);
                    ImPlot::PlotLine("Waveform", streaming->waveformCapture.data(), 2048);
                }
                ImPlot::EndPlot();
            }
        }
        ImGui::End();

        ImGui::Begin("Spectrum");
        {
            if (ImPlot::BeginPlot("Spectrum", ImVec2(-1, -1), ImPlotFlags_NoFrame | ImPlotFlags_CanvasOnly | ImPlotFlags_AntiAliased)) {
                if (!streaming->waveformCapture.empty()) {

                    const char* error = nullptr;

                    std::vector<real_type> real(2048);
                    for (size_t i = 0; i < 2048; ++i) {
                        real[i] = (float)streaming->waveformCapture[i];
                    }

                    std::vector<complex_type> complex(2048);
                    if (simple_fft::FFT(real, complex, 2048, error)) {
                        ImPlot::SetupAxis(ImAxis_X1, NULL, ImPlotAxisFlags_NoDecorations);
                        ImPlot::SetupAxis(ImAxis_Y1, NULL, ImPlotAxisFlags_NoDecorations);
                        ImPlot::SetupAxisLimits(ImAxis_X1, 0.0, 2048.0, ImPlotCond_Always);
                        ImPlot::SetupAxisLimits(ImAxis_Y1, (double)INT16_MIN, (double)INT16_MAX, ImPlotCond_Always);

                        std::vector<real_type> spectrum(2048);
                        for (size_t i = 0; i < 2048; ++i) {
                            spectrum[i] = complex[i].real();
                        }
                        ImPlot::PlotBars("Spectrum", spectrum.data(), 2048);
                    }
                }
                ImPlot::EndPlot();
            }
        }
        ImGui::End();

        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.45f, 0.55f, 0.60f, 1.00f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());
        glfwMakeContextCurrent(window);
        glfwSwapBuffers(window);
    }
}

INT WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, INT nCmdShow)
{
    AllocConsole();

    FILE* f;
    freopen_s(&f, "CONIN$", "r", stdin);
    freopen_s(&f, "CONOUT$", "w", stderr);
    freopen_s(&f, "CONOUT$", "w", stdout);

    if (!glfwInit()) {
        return 1;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    GLFWwindow* window = glfwCreateWindow(750, 720, "ldaw", NULL, NULL);
    if (window == NULL) {
        return 1;
    }

    const HWND hWnd = glfwGetWin32Window(window);

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL2_Init();

    wchar_t intdir[MAX_PATH];
    getIntermediateDirectory(intdir, _countof(intdir));

    if (CreateDirectory(intdir, NULL)) {
        SetFileAttributes(intdir, FILE_ATTRIBUTE_HIDDEN);
    }

    wchar_t homePath[MAX_PATH + 1];
    SHGetSpecialFolderPath(hWnd, homePath, CSIDL_LOCAL_APPDATA, FALSE);



    UiEventQueue events;

    StreamingThreadContext streamingContext;
    streamingContext.feedback = &events;
    streamingContext.hWnd = hWnd;
    streamingContext.sampleRate = 44100;

    FileWatcherThreadContext fileWatcherContext;
    fileWatcherContext.feedback = &events;

    std::thread streamingThread(streamingThreadEntry, std::ref(streamingContext));
    std::thread fileWatcherThread(fileWatcherThreadEntry, std::ref(fileWatcherContext));
    
    doMenu(window, &events, &streamingContext, &fileWatcherContext);
    
    streamingContext.queue.push(StreamingEvent{ StreamingEventType::Exit });
    fileWatcherContext.queue.push(FileWatcherEvent{ FileWatcherEventType::Exit });

    streamingThread.join();
    fileWatcherThread.join();

    return 0;
}
