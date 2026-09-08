#pragma once

#include <cstddef>
#include <cstdint>

// Optional synchronous observer. Loaders remain usable in tools and extractor
// DLLs without linking the game's renderer or touching partially loaded state.
namespace loading_progress {
using Observer = void (*)(const char *label, std::size_t completed, std::size_t total);
using ThemeObserver = void (*)(const char *filename);
using StageObserver = void (*)(const char *id);
inline Observer observer = nullptr;
inline ThemeObserver theme_observer = nullptr;
inline StageObserver stage_observer = nullptr;
struct FontGlyph {
    int id, width, height, advance, x_offset, y_offset;
    const std::uint32_t *pixels;
    int row_width;
};
using FontGlyphObserver = void (*)(const FontGlyph &glyph);
inline FontGlyphObserver font_glyph_observer = nullptr;
inline void (*font_ready_observer)() = nullptr;
inline void report(const char *label, std::size_t completed = 0, std::size_t total = 0) { if (observer) observer(label, completed, total); }
inline void theme(const char *filename) { if (theme_observer) theme_observer(filename); }
inline void stage(const char *id) { if (stage_observer) stage_observer(id); }
}
