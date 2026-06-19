#include "list.h"

#include <cstdlib>
#include <vector>

static struct BuildingLists {
    std::vector<int> small;
    std::vector<int> large;
    std::vector<int> burning;
} data;

void building_list_small_clear(void)
{
    data.small.clear();
}

void building_list_small_add(int building_id)
{
    data.small.push_back(building_id);
}

int building_list_small_size(void)
{
    return static_cast<int>(data.small.size());
}

int building_list_small_item(int index)
{
    return data.small[index];
}

void building_list_large_clear(void)
{
    data.large.clear();
}

void building_list_large_add(int building_id)
{
    data.large.push_back(building_id);
}

int building_list_large_size(void)
{
    return static_cast<int>(data.large.size());
}

int building_list_large_item(int index)
{
    return data.large[index];
}

void building_list_burning_clear(void)
{
    data.burning.clear();
}

void building_list_burning_add(int building_id)
{
    data.burning.push_back(building_id);
}

int building_list_burning_size(void)
{
    return static_cast<int>(data.burning.size());
}

int building_list_burning_item(int index)
{
    return data.burning[index];
}

void building_list_save_state(buffer *small, buffer *large, buffer *burning, buffer *burning_totals)
{
    int buf_size = static_cast<int>(data.small.size() * sizeof(int32_t));
    if (buf_size) {
        uint8_t *buf_data = static_cast<uint8_t *>(malloc(buf_size));
        buffer_init(small, buf_data, buf_size);
        for (int value : data.small) {
            buffer_write_i32(small, value);
        }
    }

    buf_size = static_cast<int>(data.large.size() * sizeof(int32_t));
    if (buf_size) {
        uint8_t *buf_data = static_cast<uint8_t *>(malloc(buf_size));
        buffer_init(large, buf_data, buf_size);
        for (int value : data.large) {
            buffer_write_i32(large, value);
        }
    }

    buf_size = static_cast<int>(data.burning.size() * sizeof(int32_t));
    if (buf_size) {
        uint8_t *buf_data = static_cast<uint8_t *>(malloc(buf_size));
        buffer_init(burning, buf_data, buf_size);
        for (int value : data.burning) {
            buffer_write_i32(burning, value);
        }
    }

    buffer_write_i32(burning_totals, static_cast<int>(data.burning.size()));
}

void building_list_load_state(buffer *small, buffer *large, buffer *burning, buffer *burning_totals, int is_new_version)
{
    data.small.clear();
    data.large.clear();
    data.burning.clear();

    if (!is_new_version) {
        size_t size = small->size / sizeof(int16_t);
        for (size_t i = 0; i < size; i++) {
            building_list_small_add(buffer_read_i16(small));
        }
        size = large->size / sizeof(int16_t);
        for (size_t i = 0; i < size; i++) {
            building_list_large_add(buffer_read_i16(large));
        }
        size = burning->size / sizeof(int16_t);
        for (size_t i = 0; i < size; i++) {
            building_list_burning_add(buffer_read_i16(burning));
        }

        buffer_skip(burning_totals, 4);
    } else {
        size_t size = small->size / sizeof(int32_t);
        for (size_t i = 0; i < size; i++) {
            building_list_small_add(buffer_read_i32(small));
        }
        size = large->size / sizeof(int32_t);
        for (size_t i = 0; i < size; i++) {
            building_list_large_add(buffer_read_i32(large));
        }
        size = burning->size / sizeof(int32_t);
        for (size_t i = 0; i < size; i++) {
            building_list_burning_add(buffer_read_i32(burning));
        }
    }
    data.small.clear();
    data.large.clear();
    data.burning.resize(buffer_read_i32(burning_totals));
}
