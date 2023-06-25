#pragma once

#include "event_queue.hpp"

#include <string>
#include <vector>

#include <ldaw.h>

inline void defaultSongInfo(ldaw_song_info_t* info)
{
	info->state_size	= 0;
	info->sample_rate	= 44100;
	info->entropy_size	= 0;
}

typedef void(*Info)(ldaw_song_info_t* info);
typedef void(*Init)(void* state, const void* entropy);
typedef void(*Play)(int16_t* samples, size_t sampleCount, uint64_t sampleRate, uint64_t baseSample, void* state);

struct Song
{
	uint8_t			checksum[16];
	HMODULE			hModule		= NULL;
	Info			info		= nullptr;
	Init			init		= nullptr;
	Play			play		= nullptr;
	std::wstring	name;
	std::wstring	filename;
};

enum class UiEventType {
	Songs,
	SongStatus,
	ExportStatus,
	Playback,
	Volume,
};

struct UiEventData_Songs {
	std::vector<std::wstring> songs;
};

struct UiEventData_SongStatus {
	std::optional<Song> song;
	std::wstring songName;
	bool isCompiling;
	std::vector<std::string> songMessages;
	std::vector<std::string> songErrors;
};

struct UiEventData_ExportStatus {
	bool isExporting;
	std::vector<std::string> exportMessages;
	std::vector<std::string> exportErrors;
};

struct UiEventData_Playback {
	bool playing;
	uint64_t seconds;
	uint64_t samples;
};

struct UiEvent {
	UiEventType type;
	struct {
		UiEventData_Songs        songs;
		UiEventData_SongStatus   songStatus;
		UiEventData_Playback     playback;
		UiEventData_ExportStatus exportStatus;
	} data;
};

typedef EventQueue<UiEvent> UiEventQueue;

bool getExecutableDirectory(wchar_t* dir, size_t len);
bool getWorkingDirectory(wchar_t* dir, size_t len);
bool getIntermediateDirectory(wchar_t* dir, size_t len);

std::string wstringToUtf8(const std::wstring& s);
bool wstringToUtf8(char* target, size_t target_len, const wchar_t* source, size_t source_len);