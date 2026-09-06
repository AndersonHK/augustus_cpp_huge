#ifdef _WIN32
#include "platform/mod_options_win32.h"
#include <algorithm>
#include "core/config_options.h"
#include <stdexcept>
#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

namespace native_options {
std::wstring wide(const std::string &text)
{
    if (text.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), nullptr, 0);
    if (!n) throw std::runtime_error("Invalid UTF-8 text");
    std::wstring result(n, 0); MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), n); return result;
}
void WindowScale::apply(HWND window, UINT dpi)
{
    dpi_ = dpi ? dpi : GetDpiForWindow(window);
    if (!dpi_) dpi_ = 96;
    NONCLIENTMETRICSW metrics{}; metrics.cbSize = sizeof(metrics);
    LOGFONTW font{};
    if (SystemParametersInfoForDpi(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0, dpi_)) font = metrics.lfMessageFont;
    else { font.lfHeight = -MulDiv(9, dpi_, 72); wcscpy_s(font.lfFaceName, L"Segoe UI"); }
    HFONT replacement = CreateFontIndirectW(&font);
    if (!replacement) return;
    SendMessageW(window, WM_SETFONT, reinterpret_cast<WPARAM>(replacement), TRUE);
    EnumChildWindows(window, [](HWND child, LPARAM font) -> BOOL { SendMessageW(child, WM_SETFONT, font, TRUE); return TRUE; }, reinterpret_cast<LPARAM>(replacement));
    if (font_) DeleteObject(font_);
    font_ = replacement;
}
void WindowScale::client_size(HWND window, int width, int height) const
{
    RECT rect{0, 0, pixels(width), pixels(height)};
    AdjustWindowRectExForDpi(&rect, static_cast<DWORD>(GetWindowLongPtrW(window, GWL_STYLE)), FALSE, static_cast<DWORD>(GetWindowLongPtrW(window, GWL_EXSTYLE)), dpi_);
    RECT position{}; GetWindowRect(window, &position);
    MONITORINFO monitor{}; monitor.cbSize = sizeof(monitor); GetMonitorInfoW(MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST), &monitor);
    const int outer_width = std::min<int>(rect.right - rect.left, monitor.rcWork.right - monitor.rcWork.left);
    const int outer_height = std::min<int>(rect.bottom - rect.top, monitor.rcWork.bottom - monitor.rcWork.top);
    SetWindowPos(window, nullptr, std::clamp<int>(position.left, monitor.rcWork.left, monitor.rcWork.right - outer_width), std::clamp<int>(position.top, monitor.rcWork.top, monitor.rcWork.bottom - outer_height), outer_width, outer_height, SWP_NOZORDER | SWP_NOACTIVATE);
}
namespace {
int64_t value_steps(const Row &row)
{
    return (static_cast<int64_t>(row.maximum) - row.minimum) / std::max(1, row.step);
}
int slider_steps(const Row &row)
{
    return static_cast<int>(std::min<int64_t>(10000, value_steps(row)));
}
int slider_value(const Row &row, int position)
{
    const int positions = slider_steps(row);
    const int64_t step = positions ? (static_cast<int64_t>(position) * value_steps(row) + positions / 2) / positions : 0;
    return static_cast<int>(row.minimum + step * row.step);
}
HWND control(HWND parent, const wchar_t *type, const wchar_t *text, DWORD style, int x, int y, int w, int h, int id)
{
    HWND out = CreateWindowExW(0, type, text, WS_CHILD | WS_VISIBLE | style, x, y, w, h, parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), GetModuleHandleW(nullptr), nullptr);
    SendMessageW(out, WM_SETFONT, reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), TRUE); return out;
}
struct NumberDialog { const Row *row; HWND edit = nullptr; bool accepted = false; int value = 0; WindowScale scale; };
void number_layout(HWND window, NumberDialog &state)
{
    auto p = [&](int value) { return state.scale.pixels(value); };
    RECT client{}; GetClientRect(window, &client);
    MoveWindow(GetDlgItem(window, 11), p(16), p(16), client.right - p(32), p(36), TRUE);
    MoveWindow(state.edit, p(16), p(56), client.right - p(32), p(28), TRUE);
    MoveWindow(GetDlgItem(window, IDOK), client.right - p(222), client.bottom - p(44), p(96), p(28), TRUE);
    MoveWindow(GetDlgItem(window, IDCANCEL), client.right - p(112), client.bottom - p(44), p(96), p(28), TRUE);
}
LRESULT CALLBACK number_proc(HWND window, UINT msg, WPARAM wp, LPARAM lp)
{
    auto *state = reinterpret_cast<NumberDialog *>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (msg == WM_CREATE) {
        state = static_cast<NumberDialog *>(reinterpret_cast<CREATESTRUCT *>(lp)->lpCreateParams); SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
        auto label = wide(state->row->name + " (" + std::to_string(state->row->minimum) + " to " + std::to_string(state->row->maximum) + ")" + (state->row->step > 1 ? " - increment " + std::to_string(state->row->step) : ""));
        control(window, L"STATIC", label.c_str(), 0, 16, 16, 440, 30, 11);
        state->edit = control(window, L"EDIT", std::to_wstring(state->row->value).c_str(), WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL, 16, 52, 440, 26, 10);
        SendMessageW(state->edit, EM_SETLIMITTEXT, 12, 0); SendMessageW(state->edit, EM_SETSEL, 0, -1);
        control(window, L"BUTTON", L"Apply", WS_TABSTOP | BS_DEFPUSHBUTTON, 250, 96, 96, 28, IDOK);
        control(window, L"BUTTON", L"Cancel", WS_TABSTOP, 360, 96, 96, 28, IDCANCEL); state->scale.apply(window); SetFocus(state->edit); return 0;
    }
    if (msg == WM_SIZE && state) { number_layout(window, *state); return 0; }
    if (msg == WM_DPICHANGED && state) {
        state->scale.apply(window, HIWORD(wp));
        const RECT &rect = *reinterpret_cast<RECT *>(lp);
        SetWindowPos(window, nullptr, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, SWP_NOZORDER | SWP_NOACTIVATE);
        number_layout(window, *state); return 0;
    }
    if (msg == WM_COMMAND && LOWORD(wp) == IDOK) {
        wchar_t text[32]; GetWindowTextW(state->edit, text, 32); wchar_t *end = nullptr;
        errno = 0; long long value = wcstoll(text, &end, 10);
        if (errno || end == text || *end || value < state->row->minimum || value > state->row->maximum || (value - state->row->minimum) % state->row->step) { MessageBoxW(window, L"Enter an integer within the displayed bounds and increment.", L"Invalid value", MB_OK | MB_ICONWARNING); return 0; }
        state->value = static_cast<int>(value); state->accepted = true; DestroyWindow(window); return 0;
    }
    if ((msg == WM_COMMAND && LOWORD(wp) == IDCANCEL) || msg == WM_CLOSE) { DestroyWindow(window); return 0; }
    return DefWindowProcW(window, msg, wp, lp);
}
void register_class(const wchar_t *name, WNDPROC proc)
{
    WNDCLASSW wc{}; wc.lpfnWndProc = proc; wc.hInstance = GetModuleHandleW(nullptr); wc.lpszClassName = name; wc.hCursor = LoadCursor(nullptr, IDC_ARROW); wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1); RegisterClassW(&wc);
}
void modal_loop(HWND owner, HWND dialog)
{
    EnableWindow(owner, FALSE); ShowWindow(dialog, SW_SHOW); MSG msg{};
    while (IsWindow(dialog) && GetMessageW(&msg, nullptr, 0, 0) > 0) { if (!IsDialogMessageW(dialog, &msg)) { TranslateMessage(&msg); DispatchMessageW(&msg); } }
    EnableWindow(owner, TRUE); SetActiveWindow(owner);
}
}
void validate_number_dialog_layout(HWND owner, const std::function<void(HWND, UINT)> &capture)
{
    register_class(L"VespasianSettingNumber", number_proc);
    Row row{"Example mod", "Retirement age", "Difficulty", "", false, true, 50, 40, 90};
    for (UINT dpi : {96u, 144u, 192u}) {
        NumberDialog state{&row};
        HWND dialog = CreateWindowExW(WS_EX_DLGMODALFRAME, L"VespasianSettingNumber", L"Mod setting", WS_CAPTION | WS_SYSMENU, 50, 50, 490, 180, owner, nullptr, GetModuleHandleW(nullptr), &state);
        state.scale.apply(dialog, dpi); state.scale.client_size(dialog, 472, 144); number_layout(dialog, state);
        RECT client{}; GetClientRect(dialog, &client);
        for (int id : {11, 10, IDOK, IDCANCEL}) {
            RECT child{}; GetWindowRect(GetDlgItem(dialog, id), &child); MapWindowPoints(nullptr, dialog, reinterpret_cast<POINT *>(&child), 2);
            if (child.left < 0 || child.top < 0 || child.right > client.right || child.bottom > client.bottom) throw std::runtime_error("DPI-scaled number dialog clips a control");
        }
        ShowWindow(dialog, SW_SHOWNOACTIVATE); UpdateWindow(dialog); capture(dialog, dpi);
        SetWindowTextW(state.edit, L"75"); SendMessageW(dialog, WM_COMMAND, IDOK, 0);
        if (!state.accepted || state.value != 75) throw std::runtime_error("DPI-scaled number dialog cannot apply a value");
    }
}
namespace {
enum { OPTIONS_GROUP = 30, OPTIONS_PAGE, OPTIONS_CATEGORY, OPTIONS_LIST, OPTIONS_TOGGLE, OPTIONS_SLIDER, OPTIONS_VALUE, OPTIONS_APPLY };
std::pair<std::string, std::string> section(const Row &row)
{
    const auto slash = row.category.find(" / ");
    if (slash == std::string::npos) return {row.category.empty() ? "Options" : row.category, "General"};
    return {row.category.substr(0, slash), row.category.substr(slash + 3)};
}
void unique_name(std::vector<std::string> &names, const std::string &name)
{
    if (std::find(names.begin(), names.end(), name) == names.end()) names.push_back(name);
}
void choose_name(std::string &selected, const std::vector<std::string> &names)
{
    if (std::find(names.begin(), names.end(), selected) == names.end()) selected = names.empty() ? "" : names.front();
}
void populate_tabs(HWND tabs, const std::vector<std::string> &names, const std::string &selected)
{
    TabCtrl_DeleteAllItems(tabs);
    for (int i = 0; i < names.size(); ++i) {
        auto title = wide(names[i]); TCITEMW item{}; item.mask = TCIF_TEXT; item.pszText = title.data();
        SendMessageW(tabs, TCM_INSERTITEMW, i, reinterpret_cast<LPARAM>(&item));
        if (names[i] == selected) TabCtrl_SetCurSel(tabs, i);
    }
}
}
HWND Panel::create(HWND parent, int id)
{
    register_class(L"VespasianOptionsPanel", window_proc);
    window_ = CreateWindowExW(WS_EX_CONTROLPARENT, L"VespasianOptionsPanel", L"", WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN, 0, 0, 600, 400, parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), GetModuleHandleW(nullptr), this);
    return window_;
}
LRESULT CALLBACK Panel::window_proc(HWND window, UINT message, WPARAM wp, LPARAM lp)
{
    auto *panel = reinterpret_cast<Panel *>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_CREATE) {
        panel = static_cast<Panel *>(reinterpret_cast<CREATESTRUCT *>(lp)->lpCreateParams);
        panel->window_ = window; SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(panel));
        panel->groups_tab_ = control(window, WC_TABCONTROLW, L"", WS_TABSTOP, 0, 0, 100, 30, OPTIONS_GROUP);
        panel->pages_tab_ = control(window, WC_TABCONTROLW, L"", WS_TABSTOP, 0, 0, 100, 30, OPTIONS_PAGE);
        panel->categories_list_ = control(window, L"LISTBOX", L"", WS_TABSTOP | WS_BORDER | WS_VSCROLL | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT, 0, 0, 100, 100, OPTIONS_CATEGORY);
        panel->list_ = control(window, WC_LISTVIEWW, L"", WS_TABSTOP | WS_BORDER | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS, 0, 0, 300, 200, OPTIONS_LIST);
        ListView_SetExtendedListViewStyle(panel->list_, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_LABELTIP);
        for (int i = 0; i < 2; ++i) { LVCOLUMNW column{}; column.mask = LVCF_TEXT; column.pszText = const_cast<wchar_t *>(i ? L"Value" : L"Setting"); SendMessageW(panel->list_, LVM_INSERTCOLUMNW, i, reinterpret_cast<LPARAM>(&column)); }
        panel->label_ = control(window, L"STATIC", L"", 0, 0, 0, 300, 32, 0);
        panel->toggle_ = control(window, L"BUTTON", L"Enabled", WS_TABSTOP | BS_AUTOCHECKBOX, 0, 0, 150, 28, OPTIONS_TOGGLE);
        panel->slider_ = control(window, TRACKBAR_CLASSW, L"", WS_TABSTOP | TBS_HORZ | TBS_NOTICKS, 0, 0, 200, 28, OPTIONS_SLIDER);
        panel->value_ = control(window, L"EDIT", L"", WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL, 0, 0, 80, 28, OPTIONS_VALUE);
        panel->apply_ = control(window, L"BUTTON", L"Apply", WS_TABSTOP, 0, 0, 80, 28, OPTIONS_APPLY);
        panel->detail_ = control(window, L"STATIC", L"", 0, 0, 0, 300, 60, 0);
        panel->scale(GetDpiForWindow(window)); return 0;
    }
    if (!panel) return DefWindowProcW(window, message, wp, lp);
    if (message == WM_SIZE) { panel->layout(); return 0; }
    if (message == WM_NOTIFY && !panel->rebuilding_) return panel->notify(reinterpret_cast<NMHDR *>(lp));
    if (message == WM_COMMAND && !panel->rebuilding_) {
        if (LOWORD(wp) == OPTIONS_CATEGORY && HIWORD(wp) == LBN_SELCHANGE) {
            const int selected = static_cast<int>(SendMessageW(panel->categories_list_, LB_GETCURSEL, 0, 0));
            if (selected >= 0 && selected < panel->categories_.size()) { panel->category_ = panel->categories_[selected]; panel->rebuild_list(); }
        } else if (LOWORD(wp) == OPTIONS_TOGGLE) panel->apply_value(SendMessageW(panel->toggle_, BM_GETCHECK, 0, 0) == BST_CHECKED);
        else if (LOWORD(wp) == OPTIONS_APPLY) panel->apply_editor();
        return 0;
    }
    if (message == WM_HSCROLL && reinterpret_cast<HWND>(lp) == panel->slider_ && panel->selected_ >= 0) {
        const Row &row = panel->rows_[panel->selected_];
        const int value = slider_value(row, static_cast<int>(SendMessageW(panel->slider_, TBM_GETPOS, 0, 0)));
        SetWindowTextW(panel->value_, std::to_wstring(value).c_str());
        if (LOWORD(wp) == TB_ENDTRACK) panel->apply_value(value);
        return 0;
    }
    return DefWindowProcW(window, message, wp, lp);
}
void Panel::scale(UINT dpi)
{
    dpi_ = dpi ? dpi : 96; layout();
}
void Panel::layout()
{
    if (!window_ || !list_) return;
    auto p = [&](int value) { return MulDiv(value, dpi_, 96); };
    RECT rect{}; GetClientRect(window_, &rect);
    const int width = rect.right, height = rect.bottom;
    const int side = std::min(p(180), width / 3), right = side + p(12), body = p(76);
    const int editor = std::max(body + p(70), height - p(156));
    MoveWindow(groups_tab_, 0, 0, width, p(32), TRUE);
    MoveWindow(pages_tab_, 0, p(38), width, p(32), TRUE);
    MoveWindow(categories_list_, 0, body, side, std::max(p(30), height - body), TRUE);
    MoveWindow(list_, right, body, std::max(p(100), width - right), std::max(p(60), editor - body - p(8)), TRUE);
    ListView_SetColumnWidth(list_, 0, std::max(p(120), width - right - p(114)));
    ListView_SetColumnWidth(list_, 1, p(90));
    MoveWindow(label_, right, editor, width - right, p(36), TRUE);
    MoveWindow(toggle_, right, editor + p(38), width - right, p(28), TRUE);
    MoveWindow(slider_, right, editor + p(38), std::max(p(60), width - right - p(178)), p(28), TRUE);
    MoveWindow(value_, width - p(166), editor + p(38), p(78), p(28), TRUE);
    MoveWindow(apply_, width - p(80), editor + p(38), p(80), p(28), TRUE);
    MoveWindow(detail_, right, editor + p(76), width - right, std::max(p(28), height - editor - p(76)), TRUE);
}
void Panel::set_rows(std::vector<Row> rows)
{
    std::string selected_name;
    if (selected_ >= 0 && selected_ < rows_.size()) selected_name = rows_[selected_].group + "\n" + rows_[selected_].category + "\n" + rows_[selected_].name;
    rows_ = std::move(rows); selected_ = -1;
    for (int i = 0; i < rows_.size(); ++i) if (rows_[i].group + "\n" + rows_[i].category + "\n" + rows_[i].name == selected_name) { selected_ = i; break; }
    rebuild_navigation();
}
void Panel::select_section(const std::string &group, const std::string &page, const std::string &category)
{
    group_ = group; page_ = page; category_ = category; rebuild_navigation();
}
void Panel::rebuild_navigation()
{
    rebuilding_ = true; groups_.clear(); pages_.clear(); categories_.clear();
    for (const auto &row : rows_) unique_name(groups_, row.group);
    choose_name(group_, groups_);
    for (const auto &row : rows_) if (row.group == group_) unique_name(pages_, section(row).first);
    const std::vector<std::string> order = {"General Settings", "User Interface", "Difficulty", "City Management"};
    if (group_ == "Hardcoded settings") std::stable_sort(pages_.begin(), pages_.end(), [&](const auto &a, const auto &b) { return std::find(order.begin(), order.end(), a) < std::find(order.begin(), order.end(), b); });
    choose_name(page_, pages_);
    for (const auto &row : rows_) if (row.group == group_ && section(row).first == page_) unique_name(categories_, section(row).second);
    auto general = std::find(categories_.begin(), categories_.end(), "General");
    if (general != categories_.end()) std::rotate(categories_.begin(), general, general + 1);
    choose_name(category_, categories_);
    populate_tabs(groups_tab_, groups_, group_); populate_tabs(pages_tab_, pages_, page_);
    SendMessageW(categories_list_, LB_RESETCONTENT, 0, 0);
    for (int i = 0; i < categories_.size(); ++i) {
        const auto label = wide(categories_[i]); SendMessageW(categories_list_, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label.c_str()));
        if (categories_[i] == category_) SendMessageW(categories_list_, LB_SETCURSEL, i, 0);
    }
    rebuilding_ = false; rebuild_list();
}
void Panel::rebuild_list()
{
    rebuilding_ = true; visible_.clear(); SendMessageW(list_, WM_SETREDRAW, FALSE, 0); ListView_DeleteAllItems(list_);
    for (int i = 0; i < rows_.size(); ++i) {
        const Row &row = rows_[i];
        if (row.group != group_ || section(row) != std::make_pair(page_, category_)) continue;
        const int index = static_cast<int>(visible_.size()); visible_.push_back(i);
        auto name = wide(row.name); LVITEMW item{}; item.mask = LVIF_TEXT; item.iItem = index; item.pszText = name.data();
        SendMessageW(list_, LVM_INSERTITEMW, 0, reinterpret_cast<LPARAM>(&item));
        auto value = row.boolean ? (row.value ? std::wstring(L"On") : std::wstring(L"Off")) : std::to_wstring(row.value);
        item.iSubItem = 1; item.pszText = value.data(); SendMessageW(list_, LVM_SETITEMTEXTW, index, reinterpret_cast<LPARAM>(&item));
    }
    auto selected = std::find(visible_.begin(), visible_.end(), selected_);
    if (selected == visible_.end()) { selected_ = visible_.empty() ? -1 : visible_.front(); selected = visible_.begin(); }
    if (!visible_.empty()) { const int index = static_cast<int>(selected - visible_.begin()); ListView_SetItemState(list_, index, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED); ListView_EnsureVisible(list_, index, FALSE); }
    SendMessageW(list_, WM_SETREDRAW, TRUE, 0); InvalidateRect(list_, nullptr, TRUE);
    rebuilding_ = false; show_selected();
}
void Panel::show_selected()
{
    const Row *row = selected_ >= 0 && selected_ < rows_.size() ? &rows_[selected_] : nullptr;
    SetWindowTextW(label_, row ? wide(row->name).c_str() : L"No settings in this section.");
    std::string detail = row ? row->detail : "";
    if (row && !row->boolean) detail = "Range: " + std::to_string(row->minimum) + " to " + std::to_string(row->maximum) + ", increment " + std::to_string(row->step) + ". " + detail;
    SetWindowTextW(detail_, wide(detail).c_str());
    const bool boolean = row && row->boolean;
    ShowWindow(toggle_, boolean ? SW_SHOW : SW_HIDE);
    for (HWND control : {slider_, value_, apply_}) ShowWindow(control, row && !boolean ? SW_SHOW : SW_HIDE);
    for (HWND control : {toggle_, slider_, value_, apply_}) EnableWindow(control, row && row->enabled);
    if (!row) return;
    SendMessageW(toggle_, BM_SETCHECK, row->value ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(slider_, TBM_SETRANGEMIN, FALSE, 0);
    SendMessageW(slider_, TBM_SETRANGEMAX, FALSE, slider_steps(*row));
    const int64_t steps = value_steps(*row);
    const int64_t selected_step = (static_cast<int64_t>(std::clamp(row->value, row->minimum, row->maximum)) - row->minimum) / std::max(1, row->step);
    SendMessageW(slider_, TBM_SETPOS, TRUE, steps ? (selected_step * slider_steps(*row) + steps / 2) / steps : 0);
    SetWindowTextW(value_, std::to_wstring(row->value).c_str());
}
void Panel::apply_value(int value)
{
    if (selected_ < 0 || selected_ >= rows_.size()) return;
    const Row row = rows_[selected_];
    if (!row.enabled || value == row.value) return;
    if (value < row.minimum || value > row.maximum || (static_cast<int64_t>(value) - row.minimum) % row.step) { MessageBoxW(window_, L"Enter a value within the displayed range and increment.", L"Invalid setting", MB_OK | MB_ICONWARNING); return; }
    try {
        row.apply(value); rows_[selected_].value = value;
        SendMessageW(GetParent(window_), WM_APP + 11, 0, 0);
    } catch (const std::exception &error) { show_selected(); MessageBoxW(window_, wide(error.what()).c_str(), L"Setting could not be applied", MB_OK | MB_ICONERROR); }
}
void Panel::apply_editor()
{
    wchar_t text[64]; GetWindowTextW(value_, text, 64); wchar_t *end = nullptr;
    errno = 0; const long long value = wcstoll(text, &end, 10);
    if (errno || end == text || *end || value < INT_MIN || value > INT_MAX) { MessageBoxW(window_, L"Enter an integer.", L"Invalid setting", MB_OK | MB_ICONWARNING); return; }
    apply_value(static_cast<int>(value));
}
void Panel::edit_selected()
{
    if (selected_ < 0 || !rows_[selected_].enabled) return;
    if (rows_[selected_].boolean) apply_value(!rows_[selected_].value);
    else { SetFocus(value_); SendMessageW(value_, EM_SETSEL, 0, -1); }
}
LRESULT Panel::notify(NMHDR *header)
{
    if (header->code == TCN_SELCHANGE && (header->hwndFrom == groups_tab_ || header->hwndFrom == pages_tab_)) {
        const int index = TabCtrl_GetCurSel(header->hwndFrom);
        const auto &names = header->hwndFrom == groups_tab_ ? groups_ : pages_;
        if (index >= 0 && index < names.size()) { (header->hwndFrom == groups_tab_ ? group_ : page_) = names[index]; category_.clear(); rebuild_navigation(); }
        return 0;
    }
    if (header->hwndFrom != list_) return 0;
    if (header->code == LVN_ITEMCHANGED) {
        const int index = ListView_GetNextItem(list_, -1, LVNI_SELECTED);
        if (index >= 0 && index < visible_.size()) { selected_ = visible_[index]; show_selected(); }
    }
    if (header->code == NM_DBLCLK && reinterpret_cast<NMITEMACTIVATE *>(header)->iItem >= 0) edit_selected();
    if (header->code == LVN_KEYDOWN) { const auto key = reinterpret_cast<NMLVKEYDOWN *>(header)->wVKey; if (key == VK_SPACE || key == VK_RETURN) edit_selected(); }
    if (header->code == NM_CUSTOMDRAW) {
        auto *draw = reinterpret_cast<NMLVCUSTOMDRAW *>(header);
        if (draw->nmcd.dwDrawStage == CDDS_PREPAINT) return CDRF_NOTIFYITEMDRAW;
        if (draw->nmcd.dwDrawStage == CDDS_ITEMPREPAINT && draw->nmcd.dwItemSpec < visible_.size() && !rows_[visible_[draw->nmcd.dwItemSpec]].enabled) draw->clrText = GetSysColor(COLOR_GRAYTEXT);
    }
    return CDRF_DODEFAULT;
}
void append_mod_rows(std::vector<Row> &rows, const mod_content::Session &session, std::function<void(const std::string &, int)> apply)
{
    for (const auto &s : session.settings()) {
        auto key = s.key(); Row row{s.mod, s.name, s.category, s.effective ? s.description : s.disabled_reason, s.boolean, s.effective, s.value, s.minimum, s.maximum};
        row.apply = [apply, key](int value) { apply(key, value); }; rows.push_back(std::move(row));
    }
}
void append_hardcoded_rows(std::vector<Row> &rows, const std::function<int(const char *, int)> &get, const std::function<void(const char *, int)> &set)
{
    for (const auto &option : config_options) {
        if (!option.visible) continue;
        const bool boolean = option.minimum == 0 && option.maximum == 1;
        Row row{"Hardcoded settings", option.name, option.category, "", boolean, true, get(option.key, option.fallback), option.minimum, option.maximum};
        row.step = option.step;
        if (option.step > 1) row.detail = "Increment: " + std::to_string(option.step);
        if (option.id == CONFIG_SCALE_FILTER) row.detail = "0 = Automatic, 1 = Nearest, 2 = Linear, 3 = Best";
        if (option.id == CONFIG_UI_EMPIRE_SIDEBAR_SORT_METHOD) row.detail = "0 = City, 1 = Export quota, 2 = Import quota, 3 = Route cost, 4 = Profit";
        if (option.id == CONFIG_UI_CART_DEPOT_TOOLTIP_STYLE) row.detail = "0 = None, 1 = Minimal, 2 = Full";
        const char *key = option.key; row.apply = [set, key](int value) { set(key, value); }; rows.push_back(std::move(row));
    }
    std::stable_sort(rows.begin(), rows.end(), [](const Row &a, const Row &b) { return a.category < b.category; });
}
namespace {
struct OptionsDialog { Panel panel; std::function<std::vector<Row>()> rows; WindowScale scale; };
LRESULT CALLBACK options_proc(HWND window, UINT msg, WPARAM wp, LPARAM lp)
{
    auto *state = reinterpret_cast<OptionsDialog *>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (msg == WM_CREATE) { state = static_cast<OptionsDialog *>(reinterpret_cast<CREATESTRUCT *>(lp)->lpCreateParams); SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state)); state->panel.create(window, 20); state->panel.set_rows(state->rows()); state->scale.apply(window); state->panel.scale(state->scale.dpi()); return 0; }
    if (msg == WM_SIZE && state) { const int margin = state->scale.pixels(12); MoveWindow(state->panel.handle(), margin, margin, LOWORD(lp) - margin * 2, HIWORD(lp) - margin * 2, TRUE); return 0; }
    if (msg == WM_DPICHANGED && state) {
        state->scale.apply(window, HIWORD(wp)); state->panel.scale(state->scale.dpi());
        const RECT &rect = *reinterpret_cast<RECT *>(lp);
        SetWindowPos(window, nullptr, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, SWP_NOZORDER | SWP_NOACTIVATE); return 0;
    }
    if (msg == WM_NOTIFY && state) return state->panel.notify(reinterpret_cast<NMHDR *>(lp));
    if (msg == WM_APP + 11 && state) { state->panel.set_rows(state->rows()); return 0; }
    if (msg == WM_CLOSE) { DestroyWindow(window); return 0; }
    return DefWindowProcW(window, msg, wp, lp);
}
}
void show_dialog(HWND owner, const std::function<std::vector<Row>()> &rows)
{
    INITCOMMONCONTROLSEX icc{sizeof(icc), ICC_LISTVIEW_CLASSES | ICC_TAB_CLASSES | ICC_BAR_CLASSES}; InitCommonControlsEx(&icc); register_class(L"VespasianModOptions", options_proc);
    OptionsDialog state; state.rows = rows; RECT rect{}; GetWindowRect(owner, &rect);
    HWND dialog = CreateWindowExW(WS_EX_DLGMODALFRAME, L"VespasianModOptions", L"Settings - double-click or press Space to change", WS_OVERLAPPEDWINDOW, rect.left + 30, rect.top + 30, 900, 650, owner, nullptr, GetModuleHandleW(nullptr), &state);
    state.scale.client_size(dialog, 980, 580);
    modal_loop(owner, dialog);
}
}
#endif
