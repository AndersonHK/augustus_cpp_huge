#pragma once

#include "game/load_save/LoadSaveModuleAbi.h"

#include <cstdint>
#include <string>
#include <vector>

class LoadSaveModuleClient {
public:
    uint32_t readArchive(const char *path, uint64_t offset, std::vector<uint8_t> &archive, std::string &diagnostic) const;

#ifdef STARTUP_PARSER_TEST
    static bool moduleIsLoadedForTest();
#endif
};
