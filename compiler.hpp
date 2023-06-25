#pragma once

#include <string>
#include <vector>

int compileSongToWasm(const wchar_t* name, std::vector<std::string>& errors, std::vector<std::string>& messages);
int compileSongToDll();
int compileSongToHTML();