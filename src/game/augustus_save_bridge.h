#pragma once

#include "game/archive_origin.h"
#include <map>

// Serialized producer data, deliberately separate from native runtime objects.
// No source ordinal is usable as a native definition identity.
struct AugustusArchive {
    ArchiveOrigin origin;
    std::map<std::string, std::vector<uint8_t>> pieces;
};

bool game_file_io_decode_augustus_archive(const uint8_t *bytes, size_t length, AugustusArchive &archive, std::string &diagnostic);
