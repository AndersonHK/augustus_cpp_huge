#include "custom_media.h"

#include "core/log.h"

#include <deque>

static std::deque<custom_media_t> custom_media;

static int entry_in_use(const custom_media_t &entry)
{
    return entry.type != CUSTOM_MEDIA_UNDEFINED;
}

static custom_media_t new_entry(unsigned int position)
{
    custom_media_t entry = {};
    entry.id = position;
    return entry;
}

static custom_media_t *append_entry()
{
    custom_media.push_back(new_entry(static_cast<unsigned int>(custom_media.size())));
    return &custom_media.back();
}

static void reset_entries()
{
    custom_media.clear();
    append_entry();
}

static custom_media_t *create_entry()
{
    for (size_t i = 1; i < custom_media.size(); ++i) {
        if (!entry_in_use(custom_media[i])) {
            custom_media[i] = new_entry(static_cast<unsigned int>(i));
            return &custom_media[i];
        }
    }
    return append_entry();
}

void custom_media_clear(void)
{
    if (!custom_media.empty()) {
        for (custom_media_t &entry : custom_media) {
            message_media_text_blob_mark_entry_as_unused(entry.filename);
        }
    }

    reset_entries();
}

custom_media_t *custom_media_get(int media_id)
{
    return media_id < 0 || static_cast<size_t>(media_id) >= custom_media.size() ? nullptr : &custom_media[media_id];
}

custom_media_t *custom_media_create_blank(void)
{
    return create_entry();
}

custom_media_t *custom_media_create(custom_media_type type, const uint8_t *filename, custom_media_link_type link_type, int link_id)
{
    custom_media_t *entry = custom_media_create_blank();
    entry->type = type;
    entry->filename = message_media_text_blob_add(filename);
    entry->link_type = link_type;
    entry->link_id = link_id;

    return entry;
}

void custom_media_save_state(buffer *buf)
{
    uint32_t array_size = static_cast<uint32_t>(custom_media.size());
    uint32_t struct_size = (4 * sizeof(int32_t)) + (1 * sizeof(int16_t));
    buffer_init_dynamic_array(buf, array_size, struct_size);

    for (const custom_media_t &media_entry : custom_media) {
        const custom_media_t *entry = &media_entry;
        int entry_id = entry && entry->id ? entry->id : 0;
        buffer_write_i32(buf, entry_id);
        int entry_type = entry && entry->type ? entry->type : 0;
        buffer_write_i32(buf, entry_type);
        size_t entry_filename_id = entry && entry->filename && entry->filename->id ? entry->filename->id : 0;
        buffer_write_u32(buf, (unsigned int) entry_filename_id);
        uint16_t entry_link_type = static_cast<uint16_t>(entry && entry->link_type ? entry->link_type : 0);
        buffer_write_u16(buf, entry_link_type);
        size_t entry_link_id = entry && entry->link_id ? entry->link_id : 0;
        buffer_write_u32(buf, (unsigned int) entry_link_id);
    }
}

void custom_media_load_state_entry(buffer *buf, custom_media_t *entry, custom_media_link_type *link_type, int *link_id)
{
    entry->id = buffer_read_i32(buf);
    entry->type = static_cast<custom_media_type>(buffer_read_i32(buf));

    // Expects the media text blob to be loaded already.
    int linked_text_blob_id = buffer_read_i32(buf);
    entry->filename = message_media_text_blob_get_entry(linked_text_blob_id);
    entry->link_type = static_cast<custom_media_link_type>(buffer_read_i16(buf));
    entry->link_id = buffer_read_i32(buf);

    *link_type = entry->link_type;
    *link_id = entry->link_id;
}

int custom_media_relink_text_blob(size_t text_id, text_blob_string_t *new_text_link)
{
    for (custom_media_t &entry : custom_media) {
        if (entry.filename && entry.filename->id == text_id) {
            entry.filename = new_text_link;
            return 1;
        }
    }
    return 0;
}
