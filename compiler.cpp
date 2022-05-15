#include "compiler.hpp"
#include "ldaw.hpp"

#include <fstream>

static int createWasmBatFile(std::wstring name)
{
    const std::wstring path = getIntermediateDirectory() + L'\\' + name + L".bat";
    std::ofstream f(path);



    return 0;
}

int compileSongToWasm(std::wstring name)
{
    const std::wstring bat = name + L"-wasm";

    if (createWasmBatFile(name) != 0) {
        return 1;
    }



    return 0;
}
