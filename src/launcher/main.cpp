#include "platform/mod_options_win32.h"
#include <shellapi.h>
#include "core/config_options.h"
#include <shlobj.h>
#include <algorithm>
#include <fstream>
#include <set>
#include <sstream>
#include <stdexcept>
#include <iostream>

namespace fs = std::filesystem;
using native_options::wide;
namespace {
bool testing = false;
enum { TAB = 100, AVAILABLE, ACTIVE, ADD, REMOVE, UP, DOWN, START, OPTIONS };
struct Entry { std::string name, problem; bool missing = false; };
struct Launcher {
    HWND window = nullptr, tabs = nullptr, available = nullptr, active = nullptr, status = nullptr;
    HWND buttons[5]{};
    native_options::Panel options;
    native_options::WindowScale scale;
    fs::path root, user, executable;
    std::vector<std::string> mods, unused;
    std::vector<Entry> entries;
    mod_content::Session settings;
    bool rebuilding = false, right_selected = false, valid = false;
    int selected = 0;
    std::string problem;
    fs::path list_path() const { return user / "config/mod-list"; }
};
HWND make(HWND parent, const wchar_t *type, const wchar_t *text, DWORD style, int id)
{
    HWND out = CreateWindowExW(0, type, text, WS_CHILD | WS_VISIBLE | style, 0, 0, 100, 30, parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), GetModuleHandleW(nullptr), nullptr);
    SendMessageW(out, WM_SETFONT, reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), TRUE); return out;
}
void column(HWND list, const wchar_t *title)
{
    LVCOLUMNW c{}; c.mask = LVCF_TEXT | LVCF_WIDTH; c.pszText = const_cast<wchar_t *>(title); c.cx = 370; SendMessageW(list, LVM_INSERTCOLUMNW, 0, reinterpret_cast<LPARAM>(&c));
    ListView_SetExtendedListViewStyle(list, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_LABELTIP);
}
void row(HWND list, int index, const std::string &text)
{
    auto label = wide(text); LVITEMW item{}; item.mask = LVIF_TEXT; item.iItem = index; item.pszText = label.data(); SendMessageW(list, LVM_INSERTITEMW, 0, reinterpret_cast<LPARAM>(&item));
}
void refresh_options(Launcher &s)
{
    std::vector<native_options::Row> rows;
    auto ini = s.user / "config/Vespasian.ini";
    native_options::append_hardcoded_rows(rows, [ini](const char *key, int fallback) {
        if (!fs::exists(ini)) return fallback;
        std::istringstream input(mod_content::read(ini)); std::string line;
        while (std::getline(input, line)) if (line.compare(0, strlen(key) + 1, std::string(key) + "=") == 0) {
            auto value = line.substr(strlen(key) + 1); if (!value.empty() && value.back() == '\r') value.pop_back();
            if (std::string(key) == "scale_filter") { if (value == "auto") return 0; if (value == "nearest") return 1; if (value == "linear") return 2; if (value == "best") return 3; }
            try { return std::stoi(value); } catch (...) { return fallback; }
        }
        return fallback;
    }, [ini](const char *key, int value) {
        std::istringstream input(fs::exists(ini) ? mod_content::read(ini) : ""); std::string line, output; bool found = false;
        while (std::getline(input, line)) { if (!line.empty() && line.back() == '\r') line.pop_back(); if (line.compare(0, strlen(key) + 1, std::string(key) + "=") == 0) { line = std::string(key) + "=" + std::to_string(value); found = true; } output += line + "\r\n"; }
        if (!found) output += std::string(key) + "=" + std::to_string(value) + "\r\n";
        mod_content::write_atomic(ini, output);
    });
    native_options::append_mod_rows(rows, s.settings, [&s](const std::string &key, int value) {
        auto candidate = s.settings; candidate.set(key, value); candidate.save(); s.settings = std::move(candidate);
    });
    s.options.set_rows(std::move(rows));
}
void enable_buttons(Launcher &s)
{
    EnableWindow(s.buttons[0], !s.right_selected && !s.unused.empty());
    EnableWindow(s.buttons[1], s.right_selected && !s.mods.empty());
    EnableWindow(s.buttons[2], s.right_selected && s.selected > 0);
    EnableWindow(s.buttons[3], s.right_selected && s.selected + 1 < static_cast<int>(s.mods.size()));
    EnableWindow(s.buttons[4], s.valid);
}
void select(Launcher &s)
{
    if ((s.right_selected ? s.mods : s.unused).empty()) s.right_selected = !s.right_selected;
    const auto size = (s.right_selected ? s.mods : s.unused).size();
    s.rebuilding = true;
    ListView_SetItemState(s.available, -1, 0, LVIS_SELECTED | LVIS_FOCUSED); ListView_SetItemState(s.active, -1, 0, LVIS_SELECTED | LVIS_FOCUSED);
    if (size) { s.selected = std::clamp(s.selected, 0, static_cast<int>(size) - 1); HWND list = s.right_selected ? s.active : s.available; ListView_SetItemState(list, s.selected, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED); ListView_EnsureVisible(list, s.selected, FALSE); }
    s.rebuilding = false; enable_buttons(s);
}
void refresh(Launcher &s)
{
    s.rebuilding = true; s.unused.clear(); s.entries.clear(); s.problem.clear(); s.valid = !s.mods.empty();
    if (!s.valid) s.problem = "Add a mod to start the game.";
    std::set<std::string> active_names, preceding;
    for (const auto &name : s.mods) active_names.insert(mod_content::lower(name));
    if (fs::exists(s.root / "Mods")) for (const auto &item : fs::directory_iterator(s.root / "Mods")) {
        auto name = item.path().filename().u8string();
        if (item.is_directory() && fs::exists(item.path() / "mod.xml") && !active_names.count(mod_content::lower(name))) s.unused.push_back(name);
    }
    std::sort(s.unused.begin(), s.unused.end());
    std::vector<mod_content::Layer> layers;
    for (const auto &name : s.mods) {
        Entry entry{name}; auto path = s.root / "Mods" / fs::u8path(name);
        entry.missing = !fs::is_directory(path);
        if (entry.missing) entry.problem = "Mod folder is missing";
        else try {
            auto metadata = mod_content::manifest(path / "mod.xml");
            if (metadata.name != name) entry.problem = "Manifest name must match folder";
            for (const auto &dep : metadata.dependencies) if (!preceding.count(mod_content::lower(dep))) entry.problem += (entry.problem.empty() ? "" : "; ") + std::string("Requires earlier mod: ") + dep;
        } catch (const std::exception &e) { entry.problem = e.what(); }
        if (!preceding.insert(mod_content::lower(name)).second) entry.problem = "Duplicate mod";
        if (!entry.problem.empty()) { s.valid = false; if (s.problem.empty()) s.problem = name + ": " + entry.problem; }
        layers.push_back({name, path}); s.entries.push_back(std::move(entry));
    }
    s.settings = {};
    if (s.valid) try { s.settings.load(layers, s.user / "config/mod-settings.xml"); } catch (const std::exception &e) { s.valid = false; s.problem = e.what(); }
    if (s.valid && !fs::exists(s.executable)) { s.valid = false; s.problem = "Vespasian.exe was not found beside the launcher."; }
    ListView_DeleteAllItems(s.available); ListView_DeleteAllItems(s.active);
    for (int i = 0; i < static_cast<int>(s.unused.size()); ++i) row(s.available, i, s.unused[i]);
    for (int i = 0; i < static_cast<int>(s.entries.size()); ++i) row(s.active, i, s.entries[i].missing ? "<" + s.entries[i].name + " -- is missing!>" : s.entries[i].name);
    s.rebuilding = false; select(s); refresh_options(s);
    SetWindowTextW(s.status, wide(s.valid ? "Later mods override earlier fields. Settings: double-click or press Space to change." : s.problem).c_str());
}
void layout(Launcher &s)
{
    RECT r{}; GetClientRect(s.window, &r); const int w = r.right, h = r.bottom;
    auto p = [&](int value) { return s.scale.pixels(value); };
    MoveWindow(s.tabs, p(12), p(12), w - p(24), h - p(82), TRUE);
    bool mods = TabCtrl_GetCurSel(s.tabs) == 0; int table_w = (w - p(52)) / 2;
    MoveWindow(s.available, p(24), p(52), table_w, h - p(145), TRUE); MoveWindow(s.active, p(28) + table_w, p(52), table_w, h - p(145), TRUE);
    ListView_SetColumnWidth(s.available, 0, std::max(p(160), table_w - p(24))); ListView_SetColumnWidth(s.active, 0, std::max(p(160), table_w - p(24)));
    MoveWindow(s.options.handle(), p(24), p(52), w - p(48), h - p(145), TRUE);
    ShowWindow(s.available, mods ? SW_SHOW : SW_HIDE); ShowWindow(s.active, mods ? SW_SHOW : SW_HIDE); ShowWindow(s.options.handle(), mods ? SW_HIDE : SW_SHOW);
    for (int i = 0; i < 5; ++i) { MoveWindow(s.buttons[i], i == 4 ? w - p(144) : p(12 + i * 130), h - p(64), p(i == 4 ? 132 : 120), p(30), TRUE); ShowWindow(s.buttons[i], mods || i == 4 ? SW_SHOW : SW_HIDE); }
    MoveWindow(s.status, p(12), h - p(28), w - p(24), p(24), TRUE);
}
std::wstring quote(const std::wstring &arg)
{
    std::wstring out = L"\""; unsigned slashes = 0;
    for (wchar_t c : arg) { if (c == L'\\') { ++slashes; continue; } if (c == L'"') out.append(slashes * 2 + 1, L'\\'); else out.append(slashes, L'\\'); slashes = 0; out += c; }
    out.append(slashes * 2, L'\\'); return out + L"\"";
}
void command(Launcher &s, int id)
{
    if (id == START) {
        refresh(s); if (!s.valid) return;
        mod_content::write_list(s.list_path(), s.mods); s.settings.save();
        std::wstring cmd = quote(s.executable.wstring()) + L" " + quote(s.root.wstring()) + L" --mod " + quote(wide(s.mods.back()));
        STARTUPINFOW startup{}; startup.cb = sizeof(startup); PROCESS_INFORMATION process{};
        if (!CreateProcessW(s.executable.c_str(), cmd.data(), nullptr, nullptr, FALSE, CREATE_UNICODE_ENVIRONMENT | CREATE_NEW_CONSOLE, nullptr, s.root.c_str(), &startup, &process)) throw std::runtime_error("Windows could not start Vespasian (error " + std::to_string(GetLastError()) + ")");
        CloseHandle(process.hThread); CloseHandle(process.hProcess); DestroyWindow(s.window); return;
    }
    auto previous = s.mods;
    if (id == ADD && !s.right_selected && !s.unused.empty()) { s.mods.push_back(s.unused[s.selected]); s.right_selected = true; s.selected = static_cast<int>(s.mods.size()) - 1; }
    else if (id == REMOVE && s.right_selected && !s.mods.empty()) s.mods.erase(s.mods.begin() + s.selected);
    else if (id == UP && s.right_selected && s.selected > 0) { std::swap(s.mods[s.selected], s.mods[s.selected - 1]); --s.selected; }
    else if (id == DOWN && s.right_selected && s.selected + 1 < static_cast<int>(s.mods.size())) { std::swap(s.mods[s.selected], s.mods[s.selected + 1]); ++s.selected; }
    else return;
    try { mod_content::write_list(s.list_path(), s.mods); } catch (...) { s.mods = previous; refresh(s); throw; }
    refresh(s);
}
LRESULT CALLBACK window_proc(HWND window, UINT msg, WPARAM wp, LPARAM lp)
{
    auto *s = reinterpret_cast<Launcher *>(GetWindowLongPtrW(window, GWLP_USERDATA));
    try {
        if (msg == WM_CREATE) {
            s = static_cast<Launcher *>(reinterpret_cast<CREATESTRUCT *>(lp)->lpCreateParams); s->window = window; SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(s));
            s->tabs = make(window, WC_TABCONTROLW, L"", WS_TABSTOP, TAB);
            TCITEMW item{}; item.mask = TCIF_TEXT; item.pszText = const_cast<wchar_t *>(L"Mods"); SendMessageW(s->tabs, TCM_INSERTITEMW, 0, reinterpret_cast<LPARAM>(&item)); item.pszText = const_cast<wchar_t *>(L"Mod options"); SendMessageW(s->tabs, TCM_INSERTITEMW, 1, reinterpret_cast<LPARAM>(&item));
            const DWORD list_style = WS_BORDER | WS_TABSTOP | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS;
            s->available = make(window, WC_LISTVIEWW, L"", list_style, AVAILABLE); s->active = make(window, WC_LISTVIEWW, L"", list_style, ACTIVE);
            column(s->available, L"Available mods"); column(s->active, L"Active mods (load order)"); s->options.create(window, OPTIONS);
            const wchar_t *labels[] = {L"Add", L"Remove", L"Move up", L"Move down", L"Start game"};
            for (int i = 0; i < 5; ++i) s->buttons[i] = make(window, L"BUTTON", labels[i], WS_TABSTOP, ADD + i);
            s->status = make(window, L"STATIC", L"", SS_LEFT, 0); s->scale.apply(window); s->options.scale(s->scale.dpi()); refresh(*s); return 0;
        }
        if (msg == WM_GETMINMAXINFO) { auto *info = reinterpret_cast<MINMAXINFO *>(lp); const UINT dpi = s ? s->scale.dpi() : GetDpiForWindow(window); info->ptMinTrackSize = {MulDiv(780, dpi, 96), MulDiv(450, dpi, 96)}; return 0; }
        if (msg == WM_DPICHANGED && s) {
            s->scale.apply(window, HIWORD(wp)); s->options.scale(s->scale.dpi());
            const RECT &rect = *reinterpret_cast<RECT *>(lp);
            SetWindowPos(window, nullptr, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, SWP_NOZORDER | SWP_NOACTIVATE); layout(*s); return 0;
        }
        if (msg == WM_SIZE && s) { layout(*s); return 0; }
        if (msg == WM_COMMAND && s) { command(*s, LOWORD(wp)); return 0; }
        if (msg == WM_APP + 11 && s) { refresh_options(*s); return 0; }
        if (msg == WM_APP + 12 && s && !s->rebuilding) { select(*s); return 0; }
        if (msg == WM_NOTIFY && s) {
            auto *header = reinterpret_cast<NMHDR *>(lp);
            if (header->hwndFrom == s->tabs && header->code == TCN_SELCHANGE) { layout(*s); return 0; }
            if (header->hwndFrom == s->options.handle()) return s->options.notify(header);
            if (header->hwndFrom == s->active || header->hwndFrom == s->available) {
                if (header->code == NM_DBLCLK && !s->rebuilding) {
                    const auto *click = reinterpret_cast<NMITEMACTIVATE *>(lp);
                    if (click->iItem >= 0) { s->right_selected = header->hwndFrom == s->active; s->selected = click->iItem; select(*s); command(*s, s->right_selected ? REMOVE : ADD); }
                    return 0;
                }
                if (header->code == LVN_ITEMCHANGED && !s->rebuilding) {
                    auto *change = reinterpret_cast<NMLISTVIEW *>(lp);
                    if ((change->uNewState & LVIS_SELECTED) && !(change->uOldState & LVIS_SELECTED)) { s->right_selected = header->hwndFrom == s->active; s->selected = change->iItem; select(*s); }
                    else if ((change->uOldState & LVIS_SELECTED) && !(change->uNewState & LVIS_SELECTED)) PostMessageW(window, WM_APP + 12, 0, 0);
                }
                if (header->code == NM_CUSTOMDRAW && header->hwndFrom == s->active) {
                    auto *draw = reinterpret_cast<NMLVCUSTOMDRAW *>(lp);
                    if (draw->nmcd.dwDrawStage == CDDS_PREPAINT) return CDRF_NOTIFYITEMDRAW;
                    if (draw->nmcd.dwDrawStage == CDDS_ITEMPREPAINT && draw->nmcd.dwItemSpec < s->entries.size()) {
                        const auto &entry = s->entries[draw->nmcd.dwItemSpec];
                        if (entry.missing) draw->clrText = RGB(210, 25, 25);
                        else if (!entry.problem.empty()) { draw->clrText = RGB(150, 112, 0); draw->clrTextBk = RGB(255, 250, 200); }
                    }
                }
            }
        }
        if (msg == WM_DESTROY) { PostQuitMessage(0); return 0; }
    } catch (const std::exception &e) { if (testing) { std::cerr << e.what() << '\n'; return -1; } MessageBoxW(window, wide(e.what()).c_str(), L"Vespasian Launcher", MB_OK | MB_ICONERROR); }
    return DefWindowProcW(window, msg, wp, lp);
}
fs::path preference(const fs::path &dir, const char *name)
{
    if (!fs::exists(dir / name)) return {}; auto text = mod_content::read(dir / name);
    while (!text.empty() && (text.back() == '\r' || text.back() == '\n')) text.pop_back(); return fs::u8path(text);
}
void register_launcher(HINSTANCE instance)
{
    WNDCLASSEXW wc{}; wc.cbSize = sizeof(wc); wc.lpfnWndProc = window_proc; wc.hInstance = instance; wc.hCursor = LoadCursor(nullptr, IDC_ARROW); wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1); wc.lpszClassName = L"VespasianLauncher"; wc.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(101)); wc.hIconSm = wc.hIcon; RegisterClassExW(&wc);
}
void capture(HWND window, const fs::path &path)
{
    RECT rect{}; GetClientRect(window, &rect); int width = rect.right, height = rect.bottom;
    HDC screen = GetDC(window), memory = CreateCompatibleDC(screen);
    BITMAPINFO info{}; info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER); info.bmiHeader.biWidth = width; info.bmiHeader.biHeight = -height; info.bmiHeader.biPlanes = 1; info.bmiHeader.biBitCount = 32;
    void *pixels = nullptr; HBITMAP bitmap = CreateDIBSection(screen, &info, DIB_RGB_COLORS, &pixels, nullptr, 0); auto old = SelectObject(memory, bitmap);
    RedrawWindow(window, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
    if (!PrintWindow(window, memory, PW_CLIENTONLY)) throw std::runtime_error("Cannot capture launcher test window");
    BITMAPFILEHEADER header{}; header.bfType = 0x4d42; header.bfOffBits = sizeof(header) + sizeof(info.bmiHeader); header.bfSize = header.bfOffBits + width * height * 4;
    std::ofstream file(path, std::ios::binary); file.write(reinterpret_cast<char *>(&header), sizeof(header)); file.write(reinterpret_cast<char *>(&info.bmiHeader), sizeof(info.bmiHeader)); file.write(static_cast<char *>(pixels), width * height * 4);
    SelectObject(memory, old); DeleteObject(bitmap); DeleteDC(memory); ReleaseDC(window, screen);
}
int self_test(HINSTANCE instance, const fs::path &output)
{
    fs::create_directories(output); auto root = fs::temp_directory_path() / ("vespasian-launcher-test-" + std::to_string(GetCurrentProcessId()));
    struct Cleanup { fs::path path; ~Cleanup() { std::error_code ignored; fs::remove_all(path, ignored); } } cleanup{root};
    auto require = [](bool okay, const char *message) { if (!okay) throw std::runtime_error(message); };
    mod_content::write_atomic(root / "Mods/Base/mod.xml", "<mod><name value='Base'/><description value='Base mod'/><version value='1'/><dependencies/><settings><setting id='X' name='Multiple barracks' type='bool' default='false'/></settings></mod>");
    mod_content::write_atomic(root / "Mods/Base/BuildingType/b.xml", "<building type='barracks'><construction $!X{max_count=\"1\"}/></building>");
    mod_content::write_atomic(root / "Mods/Override/mod.xml", "<mod><name value='Override'/><description value='Overlay'/><version value='1'/><dependencies><mod name='Base'/></dependencies><settings><setting id='AGE' name='Retirement age' type='int(40,90)' default='50'/></settings></mod>");
    mod_content::write_atomic(root / "Mods/Override/BuildingType/b.xml", "<building type='barracks'><construction/><labor age='$AGE'/></building>");
    mod_content::write_atomic(root / "Vespasian.exe", "Test fixture; never executed");
    Launcher s; s.root = root; s.user = root; s.executable = root / "Vespasian.exe";
    register_launcher(instance);
    HWND window = CreateWindowExW(0, L"VespasianLauncher", L"Vespasian Launcher", WS_OVERLAPPEDWINDOW, 0, 0, 980, 680, nullptr, nullptr, instance, &s);
    require(window != nullptr, "Launcher creation failed");
    s.scale.client_size(window, 980, 640); ShowWindow(window, SW_SHOWNOACTIVATE); UpdateWindow(window);
    auto selected = [&] { return ListView_GetSelectedCount(s.active) + ListView_GetSelectedCount(s.available); };
    require(selected() == 1 && !s.valid && s.mods.empty(), "Empty stack must retain an available selection");
    NMITEMACTIVATE activate{}; activate.hdr.hwndFrom = s.available; activate.hdr.code = NM_DBLCLK; activate.iItem = 0;
    SendMessageW(window, WM_NOTIFY, AVAILABLE, reinterpret_cast<LPARAM>(&activate)); require(s.mods.size() == 1 && s.valid && selected() == 1, "Add Base failed");
    s.right_selected = false; s.selected = 0; select(s); command(s, ADD);
    require(s.mods.size() == 2 && s.valid && !s.settings.settings()[0].effective && selected() == 1, "Override provenance or selection failed");
    command(s, UP); require(!s.valid && !s.entries[0].problem.empty(), "Dependency order was not detected");
    NMLVCUSTOMDRAW warning{}; warning.nmcd.hdr.hwndFrom = s.active; warning.nmcd.hdr.code = NM_CUSTOMDRAW; warning.nmcd.dwDrawStage = CDDS_ITEMPREPAINT;
    SendMessageW(window, WM_NOTIFY, ACTIVE, reinterpret_cast<LPARAM>(&warning)); require(warning.clrTextBk == RGB(255, 250, 200), "Wrong order must be highlighted yellow");
    command(s, DOWN); require(s.valid, "Move down did not repair dependency order");
    s.mods.push_back("Missing mod"); refresh(s); require(s.entries.back().missing && !s.valid, "Missing mod not detected");
    require(IsWindowEnabled(s.buttons[1]) && !IsWindowEnabled(s.buttons[4]), "Invalid stack must allow removal and prevent launching");
    NMLVCUSTOMDRAW draw{}; draw.nmcd.hdr.hwndFrom = s.active; draw.nmcd.hdr.code = NM_CUSTOMDRAW; draw.nmcd.dwDrawStage = CDDS_ITEMPREPAINT; draw.nmcd.dwItemSpec = 2;
    SendMessageW(window, WM_NOTIFY, ACTIVE, reinterpret_cast<LPARAM>(&draw)); require(draw.clrText == RGB(210, 25, 25), "Missing mod is not red");
    capture(window, output / "launcher-mods.bmp");
    s.right_selected = true; s.selected = 2; select(s); command(s, REMOVE); require(s.valid, "Removing missing entry failed");
    require(mod_content::read_list(s.list_path()) == s.mods, "Load order was not persisted");
    require(s.options.group_count() == 3, "Hardcoded settings and mod headings are missing");
    TabCtrl_SetCurSel(s.tabs, 1); layout(s); s.options.select_section("Hardcoded settings", "City Management", "Storage and Markets"); capture(window, output / "launcher-options.bmp");
    s.options.select_section("Override", "Options", "General"); capture(window, output / "launcher-mod-options.bmp");
    auto setting_value = [&](const char *key) { for (const auto &setting : s.settings.settings()) if (setting.key() == key) return setting.value; throw std::runtime_error("Missing test setting"); };
    SetWindowTextW(GetDlgItem(s.options.handle(), 36), L"75"); SendMessageW(s.options.handle(), WM_COMMAND, 37, 0);
    require(setting_value("Override:AGE") == 75, "Numeric options control did not persist its value");
    HWND slider = GetDlgItem(s.options.handle(), 35); SendMessageW(slider, TBM_SETPOS, TRUE, 25);
    SendMessageW(s.options.handle(), WM_HSCROLL, TB_ENDTRACK, reinterpret_cast<LPARAM>(slider));
    require(setting_value("Override:AGE") == 65, "Slider release did not apply the mod setting");
    s.options.select_section("Base", "Options", "General");
    require(!IsWindowEnabled(GetDlgItem(s.options.handle(), 34)), "Overridden checkbox is not disabled");
    s.right_selected = true; s.selected = 1; select(s); command(s, REMOVE); command(s, REMOVE);
    require(s.mods.empty() && selected() == 1, "Removing the final mod lost selection");
    activate.hdr.hwndFrom = s.available; activate.iItem = 0; SendMessageW(window, WM_NOTIFY, AVAILABLE, reinterpret_cast<LPARAM>(&activate));
    require(s.mods.size() == 1, "Double-click did not add mod");
    activate.hdr.hwndFrom = s.active; SendMessageW(window, WM_NOTIFY, ACTIVE, reinterpret_cast<LPARAM>(&activate));
    require(s.mods.empty() && selected() == 1, "Double-click did not remove mod");
    activate.iItem = -1; SendMessageW(window, WM_NOTIFY, ACTIVE, reinterpret_cast<LPARAM>(&activate)); require(s.mods.empty(), "Empty-space double-click changed stack");
    std::vector<native_options::Row> hardcoded;
    native_options::append_hardcoded_rows(hardcoded, [](const char *, int fallback) { return fallback; }, [](const char *, int) {});
    require(hardcoded.size() > 100, "Hardcoded catalog is incomplete");
    for (const auto &row : hardcoded) require(!row.category.empty() && row.category != "Hardcoded settings", "Hardcoded option lost its category");
    for (UINT dpi : {96u, 144u, 192u}) {
        RECT suggested{0, 0, MulDiv(980, dpi, 96), MulDiv(640, dpi, 96)};
        SendMessageW(window, WM_DPICHANGED, MAKELONG(dpi, dpi), reinterpret_cast<LPARAM>(&suggested));
        require(s.scale.dpi() == dpi && ListView_GetColumnWidth(s.options.setting_list(), 1) == MulDiv(90, dpi, 96), "DPI change did not scale settings columns");
        RECT client{}, button{}; GetClientRect(window, &client); GetWindowRect(s.buttons[4], &button); MapWindowPoints(nullptr, window, reinterpret_cast<POINT *>(&button), 2);
        require(button.bottom <= client.bottom && button.right <= client.right, "DPI change clipped Start game");
    }
    native_options::validate_number_dialog_layout(window, [&](HWND dialog, UINT dpi) { capture(dialog, output / ("number-dialog-" + std::to_string(dpi) + ".bmp")); });
    s.root = root / "empty"; refresh(s); require(selected() == 0 && s.unused.empty() && !s.valid, "Zero available rows must remain usable");
    DestroyWindow(window);
    mod_content::write_atomic(output / "launcher-tests.txt", "PASS: empty stack, add/remove/reorder, missing red entry, dependency validation, one selection, persistence, disabled setting, complete categorized hardcoded catalog, double-click add/remove, and 96/144/192 DPI layouts.\n");
    return 0;
}
}
int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show)
{
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
    try {
        INITCOMMONCONTROLSEX icc{sizeof(icc), ICC_TAB_CLASSES | ICC_LISTVIEW_CLASSES | ICC_BAR_CLASSES}; InitCommonControlsEx(&icc);
        int test_argc = 0; auto test_args = CommandLineToArgvW(GetCommandLineW(), &test_argc);
        fs::path test_output;
        for (int i = 1; i + 1 < test_argc; ++i) if (std::wstring(test_args[i]) == L"--self-test") { testing = true; test_output = test_args[i + 1]; break; }
        LocalFree(test_args);
        if (testing) return self_test(instance, test_output);
        wchar_t path[32768]; GetModuleFileNameW(nullptr, path, 32768); Launcher s; auto bin = fs::path(path).parent_path(); s.executable = bin / "Vespasian.exe";
        PWSTR roaming = nullptr; if (FAILED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &roaming))) throw std::runtime_error("Cannot locate game preferences");
        fs::path prefs = fs::path(roaming) / "augustus/augustus"; CoTaskMemFree(roaming);
        s.root = fs::exists(bin / "Mods") ? bin : preference(prefs, "data_dir.txt"); s.user = preference(prefs, "user_dir.txt");
        int argc = 0; auto args = CommandLineToArgvW(GetCommandLineW(), &argc);
        for (int i = 1; i < argc; ++i) {
            if (std::wstring(args[i]) == L"--data-dir" && i + 1 < argc) s.root = args[++i];
            else if (std::wstring(args[i]) == L"--user-dir" && i + 1 < argc) s.user = args[++i];
        }
        LocalFree(args);
        if (s.root.empty() || !fs::is_directory(s.root / "Mods")) throw std::runtime_error("Install the launcher beside Vespasian.exe and its Mods folder, or use --data-dir <game directory>.");
        if (s.user.empty()) throw std::runtime_error("Start Vespasian once to choose its user directory, or use --user-dir <user directory>.");
        if (s.user.is_relative()) s.user = s.root / s.user;
        s.root = fs::absolute(s.root).lexically_normal(); s.user = fs::absolute(s.user).lexically_normal();
        s.mods = fs::exists(s.list_path()) ? mod_content::read_list(s.list_path()) : std::vector<std::string>{"Julius", "Augustus", "Vespasian"};
        register_launcher(instance);
        HWND window = CreateWindowExW(0, L"VespasianLauncher", L"Vespasian Launcher", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 980, 680, nullptr, nullptr, instance, &s);
        if (!window) throw std::runtime_error("Cannot create launcher window"); s.scale.client_size(window, 980, 640); ShowWindow(window, show); UpdateWindow(window); SetFocus(s.right_selected ? s.active : s.available);
        MSG msg{}; while (GetMessageW(&msg, nullptr, 0, 0) > 0) { if (!IsDialogMessageW(window, &msg)) { TranslateMessage(&msg); DispatchMessageW(&msg); } } return static_cast<int>(msg.wParam);
    } catch (const std::exception &e) { if (testing) std::cerr << e.what() << '\n'; else MessageBoxW(nullptr, wide(e.what()).c_str(), L"Vespasian Launcher", MB_OK | MB_ICONERROR); return 1; }
}
