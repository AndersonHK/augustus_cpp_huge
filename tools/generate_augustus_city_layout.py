"""Derive the reviewed city-record insertion offsets from the producer writers."""
import re
from generate_augustus_model_defaults import BASE, TARGET, ROOT, git


def layout(revision):
    source = git("show", f"{revision}:src/city/data.c")
    source = source[source.index("static void save_main_data("):source.index("static void load_main_data(")]
    source = re.sub(r"/\*.*?\*/|//[^\n]*", "", source, flags=re.S)
    lines = [line.strip() for line in source.splitlines() if line.strip()][2:-1]
    constants = {"RESOURCE_MAX": 22, "RESOURCE_MAX_FOOD": 6, "MAX_GODS": 5}

    def statements(index, nested=False):
        result = []
        while index < len(lines):
            line = lines[index]
            index += 1
            if line == "}":
                assert nested
                return index, result
            loop = re.fullmatch(r"for \(int i = 0; i < (\w+); i\+\+\) \{", line)
            if loop:
                count = constants.get(loop[1], int(loop[1]) if loop[1].isdigit() else None)
                assert count is not None, line
                index, body = statements(index, True)
                result.extend(body * count)
                continue
            write = re.fullmatch(r"buffer_write_[iu](8|16|32)\(main, (.+)\);", line)
            assert write, line
            result.append((write[2], int(write[1]) // 8))
        assert not nested
        return index, result

    _, fields = statements(0)
    offsets = {}
    size = 0
    for field, width in fields:
        offsets.setdefault(field, size)
        size += width
    return size, offsets


def generate():
    common_size, common = layout(BASE)
    current_size, current = layout(TARGET)
    immigration = current["city_data.migration.adjust_percentage_immigration"]
    religion = current["city_data.houses.missing.fourth_religion"]
    assert current_size == common_size + 16
    assert current["city_data.migration.adjust_percentage_emigration"] == immigration + 4
    assert current["city_data.houses.missing.fifth_religion"] == religion + 4
    for field, offset in common.items():
        expected = offset + (8 if offset >= immigration else 0) + (8 if offset >= religion - 8 else 0)
        assert current[field] == expected, field
    output = f'''// Generated from Augustus (GPL-3.0) by tools/generate_augustus_city_layout.py.
// Common producer {BASE}; target {TARGET}.
#pragma once
#include <cstddef>
namespace augustus_city_layout {{
inline constexpr size_t common_size = {common_size};
inline constexpr size_t current_size = {current_size};
inline constexpr size_t migration_percentages = {immigration};
inline constexpr size_t extra_religions = {religion};
}}
'''
    (ROOT / "src/game/augustus_city_layout.generated.h").write_text(output, encoding="utf-8")
    print(f"City writer: {common_size} common bytes; {current_size} current bytes; insertions {immigration}, {religion}.")


if __name__ == "__main__":
    generate()
