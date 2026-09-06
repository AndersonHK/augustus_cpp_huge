#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

enum class ArchiveFamily { Unknown, SharedLegacy, Vespasian, Augustus };
enum class ArchiveIdentification { Invalid, Identified, Ambiguous };

struct ArchiveOrigin {
    ArchiveIdentification status = ArchiveIdentification::Invalid;
    ArchiveFamily family = ArchiveFamily::Unknown;
    int save_version = 0;
    int scenario_version = 0;
    int resource_version = 0;
    std::vector<std::string> layouts;
    std::string diagnostic;
};

// Examines immutable bytes before touching any city or runtime ID bridge. Names
// and extensions are deliberately absent: a caller's explicit origin is only
// accepted when that family's complete archive structure validates.
ArchiveOrigin game_file_io_identify_archive(const uint8_t *bytes, size_t length, ArchiveFamily explicit_origin = ArchiveFamily::Unknown);
