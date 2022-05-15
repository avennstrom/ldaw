#pragma once

#include "event_queue.hpp"

#include <string>
#include <vector>

typedef struct SongInfo {
	uint64_t sampleRate;
} SongInfo;

static void fillDefaultSongInfo(SongInfo* info)
{
	info->sampleRate = 44100;
}

typedef void(*GetInfo)(SongInfo* info);
typedef void(*Play)(int16_t* samples, size_t sampleCount, uint64_t sampleRate, uint64_t baseSample);

struct Song
{
    HMODULE			hModule		= NULL;
	GetInfo			getInfo		= nullptr;
    Play			play		= nullptr;
	std::wstring	name;
	std::wstring	filename;
};

enum class UiEventType {
	Songs,
	SongStatus,
	RenderStatus,
	Playback,
	Volume,
};

struct UiEvent {
	UiEventType type;

	std::vector<std::wstring> songs;

	struct {
		bool playing;
		uint64_t seconds;
		uint64_t samples;
	} playback;

	std::optional<Song> song;
	std::wstring songName;
	bool isCompiling;
	std::vector<std::string> songMessages;
	std::vector<std::string> songErrors;

	bool isRendering;
	std::vector<std::string> renderMessages;
	std::vector<std::string> renderErrors;
};

typedef EventQueue<UiEvent> UiEventQueue;

std::wstring getExecutableDirectory();
std::wstring getWorkingDirectory();
std::wstring getIntermediateDirectory();

std::string wstringToUtf8(const std::wstring& s);