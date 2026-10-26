// EduTrack Phase III academic workspace (read-only Windows front end).
// It shares calculations with the tested console modules; it never writes files.
// Compile this file INSTEAD OF gui/WindowsGUI.cpp; each owns its own entry point.
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#include <windows.h>
#include <commctrl.h>

#include "AcademicGroupStorage.h"
#include "AcademicInsights.h"
#include "AcademicProcessing.h"
#include "GroupRankSimulation.h"
#include "ImprovementGoalPlanner.h"
#include "ImprovementSimulator.h"
#include "RecordAlgorithms.h"
#include "RecordStorage.h"
#include "StudentManagement.h"

#include <algorithm>
#include <filesystem>
#include <iomanip>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

namespace {
constexpr wchar_t kClass[] = L"EduTrackPhase3Workspace";
constexpr COLORREF kInk = RGB(36, 49, 69);
constexpr COLORREF kMuted = RGB(78, 92, 108);
constexpr COLORREF kPaper = RGB(255, 255, 255);
constexpr COLORREF kSide = RGB(235, 242, 249);
constexpr COLORREF kAccent = RGB(31, 83, 131);

enum ControlId {
    NavDashboard = 101, NavStudents, NavInsights, NavSimulator, NavGoals,
    NavGroups, NavHelp,
    HeaderTitle = 150, HeaderNote, SelectLabel, StudentChoice, ReloadButton,
    PageTitle = 200, PageDescription, MainOutput, StudentTable, StatusBar,
    SubjectLabel = 300, SubjectChoice, MarkLabel, ProposedMark, SimulateButton,
    GoalModeLabel = 400, GoalMode, GoalTargetLabel, GoalTarget,
    CeilingLabel, Ceilings, PlanButton
};
enum class Page { Dashboard, Students, Insights, Simulator, Goals, Groups, Help };

std::wstring widen(const std::string& input) {
    if (input.empty()) return {};
    int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                     input.data(), static_cast<int>(input.size()), nullptr, 0);
    if (length <= 0) return L"[invalid UTF-8]";
    std::wstring out(static_cast<std::size_t>(length), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, input.data(), static_cast<int>(input.size()), out.data(), length);
    return out;
}
std::wstring fixed(double number) {
    std::wostringstream text;
    text << std::fixed << std::setprecision(2) << number;
    return text.str();
}
std::wstring readText(HWND h) {
    if (!h) return {};
    const int size = GetWindowTextLengthW(h);
    if (size < 0 || size > 4096) return {};
    std::wstring value(static_cast<std::size_t>(size) + 1, L'\0');
    value.resize(static_cast<std::size_t>(GetWindowTextW(h, value.data(), size + 1)));
    return value;
}
std::wstring trim(const std::wstring& s) {
    auto begin = s.find_first_not_of(L" \t\n\r");
    if (begin == std::wstring::npos) return {};
    const auto end = s.find_last_not_of(L" \t\n\r");
    return s.substr(begin, end - begin + 1);
}
bool number(const std::wstring& text, int low, int high, int& result) {
    std::wistringstream in(trim(text));
    long long n = 0;
    wchar_t trailing = 0;
    if (!(in >> n) || (in >> trailing) || n < low || n > high) return false;
    result = static_cast<int>(n);
    return true;
}
// Convert 75.25 -> 7525, without floating-point rounding or extra decimals.
std::optional<int> hundredths(const std::wstring& source) {
    const std::wstring s = trim(source);
    if (s.empty()) return std::nullopt;
    std::size_t dot = s.find(L'.');
    if (dot != std::wstring::npos && s.find(L'.', dot + 1) != std::wstring::npos)
        return std::nullopt;
    const auto whole = s.substr(0, dot);
    const auto decimal = dot == std::wstring::npos ? L"" : s.substr(dot + 1);
    if (whole.empty() || whole.size() > 3 || decimal.size() > 2 ||
        (dot != std::wstring::npos && decimal.empty())) return std::nullopt;
    for (wchar_t c : whole) if (c < L'0' || c > L'9') return std::nullopt;
    for (wchar_t c : decimal) if (c < L'0' || c > L'9') return std::nullopt;
    int n = 0;
    for (wchar_t c : whole) n = n * 10 + (c - L'0');
    n *= 100;
    if (!decimal.empty()) n += (decimal[0] - L'0') * 10;
    if (decimal.size() == 2) n += decimal[1] - L'0';
    return n <= 10000 ? std::optional<int>(n) : std::nullopt;
}
std::optional<std::vector<int>> parseCeilings(const std::wstring& value, std::size_t count) {
    const auto raw = trim(value);
    if (raw.empty()) return std::vector<int>{}; // no restrictions beyond 100
    std::vector<int> ceilings;
    std::size_t start = 0;
    do {
        const std::size_t next = raw.find(L',', start);
        int n = 0;
        if (!number(raw.substr(start, next == std::wstring::npos ? next : next - start),
                    0, 100, n)) return std::nullopt;
        ceilings.push_back(n);
        if (ceilings.size() > count) return std::nullopt;
        if (next == std::wstring::npos) break;
        start = next + 1;
    } while (start <= raw.size());
    if (ceilings.size() != count) return std::nullopt;
    return ceilings;
}

