#pragma once
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <commctrl.h>
#include <functional>
#include "game/mod_content.h"

namespace native_options {
std::wstring wide(const std::string &text);
class WindowScale {
public:
    WindowScale() = default;
    WindowScale(const WindowScale &) = delete;
    WindowScale &operator=(const WindowScale &) = delete;
    ~WindowScale() { if (font_) DeleteObject(font_); }
    int pixels(int value) const { return MulDiv(value, dpi_, 96); }
    UINT dpi() const { return dpi_; }
    void apply(HWND window, UINT dpi = 0);
    void client_size(HWND window, int width, int height) const;
private:
    UINT dpi_ = 96;
    HFONT font_ = nullptr;
};
struct Row {
    std::string group, name, category, detail;
    bool boolean = true, enabled = true;
    int value = 0, minimum = 0, maximum = 1;
    std::function<void(int)> apply;
    int step = 1;
};
class Panel {
public:
    HWND create(HWND parent, int id);
    void set_rows(std::vector<Row> rows);
    LRESULT notify(NMHDR *notification);
    void edit_selected();
    HWND handle() const { return window_; }
    HWND setting_list() const { return list_; }
    int group_count() const { return static_cast<int>(groups_.size()); }
    void select_section(const std::string &group, const std::string &page, const std::string &category);
    void scale(UINT dpi);
private:
    static LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM wp, LPARAM lp);
    void rebuild_navigation();
    void rebuild_list();
    void show_selected();
    void layout();
    void apply_value(int value);
    void apply_editor();
    HWND window_ = nullptr, groups_tab_ = nullptr, pages_tab_ = nullptr, categories_list_ = nullptr;
    HWND label_ = nullptr, toggle_ = nullptr, slider_ = nullptr, value_ = nullptr, apply_ = nullptr, detail_ = nullptr;
    HWND list_ = nullptr;
    UINT dpi_ = 96;
    bool rebuilding_ = false;
    int selected_ = -1;
    std::string group_, page_, category_;
    std::vector<std::string> groups_, pages_, categories_;
    std::vector<int> visible_;
    std::vector<Row> rows_;
};
void append_mod_rows(std::vector<Row> &rows, const mod_content::Session &session, std::function<void(const std::string &, int)> apply);
void append_hardcoded_rows(std::vector<Row> &rows, const std::function<int(const char *, int)> &get, const std::function<void(const char *, int)> &set);
void validate_number_dialog_layout(HWND owner, const std::function<void(HWND, UINT)> &capture);
void show_dialog(HWND owner, const std::function<std::vector<Row>()> &rows);
}
#endif
