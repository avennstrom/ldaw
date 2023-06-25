#include "compiler.hpp"
#include "ldaw.hpp"

#include <fstream>

static int createWasmBatFile(const wchar_t* path, const wchar_t* songName)
{
    wchar_t exedir[MAX_PATH];
    getExecutableDirectory(exedir, sizeof(exedir));

    FILE* f = _wfopen(path, L"w");
    if (f == nullptr) {
        return 1;
    }

    const char* emsdk_env = "C:\\Users\\User\\emsdk\\emsdk_env.bat";

    fprintf(f, "@echo off\n");
    fprintf(f, "chcp 65001\n");
    fprintf(f, "call \"%s\"\n", emsdk_env);
    fprintf(f, "emcc -O3 --no-entry -I\"%ws\" -o ..\\%ws.wasm ..\\%ws.c\n", exedir, songName, songName);
    fclose(f);

    return 0;
}

int compileSongToWasm(const wchar_t* name, std::vector<std::string>& errors, std::vector<std::string>& messages)
{
    wchar_t intdir[MAX_PATH];
    getIntermediateDirectory(intdir, sizeof(intdir));

    wchar_t batpath[256];
    wsprintf(batpath, L"%ws\\%ws-wasm.bat", intdir, name);

    if (createWasmBatFile(batpath, name) != 0) {
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
    wsprintf(cmdline, L"/C \"%s\"", batpath);

    wchar_t cmd[MAX_PATH];
    DWORD r = GetEnvironmentVariable(TEXT("COMSPEC"), cmd, _countof(cmd));
    if (r == 0 || r > _countof(cmd)) {
        return 1;
    }

    if (!CreateProcess(cmd, cmdline, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, intdir, &si, &pi)) {
        errors.push_back("Internal error: failed to launch process.");
        return 1;
    }

    r = WaitForSingleObject(pi.hProcess, 10 * 1000);
    if (r != WAIT_OBJECT_0) {
        errors.push_back("Internal error: timeout while waiting for compiler.");
        return 1;
    }

    DWORD exitCode;
    if (!GetExitCodeProcess(pi.hProcess, &exitCode)) {
        errors.push_back("Internal error: failed to aquire compiler process exit code.");
        return 1;
    }

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    CloseHandle(hChildStdOutWrite);

    std::string output;
    output.reserve(4096);

    while (true) {
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
        errors.push_back(wstringToUtf8(batpath) + " failed with code " + std::to_string(exitCode));
        return 1;
    }

    messages.push_back("Export finished: " + wstringToUtf8(name) + ".wasm");

    return 0;
}