class Workspace {
public:
    bool open(HINSTANCE hinstance, int show) {
        std::string error;
        if (!loadFiles(error)) {
            MessageBoxW(nullptr, widen(error).c_str(), L"EduTrack — file safety check",
                        MB_OK | MB_ICONERROR);
            return false;
        }
        WNDCLASSW wc{};
        wc.lpfnWndProc = &Workspace::procedure;
        wc.hInstance = hinstance;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = GetSysColorBrush(COLOR_WINDOW);
        wc.lpszClassName = kClass;
        if (!RegisterClassW(&wc)) return false;
        window_ = CreateWindowExW(0, kClass, L"EduTrack | Academic Workspace — Phase III",
                  WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, CW_USEDEFAULT, CW_USEDEFAULT,
                  px(1300), px(840), nullptr, nullptr, hinstance, this);
        if (!window_) return false;
        ShowWindow(window_, show);
        UpdateWindow(window_);
        return true;
    }
private:
    StudentManagement records_;
    AcademicGroups groups_;
    HWND window_ = nullptr;
    std::map<int, HWND> controls_;
    HFONT normalFont_ = nullptr, boldFont_ = nullptr, titleFont_ = nullptr;
    HBRUSH whiteBrush_ = nullptr, sideBrush_ = nullptr;
    int dpi_ = 96;
    bool updating_ = false;
    Page page_ = Page::Dashboard;
    std::vector<int> selectedRolls_;
    std::vector<int> tableRolls_;

    int px(int logical) const { return MulDiv(logical, dpi_, 96); }
    static LRESULT CALLBACK procedure(HWND hwnd, UINT message, WPARAM w, LPARAM l) {
        Workspace* app = reinterpret_cast<Workspace*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (message == WM_NCCREATE) {
            auto* create = reinterpret_cast<CREATESTRUCTW*>(l);
            app = static_cast<Workspace*>(create->lpCreateParams);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
        }
        return app ? app->handle(hwnd, message, w, l) : DefWindowProcW(hwnd, message, w, l);
    }
    LRESULT handle(HWND hwnd, UINT message, WPARAM w, LPARAM l) {
        switch (message) {
            case WM_CREATE: {
                window_ = hwnd;
                HDC dc = GetDC(hwnd);
                if (dc) { dpi_ = GetDeviceCaps(dc, LOGPIXELSX); ReleaseDC(hwnd, dc); }
                if (dpi_ < 96) dpi_ = 96;
                if (!makeInterface()) return -1;
                fillStudents();
                showPage(Page::Dashboard);
                return 0;
            }
            case WM_SIZE:
                arrange(LOWORD(l), HIWORD(l));
                InvalidateRect(hwnd, nullptr, TRUE);
                return 0;
            case WM_GETMINMAXINFO: {
                auto* limits = reinterpret_cast<MINMAXINFO*>(l);
                limits->ptMinTrackSize.x = px(1050);
                limits->ptMinTrackSize.y = px(690);
                return 0;
            }
            case WM_ERASEBKGND: {
                HDC dc = reinterpret_cast<HDC>(w);
                RECT rect{};
                GetClientRect(hwnd, &rect);
                FillRect(dc, &rect, whiteBrush_ ? whiteBrush_ : GetSysColorBrush(COLOR_WINDOW));
                rect.right = px(222);
                FillRect(dc, &rect, sideBrush_ ? sideBrush_ : GetSysColorBrush(COLOR_WINDOW));
                return 1;
            }
            case WM_CTLCOLORSTATIC: case WM_CTLCOLOREDIT: {
                HDC dc = reinterpret_cast<HDC>(w);
                HWND child = reinterpret_cast<HWND>(l);
                // Read-only EDIT controls also send WM_CTLCOLORSTATIC on Windows.
                wchar_t klass[16]{};
                GetClassNameW(child, klass, 16);
                if (lstrcmpiW(klass, L"Edit") == 0) {
                    SetBkMode(dc, OPAQUE);
                    SetBkColor(dc, kPaper);
                    SetTextColor(dc, kInk);
                    return reinterpret_cast<LRESULT>(whiteBrush_);
                }
                const int id = GetDlgCtrlID(child);
                const bool sidebar = id >= NavDashboard && id <= NavHelp;
                SetBkMode(dc, OPAQUE);
                SetBkColor(dc, sidebar ? kSide : kPaper);
                SetTextColor(dc, sidebar ? kAccent : (id == PageDescription || id == HeaderNote || id == StatusBar ? kMuted : kInk));
                return reinterpret_cast<LRESULT>(sidebar ? sideBrush_ : whiteBrush_);
            }
            case WM_NOTIFY: {
                auto* hdr = reinterpret_cast<NMHDR*>(l);
                if (hdr && hdr->idFrom == StudentTable && hdr->code == LVN_ITEMCHANGED && !updating_) {
                    auto* item = reinterpret_cast<NMLISTVIEW*>(l);
                    if ((item->uChanged & LVIF_STATE) && (item->uNewState & LVIS_SELECTED) &&
                         item->iItem >= 0 && static_cast<std::size_t>(item->iItem) < tableRolls_.size()) {
                        selectRoll(tableRolls_[static_cast<std::size_t>(item->iItem)]);
                    }
                }
                return 0;
            }
            case WM_COMMAND:
                command(LOWORD(w), HIWORD(w));
                return 0;
            case WM_DESTROY:
                if (normalFont_) DeleteObject(normalFont_);
                if (boldFont_) DeleteObject(boldFont_);
                if (titleFont_) DeleteObject(titleFont_);
                if (whiteBrush_) DeleteObject(whiteBrush_);
                if (sideBrush_) DeleteObject(sideBrush_);
                PostQuitMessage(0);
                return 0;
        }
        return DefWindowProcW(hwnd, message, w, l);
    }
    HWND ctl(int id) const {
        auto it = controls_.find(id);
        return it == controls_.end() ? nullptr : it->second;
    }
    void text(int id, const std::wstring& value) {
        if (HWND h = ctl(id)) SetWindowTextW(h, value.c_str());
    }
    void status(const std::wstring& value) { text(StatusBar, value); }
    void warning(const std::wstring& value) {
        MessageBoxW(window_, value.c_str(), L"EduTrack", MB_OK | MB_ICONWARNING);
        status(value);
    }
    HWND add(int id, const wchar_t* className, const wchar_t* value, DWORD style,
             DWORD extra = 0) {
        HWND h = CreateWindowExW(extra, className, value, WS_CHILD | WS_VISIBLE | style,
                   0, 0, px(100), px(32), window_,
                   reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                   GetModuleHandleW(nullptr), nullptr);
        if (h) {
            controls_[id] = h;
            SendMessageW(h, WM_SETFONT, reinterpret_cast<WPARAM>(normalFont_), TRUE);
        }
        return h;
    }
    void label(int id, const wchar_t* caption, bool title = false) {
        HWND h = add(id, L"STATIC", caption, SS_LEFT | SS_NOPREFIX);
        if (h && title) SendMessageW(h, WM_SETFONT, reinterpret_cast<WPARAM>(boldFont_), TRUE);
    }
    void button(int id, const wchar_t* caption) {
        add(id, L"BUTTON", caption, BS_PUSHBUTTON | WS_TABSTOP);
    }
    void field(int id, const wchar_t* initial = L"") {
        HWND h = add(id, L"EDIT", initial, WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP, WS_EX_CLIENTEDGE);
        if (h) SendMessageW(h, EM_LIMITTEXT, 160, 0);
    }
    void combo(int id) {
        add(id, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP);
    }
    void comboItem(int id, const std::wstring& value) {
        SendMessageW(ctl(id), CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(value.c_str()));
    }
    void makeTable() {
        HWND h = add(StudentTable, WC_LISTVIEWW, L"", LVS_REPORT | LVS_SINGLESEL | WS_TABSTOP,
                     WS_EX_CLIENTEDGE);
        if (!h) return;
        ListView_SetExtendedListViewStyle(h, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
        const std::pair<const wchar_t*, int> columns[] = {
            {L"Roll", 105}, {L"Student", 235}, {L"Group", 155},
            {L"Semester", 98}, {L"Average", 96}, {L"Grade", 80}
        };
        for (int i = 0; i < static_cast<int>(sizeof(columns) / sizeof(columns[0])); ++i) {
            LVCOLUMNW col{};
            col.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
            col.cx = px(columns[i].second);
            col.iSubItem = i;
            col.pszText = const_cast<wchar_t*>(columns[i].first);
            ListView_InsertColumn(h, i, &col);
        }
    }
    bool makeInterface() {
        whiteBrush_ = CreateSolidBrush(kPaper);
        sideBrush_ = CreateSolidBrush(kSide);
        normalFont_ = CreateFontW(-px(16), 0, 0, 0, FW_NORMAL, 0, 0, 0,
                           DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        boldFont_ = CreateFontW(-px(18), 0, 0, 0, FW_SEMIBOLD, 0, 0, 0,
                         DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        titleFont_ = CreateFontW(-px(28), 0, 0, 0, FW_BOLD, 0, 0, 0,
                          DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        label(HeaderTitle, L"EduTrack  |  Phase III");
        SendMessageW(ctl(HeaderTitle), WM_SETFONT, reinterpret_cast<WPARAM>(titleFont_), TRUE);
        label(HeaderNote, L"Read-only academic workspace  |  What-if results never change marks");
        label(SelectLabel, L"Student:");
        combo(StudentChoice);
        button(ReloadButton, L"Reload files");
        const std::pair<int, const wchar_t*> pages[] = {
            {NavDashboard, L"Dashboard"}, {NavStudents, L"Student profiles"},
            {NavInsights, L"Academic insights"}, {NavSimulator, L"What-if simulator"},
            {NavGoals, L"Goal planner"}, {NavGroups, L"Academic groups"},
            {NavHelp, L"Help and safety"}
        };
        for (const auto& item : pages) button(item.first, item.second);
        label(PageTitle, L"", true);
        label(PageDescription, L"");
        HWND output = add(MainOutput, L"EDIT", L"", ES_MULTILINE | ES_READONLY |
                         ES_AUTOVSCROLL | WS_VSCROLL | WS_TABSTOP, WS_EX_CLIENTEDGE);
        if (output) SendMessageW(output, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN,
                                 MAKELPARAM(px(14), px(14)));
        makeTable();
        label(SubjectLabel, L"Subject"); combo(SubjectChoice);
        label(MarkLabel, L"Hypothetical mark (0–100)"); field(ProposedMark);
        button(SimulateButton, L"Calculate result");
        label(GoalModeLabel, L"Goal type"); combo(GoalMode);
        comboItem(GoalMode, L"Target percentage");
        comboItem(GoalMode, L"Target letter grade");
        SendMessageW(ctl(GoalMode), CB_SETCURSEL, 0, 0);
        label(GoalTargetLabel, L"Target (e.g. 75.25)"); field(GoalTarget, L"75");
        label(CeilingLabel, L"Optional subject ceilings (comma-separated)");
        field(Ceilings);
        button(PlanButton, L"Build hypothetical plan");
        label(StatusBar, L"Ready. Read-only; all data is loaded from the current working folder.");
        return whiteBrush_ && sideBrush_ && normalFont_ && boldFont_ && titleFont_ &&
               ctl(MainOutput) && ctl(StudentTable) && ctl(StudentChoice);
    }
    void place(int id, int x, int y, int w, int h) {
        HWND control = ctl(id);
        if (control) MoveWindow(control, x, y, std::max(1, w), std::max(1, h), TRUE);
    }
    void arrange(int w, int h) {
        if (!ctl(HeaderTitle) || w == 0 || h == 0) return;
        const int sidebar = px(222), pad = px(26), top = px(110);
        const int x = sidebar + pad, right = w - pad, content = std::max(px(200), right - x);
        const int bottom = h - px(56);
        place(HeaderTitle, x, px(18), px(280), px(40));
        place(HeaderNote, x, px(63), std::max(px(340), content - px(10)), px(31));
        place(SelectLabel, std::max(x + px(30), right - px(470)), px(18), px(83), px(32));
        place(StudentChoice, std::max(x + px(113), right - px(390)), px(18), px(274), px(240));
        place(ReloadButton, right - px(106), px(18), px(106), px(35));
        const int nav[] = {NavDashboard, NavStudents, NavInsights, NavSimulator,
                           NavGoals, NavGroups, NavHelp};
        for (int i = 0; i < 7; ++i) place(nav[i], px(16), px(105 + 55 * i), px(190), px(42));
        place(PageTitle, x, top, content, px(32));
        place(PageDescription, x, top + px(43), content, px(47));
        const int outputY = top + px(119);
        if (page_ == Page::Students) {
            const int tableWidth = content * 63 / 100;
            place(StudentTable, x, outputY, tableWidth - px(13), bottom - outputY);
            place(MainOutput, x + tableWidth + px(7), outputY,
                  content - tableWidth - px(7), bottom - outputY);
        } else {
            place(StudentTable, x, outputY, content, bottom - outputY);
            int resultY = outputY;
            if (page_ == Page::Simulator) {
                const int first = std::max(px(245), content / 3);
                place(SubjectLabel, x, outputY, first - px(20), px(26));
                place(SubjectChoice, x, outputY + px(30), first - px(20), px(210));
                place(MarkLabel, x + first, outputY, px(236), px(26));
                place(ProposedMark, x + first, outputY + px(30), px(190), px(34));
                place(SimulateButton, x + first + px(208), outputY + px(30),
                      std::max(px(130), content - first - px(208)), px(36));
                resultY += px(95);
            }
            if (page_ == Page::Goals) {
                const int col = (content - px(20)) / 3;
                place(GoalModeLabel, x, outputY, col, px(26));
                place(GoalMode, x, outputY + px(28), col, px(200));
                place(GoalTargetLabel, x + col + px(10), outputY, col, px(26));
                place(GoalTarget, x + col + px(10), outputY + px(28), col, px(34));
                place(CeilingLabel, x + 2 * (col + px(10)), outputY, col, px(40));
                place(Ceilings, x + 2 * (col + px(10)), outputY + px(40), col, px(34));
                place(PlanButton, x, outputY + px(91), px(242), px(38));
                resultY += px(153);
            }
            place(MainOutput, x, resultY, content, bottom - resultY);
        }
        place(StatusBar, x, h - px(39), content, px(28));
    }
    bool loadFiles(std::string& error) {
        namespace fs = std::filesystem;
        error.clear();
        for (const char* base : {"students.txt", "academic_groups.txt"}) {
            for (const char* ext : {".tmp", ".bak"}) {
                std::error_code ec;
                const std::string name = std::string(base) + ext;
                const bool found = fs::exists(name, ec);
                if (ec) { error = "Cannot inspect " + name + ": " + ec.message(); return false; }
                if (found) { error = "Recovery file " + name + " exists. Inspect backups before opening."; return false; }
            }
        }
        StudentManagement newRecords;
        AcademicGroups newGroups;
        for (int which = 0; which < 2; ++which) {
            const char* filename = which == 0 ? "students.txt" : "academic_groups.txt";
            std::error_code ec;
            const bool exists = fs::exists(filename, ec);
            if (ec) { error = "Cannot inspect " + std::string(filename); return false; }
            if (!exists) continue;
            if (which == 0) {
                if (!RecordStorage::load(filename, newRecords, error)) return false;
            } else if (!AcademicGroupStorage::load(filename, newRecords, newGroups, error)) {
                return false;
            }
        }
        if (!newGroups.consistentWith(newRecords)) {
            error = "Group assignments and student records disagree. No file was changed.";
            return false;
        }
        records_ = std::move(newRecords);
        groups_ = std::move(newGroups);
        return true;
    }
    const Student* selected() const {
        const int index = static_cast<int>(SendMessageW(ctl(StudentChoice), CB_GETCURSEL, 0, 0));
        if (index <= 0 || static_cast<std::size_t>(index) > selectedRolls_.size()) return nullptr;
        return records_.findStudent(selectedRolls_[static_cast<std::size_t>(index - 1)]);
    }
    void fillStudents(int desiredRoll = 0) {
        updating_ = true;
        SendMessageW(ctl(StudentChoice), CB_RESETCONTENT, 0, 0);
        comboItem(StudentChoice, L"Choose a student");
        selectedRolls_.clear();
        HWND table = ctl(StudentTable);
        ListView_DeleteAllItems(table);
        tableRolls_.clear();
        int selectIndex = 0;
        auto sorted = RecordAlgorithms::sortByRollNumber(records_.allStudents());
        for (const Student* s : sorted) {
            if (desiredRoll && s->rollNumber() == desiredRoll) selectIndex = static_cast<int>(selectedRolls_.size()) + 1;
            selectedRolls_.push_back(s->rollNumber());
            tableRolls_.push_back(s->rollNumber());
            comboItem(StudentChoice, std::to_wstring(s->rollNumber()) + L"  —  " + widen(s->name()));
            const auto* group = groups_.groupFor(*s);
            const std::wstring cells[] = {
                std::to_wstring(s->rollNumber()), widen(s->name()),
                group ? widen(group->id) : L"Not assigned",
                std::to_wstring(s->semester()), fixed(RecordAlgorithms::percentage(*s)) + L"%",
                widen(AcademicProcessing::grade(RecordAlgorithms::percentage(*s)))
            };
            LVITEMW row{};
            row.mask = LVIF_TEXT;
            row.iItem = static_cast<int>(tableRolls_.size() - 1);
            row.pszText = const_cast<wchar_t*>(cells[0].c_str());
            ListView_InsertItem(table, &row);
            for (int col = 1; col < 6; ++col)
                ListView_SetItemText(table, row.iItem, col, const_cast<wchar_t*>(cells[col].c_str()));
        }
        if (!selectedRolls_.empty() && selectIndex == 0) selectIndex = 1;
        SendMessageW(ctl(StudentChoice), CB_SETCURSEL, selectIndex, 0);
        updating_ = false;
        fillSubjects();
    }
    void selectRoll(int roll) {
        for (std::size_t i = 0; i < selectedRolls_.size(); ++i) {
            if (selectedRolls_[i] != roll) continue;
            SendMessageW(ctl(StudentChoice), CB_SETCURSEL, i + 1, 0);
            fillSubjects();
            renderCurrent();
            return;
        }
    }
    void fillSubjects() {
        SendMessageW(ctl(SubjectChoice), CB_RESETCONTENT, 0, 0);
        const Student* s = selected();
        if (!s) return;
        const AcademicGroup* g = groups_.groupFor(*s);
        for (std::size_t i = 0; i < s->marks().size(); ++i) {
            const std::wstring title = g
                ? widen(g->subjects[i].code + " - " + g->subjects[i].name)
                : L"Subject " + std::to_wstring(i + 1);
            comboItem(SubjectChoice, title);
        }
        SendMessageW(ctl(SubjectChoice), CB_SETCURSEL, 0, 0);
        text(ProposedMark, std::to_wstring(s->marks().front()));
    }
    std::wstring subjectTitle(const Student& s, std::size_t i) const {
        const AcademicGroup* g = groups_.groupFor(s);
        return g ? widen(g->subjects[i].code + " - " + g->subjects[i].name)
                 : L"Subject " + std::to_wstring(i + 1);
    }
    std::wstring profile(const Student* s) const {
        if (!s) return L"No students are loaded. Add records and groups using the Phase-III console application, then reload here.";
        const AcademicGroup* g = groups_.groupFor(*s);
        std::wostringstream out;
        out << L"STUDENT PROFILE\r\n\r\n" << widen(s->name()) << L"  |  Roll " << s->rollNumber()
            << L"\r\nBranch: " << widen(s->branch()) << L"  |  Semester: " << s->semester()
            << L"\r\nAcademic group: " << (g ? widen(g->id) : L"Unassigned") << L"\r\n\r\n"
            << L"Average: " << fixed(RecordAlgorithms::percentage(*s)) << L"%\r\n"
            << L"Total: " << RecordAlgorithms::totalMarks(*s) << L" / "
            << s->marks().size() * 100 << L"\r\n"
            << L"Grade (illustrative reference): "
            << widen(AcademicProcessing::grade(RecordAlgorithms::percentage(*s))) << L"\r\n\r\n"
            << L"SUBJECT MARKS\r\n";
        for (std::size_t i = 0; i < s->marks().size(); ++i)
            out << subjectTitle(*s, i) << L": " << s->marks()[i] << L"/100\r\n";
        out << L"\r\nMarks are equally weighted. The grading reference is not verified for B.Tech.";
        return out.str();
    }
    std::wstring dashboard() const {
        std::wostringstream out;
        const auto& students = records_.allStudents();
        std::size_t assigned = 0;
        for (const Student& s : students) if (groups_.groupFor(s)) ++assigned;
        out << L"ACADEMIC OVERVIEW\r\n\r\n"
            << L"Students loaded: " << students.size() << L"\r\n"
            << L"Defined academic groups: " << groups_.groups().size() << L"\r\n"
            << L"Students with verified assignments: " << assigned << L"\r\n"
            << L"Students without an assigned group: " << students.size() - assigned << L"\r\n";
        const auto average = AcademicProcessing::classAverage(students);
        if (average) out << L"Overall record average: " << fixed(*average) << L"%\r\n";
        out << L"\r\nHOW TO USE THIS WORKSPACE\r\n\r\n"
            << L"1. Choose a student in the upper-right selector.\r\n"
            << L"2. Open Student profiles to see marks and stored subjects.\r\n"
            << L"3. Open Academic insights to compare with assigned peers.\r\n"
            << L"4. Test alternative marks in What-if simulator.\r\n"
            << L"5. Set an achievable percentage or grade in Goal planner.\r\n\r\n"
            << L"This window is read-only. No buttons here save marks, change assignments,\r\n"
            << L"process queues or update the two data files.\r\n\r\n"
            << L"The overall record average is descriptive, NOT an average of a comparable group.";
        return out.str();
    }
    std::wstring insights(const Student* s) const {
        if (!s) return L"Choose a student to inspect group-based performance.";
        const AcademicGroup* g = groups_.groupFor(*s);
        if (!g) return L"This student has no valid academic-group assignment.\r\n\r\n"
                       L"Assign them to a matching named-subject group in the console first.";
        const auto r = AcademicInsights::analyze(records_, groups_, s->rollNumber());
        if (!r) return L"No comparable peers were found in this group. Insights require at least two valid assigned students.";
        std::wostringstream out;
        out << L"COMPARATIVE SUBJECT INSIGHTS\r\n\r\nGroup: " << widen(r->groupId)
            << L"  |  Student: " << widen(s->name()) << L"\r\n"
            << L"Group rank: " << r->groupRank << L" of " << r->groupSize << L"\r\n"
            << L"Student average: " << fixed(r->studentPercentage) << L"%\r\n"
            << L"Peer average (excluding student): " << fixed(r->peerPercentageAverage) << L"%\r\n"
            << L"Number of peers: " << r->peerCount << L"\r\n";
        if (r->peerCount < 3) out << L"CAUTION: A small peer group is not representative of a whole cohort.\r\n";
        out << L"\r\nSUBJECT COMPARISONS (student minus peer average)\r\n\r\n";
        for (const auto& item : r->subjects) {
            out << widen(item.code) << L"  " << widen(item.name) << L"\r\n"
                << L"   Student " << item.mark << L" | Peer average " << fixed(item.peerAverage)
                << L" | Gap " << (item.difference > 0 ? L"+" : L"")
                << fixed(item.difference) << L" marks\r\n";
        }
        if (r->largestDeficit) out << L"\r\nLargest gap below peers: "
            << widen(r->subjects[*r->largestDeficit].name) << L"\r\n";
        if (r->largestLead) out << L"Largest lead over peers: "
            << widen(r->subjects[*r->largestLead].name) << L"\r\n";
        out << L"\r\nThese are observed differences, not causal conclusions or predictions.";
        return out.str();
    }
    std::wstring groupOverview() const {
        std::wostringstream out;
        out << L"GROUP DEFINITIONS AND SUBJECT ORDER\r\n\r\n";
        if (groups_.groups().empty()) out << L"No academic groups have been created yet.\r\n";
        for (const auto& [key, g] : groups_.groups()) {
            std::size_t members = 0;
            for (const Student& student : records_.allStudents())
                if (const AcademicGroup* assigned = groups_.groupFor(student))
                    if (assigned->id == g.id) ++members;
            out << L"[" << widen(key) << L"]  " << widen(g.branch)
                << L"  |  Semester " << g.semester << L"  |  Cohort " << widen(g.cohort)
                << L"  |  Assigned students " << members << L"\r\n";
            for (std::size_t i = 0; i < g.subjects.size(); ++i)
                out << L"    " << i + 1 << L". " << widen(g.subjects[i].code)
                    << L" — " << widen(g.subjects[i].name) << L"\r\n";
            out << L"\r\n";
        }
        out << L"Assignments are managed through console menu options 21–25.\r\n"
            << L"Keep the console CLOSED while this desktop workspace is open.\r\n"
            << L"After making changes, close the console and use Reload files here.\r\n"
            << L"The read-only desktop workspace never modifies saved metadata.";
        return out.str();
    }
    void display(const std::wstring& value) {
        text(MainOutput, value);
        SendMessageW(ctl(MainOutput), EM_SETSEL, 0, 0);
        SendMessageW(ctl(MainOutput), EM_SCROLLCARET, 0, 0);
    }
    void renderCurrent() {
        const Student* s = selected();
        switch (page_) {
            case Page::Dashboard: display(dashboard()); break;
            case Page::Students: display(profile(s)); break;
            case Page::Insights: display(insights(s)); break;
            case Page::Simulator:
                display(s ? L"Select a subject and type a hypothetical mark, then Calculate result.\r\n\r\n"
                    L"If you have a verified comparable group with at least one peer, your group rank\r\n"
                    L"will also be recalculated. The selected student record is never modified."
                    : L"Choose a student first."); break;
            case Page::Goals:
                display(s ? L"Choose a target percentage (e.g. 75.25) or a letter grade (e.g. A+).\r\n\r\n"
                     L"Optionally set one maximum mark per subject in subject order.\r\n"
                     L"A blank ceiling field allows up to 100 in every subject.\r\n"
                     L"The result is a mathematical example, not a learning prediction."
                     : L"Choose a student first."); break;
            case Page::Groups: display(groupOverview()); break;
            case Page::Help:
                display(L"SAFE PHASE-III WORKFLOW\r\n\r\n"
                  L"Create/edit records and groups in the Phase-III console application.\r\n"
                  L"Close the console before launching or reloading this desktop workspace.\r\n"
                  L"No changes are written here. Student records: students.txt; group metadata: academic_groups.txt.\r\n\r\n"
                  L"A .tmp or .bak file may indicate interrupted saving. The workspace blocks loading\r\n"
                  L"until such recovery files are inspected. A corrupted or inconsistent group file\r\n"
                  L"also blocks loading rather than silently dropping assignments.\r\n\r\n"
                  L"Comparative rankings require an explicit group with at least two valid members.\r\n"
                  L"Insights exclude the selected student from peer averages.\r\n"
                  L"Goal suggestions only raise hypothetical marks, up to the given ceilings.\r\n\r\n"
                  L"Grades use an illustrative GEU distance-learning reference, NOT verified B.Tech rules.\r\n"
                  L"No machine learning, GPA or SGPA prediction is performed.\r\n\r\n"
                  L"The older editable Phase-II GUI is a separate program, not the Phase-III group editor.\r\n"
                  L"Do not run old editors concurrently with this application."); break;
        }
    }
    void showPage(Page next) {
        page_ = next;
        const bool sim = next == Page::Simulator, goal = next == Page::Goals;
        ShowWindow(ctl(StudentTable), next == Page::Students ? SW_SHOW : SW_HIDE);
        for (int id : {SubjectLabel, SubjectChoice, MarkLabel, ProposedMark, SimulateButton})
            ShowWindow(ctl(id), sim ? SW_SHOW : SW_HIDE);
        for (int id : {GoalModeLabel, GoalMode, GoalTargetLabel, GoalTarget,
                       CeilingLabel, Ceilings, PlanButton})
            ShowWindow(ctl(id), goal ? SW_SHOW : SW_HIDE);
        const wchar_t* headings[] = {L"Academic dashboard", L"Student profiles",
             L"Academic insight engine", L"Academic improvement simulator",
             L"Improvement goal planner", L"Academic groups and subjects", L"Help and safety"};
        const wchar_t* descriptions[] = {
            L"Overview of available records, assignments and academic-analysis tools.",
            L"Browse stored students and select any row to examine the subject-by-subject record.",
            L"Compare named subjects against verified peers; the selected student is excluded from peer means.",
            L"Explore possible marks, total percentage and comparable-group rank without saving edits.",
            L"Check achievable percentages or letter grades and inspect a bounded hypothetical marks plan.",
            L"Review the explicit groups and subject order used for fair comparisons.",
            L"Understand the read-only workflow, data files, limitations and recovery safeguards."
        };
        const auto i = static_cast<std::size_t>(next);
        text(PageTitle, headings[i]);
        text(PageDescription, descriptions[i]);
        RECT area{}; GetClientRect(window_, &area); arrange(area.right, area.bottom);
        renderCurrent();
        status(L"Read-only workspace — no marks or academic-group files were changed.");
    }
    void simulate() {
        const Student* s = selected();
        if (!s) { warning(L"Choose a student before running a simulation."); return; }
        const int index = static_cast<int>(SendMessageW(ctl(SubjectChoice), CB_GETCURSEL, 0, 0));
        int mark = 0;
        if (index < 0 || !number(readText(ctl(ProposedMark)), 0, 100, mark)) {
            warning(L"Select a subject and enter a whole-number mark from 0 to 100."); return;
        }
        const std::size_t subject = static_cast<std::size_t>(index) + 1;
        const auto base = ImprovementSimulator::simulate(*s, subject, mark);
        if (!base) { warning(L"The selected mark could not be simulated."); return; }
        std::wostringstream out;
        out << L"HYPOTHETICAL MARK CHANGE\r\n\r\n"
            << widen(s->name()) << L"  |  " << subjectTitle(*s, subject - 1) << L"\r\n"
            << L"Current mark: " << base->originalMark << L" / 100\r\n"
            << L"Proposed mark: " << base->proposedMark << L" / 100\r\n"
            << L"Current total: " << base->originalTotal << L"\r\n"
            << L"Proposed total: " << base->projectedTotal << L"\r\n"
            << L"Average: " << fixed(base->originalPercentage) << L"%  ->  "
            << fixed(base->projectedPercentage) << L"%\r\n"
            << L"Illustrative grade: " << widen(base->originalGrade.letter)
            << L"  ->  " << widen(base->projectedGrade.letter) << L"\r\n\r\n";
        const auto ranked = GroupRankSimulation::calculate(records_, groups_,
                          s->rollNumber(), subject, mark);
        if (ranked) {
            out << L"GROUP RANK\r\n" << L"Group: " << widen(ranked->groupId) << L"\r\n"
                << L"Comparable students: " << ranked->comparableStudents << L"\r\n"
                << L"Current rank: " << ranked->currentRank << L"\r\n"
                << L"Proposed rank: " << ranked->proposedRank << L"\r\n";
        } else {
            out << L"No group rank: this student has fewer than one verified comparable peer.\r\n";
        }
        out << L"\r\nThis scenario has NOT updated records, files, queues or undo history.";
        display(out.str()); status(L"Simulation calculated. Stored marks remain unchanged.");
    }
    void plan() {
        const Student* s = selected();
        if (!s) { warning(L"Choose a student before planning a goal."); return; }
        const auto ceilings = parseCeilings(readText(ctl(Ceilings)), s->marks().size());
        if (!ceilings) {
            warning(L"Enter one whole-number ceiling (0-100) per subject, separated by commas, or leave blank.");
            return;
        }
        const int mode = static_cast<int>(SendMessageW(ctl(GoalMode), CB_GETCURSEL, 0, 0));
        std::optional<ImprovementGoalPlanner::Result> result;
        if (mode == 0) {
            const auto target = hundredths(readText(ctl(GoalTarget)));
            if (!target) { warning(L"Enter a target percentage between 0 and 100 with at most two decimals."); return; }
            result = ImprovementGoalPlanner::forPercentage(*s, *target,
                        AcademicProcessing::GradeScale::GeuReference, *ceilings);
        } else {
            std::wstring grade = trim(readText(ctl(GoalTarget)));
            for (wchar_t& c : grade) if (c >= L'a' && c <= L'z') c = static_cast<wchar_t>(c - 32);
            std::string ascii;
            for (wchar_t c : grade) {
                if (c > 127) { warning(L"Grade must use simple Latin letters, e.g. A+."); return; }
                ascii += static_cast<char>(c);
            }
            result = ImprovementGoalPlanner::forGrade(*s, ascii,
                        AcademicProcessing::GradeScale::GeuReference, *ceilings);
        }
        if (!result) { warning(L"Invalid target or ceilings. Ceilings cannot be lower than current marks."); return; }
        std::wostringstream out;
        out << L"HYPOTHETICAL IMPROVEMENT PLAN\r\n\r\n"
            << widen(s->name()) << L"  |  Roll " << s->rollNumber() << L"\r\n";
        switch (result->status) {
            case ImprovementGoalPlanner::Status::AlreadyMet:
                out << L"STATUS: TARGET ALREADY MET\r\n"; break;
            case ImprovementGoalPlanner::Status::Feasible:
                out << L"STATUS: MATHEMATICALLY ACHIEVABLE\r\n"; break;
            case ImprovementGoalPlanner::Status::Unreachable:
                out << L"STATUS: NOT ACHIEVABLE UNDER THE SELECTED CEILINGS\r\n"; break;
        }
        out << L"Current total: " << result->currentTotal << L"\r\n"
            << L"Required total: " << result->requiredTotal << L"\r\n"
            << L"Maximum allowed total: " << result->maximumTotal << L"\r\n"
            << L"Proposed total: " << result->proposedTotal << L"\r\n"
            << L"Current average: " << fixed(result->currentPercentage) << L"%\r\n"
            << L"Hypothetical average: " << fixed(result->proposedPercentage) << L"%\r\n"
            << L"Illustrative grade: " << widen(result->currentGrade.letter)
            << L"  ->  " << widen(result->proposedGrade.letter) << L"\r\n\r\n"
            << L"SUBJECT DISTRIBUTION (one valid numerical example)\r\n";
        for (std::size_t i = 0; i < result->subjects.size(); ++i) {
            const auto& subject = result->subjects[i];
            out << subjectTitle(*s, i) << L"\r\n"
                << L"    Current " << subject.currentMark
                << L" -> Proposed " << subject.proposedMark
                << L" | Limit " << subject.maximumMark << L"\r\n";
        }
        out << L"\r\nThis is not a study-effort estimate, grade prediction, or saved edit."
            << L"\r\nThe grading scale is an illustrative reference, not verified B.Tech policy.";
        display(out.str()); status(L"Hypothetical goal calculated. Stored marks remain unchanged.");
    }
    void reload() {
        const int previous = selected() ? selected()->rollNumber() : 0;
        std::string error;
        if (!loadFiles(error)) { warning(L"Reload refused; existing in-memory records remain intact.\n" + widen(error)); return; }
        fillStudents(previous);
        renderCurrent();
        status(L"Student and group files reloaded successfully. Read-only.");
    }
    void command(int id, int notification) {
        if (id == StudentChoice && notification == CBN_SELCHANGE && !updating_) {
            fillSubjects(); renderCurrent(); return;
        }
        if (id == GoalMode && notification == CBN_SELCHANGE) {
            const bool grade = SendMessageW(ctl(GoalMode), CB_GETCURSEL, 0, 0) == 1;
            text(GoalTargetLabel, grade ? L"Letter grade (e.g. A+)" : L"Target (e.g. 75.25)");
            text(GoalTarget, grade ? L"A" : L"75");
            renderCurrent();
            return;
        }
        if (id == SubjectChoice && notification == CBN_SELCHANGE) {
            const Student* s = selected();
            const int index = static_cast<int>(SendMessageW(ctl(SubjectChoice), CB_GETCURSEL, 0, 0));
            if (s && index >= 0 && static_cast<std::size_t>(index) < s->marks().size())
                text(ProposedMark, std::to_wstring(s->marks()[static_cast<std::size_t>(index)]));
            return;
        }
        if (notification != BN_CLICKED) return;
        if (id >= NavDashboard && id <= NavHelp) {
            showPage(static_cast<Page>(id - NavDashboard)); return;
        }
        switch (id) {
            case ReloadButton: reload(); break;
            case SimulateButton: simulate(); break;
            case PlanButton: plan(); break;
        }
    }
};
} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) {
    // System-DPI aware: Win32 gives pixel sizes and the UI scales its geometry/fonts.
    SetProcessDPIAware();
    INITCOMMONCONTROLSEX controls{};
    controls.dwSize = sizeof(controls);
    controls.dwICC = ICC_LISTVIEW_CLASSES;
    if (!InitCommonControlsEx(&controls)) {
        MessageBoxW(nullptr, L"Unable to initialize Windows list controls.", L"EduTrack", MB_OK | MB_ICONERROR);
        return 1;
    }
    Workspace app;
    if (!app.open(instance, show)) return 1;
    MSG msg{};
    BOOL code = 0;
    while ((code = GetMessageW(&msg, nullptr, 0, 0)) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return code == -1 ? 1 : static_cast<int>(msg.wParam);
}
#else
// Windows GUI only. Include all other source files to compile the portable console.
#endif
