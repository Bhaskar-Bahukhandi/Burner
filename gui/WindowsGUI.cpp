// Windows desktop interface for EduTrack.
// Academic calculations are read-only. Guarded student edits, undo and FIFO queue
// actions commit record and group changes to SQLite after integrity checks.
// The queue and undo history remain in memory.
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
#include "SqliteStorage.h"
#include "StudentManagement.h"
#include "GuardedRecordWorkflow.h"
#include "RequestQueue.h"
#include "StudentFormDialog.h"
#include "GroupFormDialog.h"
#include "SubjectMarksDialog.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <iomanip>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

namespace {
constexpr wchar_t kClass[] = L"EduTrackDesktop";
constexpr wchar_t kChartClass[] = L"EduTrackReportChart";
constexpr COLORREF kInk = RGB(36, 49, 69);
constexpr COLORREF kMuted = RGB(78, 92, 108);
constexpr COLORREF kPaper = RGB(255, 255, 255);
constexpr COLORREF kSide = RGB(235, 242, 249);
constexpr COLORREF kAccent = RGB(31, 83, 131);

enum ControlId {
    NavDashboard = 101, NavStudents, NavQueue, NavReports, NavGrading,
    NavGroups, NavInsights, NavSimulator, NavGoals, NavHelp,
    HeaderTitle = 150, HeaderNote, SelectLabel, StudentChoice, ReloadButton,
    PageTitle = 200, PageDescription, MainOutput, StudentTable, StatusBar, ReportChart,
    SubjectLabel = 300, SubjectChoice, MarkLabel, ProposedMark, SimulateButton,
    GoalModeLabel = 400, GoalMode, GoalTargetLabel, GoalTarget,
    CeilingLabel, Ceilings, CeilingEdit, CeilingReset, PlanButton,
    SearchLabel = 500, SearchInput, SearchMethod, SearchButton, SortLabel, SortChoice,
    GradeLabel = 520, GradeScaleChoice,
    GroupLabel = 540, GroupChoice, GroupAssign, GroupUnassign, GroupCreate,
    StudentAdd = 570, StudentEdit, StudentDelete, StudentUndo,
    StartAdd = 600, StartGroup,
    QueueAdd, QueueUpdate, QueueDelete, QueueNext, QueueUndo,
    MenuReload = 800, MenuExit, MenuHelp, MenuFind, MenuClearSearch, MenuDashboard, MenuStudents,
    MenuQueue, MenuReports, MenuGrading, MenuGroups, MenuInsights,
    MenuSimulator, MenuGoals, MenuUndo
};
enum class Page { Dashboard, Students, Queue, Reports, Grading, Groups, Insights, Simulator, Goals, Help };

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

class Workspace {
public:
    HACCEL accelerators() const { return accelerators_; }
    HWND window() const { return window_; }
    ~Workspace() { if (accelerators_) DestroyAcceleratorTable(accelerators_); }
    bool open(HINSTANCE hinstance, int show) {
        std::string error;
        if (!loadFiles(error)) {
            MessageBoxW(nullptr, widen(error).c_str(), L"EduTrack — SQLite database check",
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
        window_ = CreateWindowExW(0, kClass, L"EduTrack | Student and Academic Workspace",
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
    ChangeHistory history_;
    RequestQueue queue_;
    bool writesBlocked_ = false;
    HWND window_ = nullptr;
    std::map<int, HWND> controls_;
    HFONT normalFont_ = nullptr, boldFont_ = nullptr, titleFont_ = nullptr;
    HBRUSH whiteBrush_ = nullptr, sideBrush_ = nullptr;
    int dpi_ = 96;
    bool updating_ = false;
    Page page_ = Page::Dashboard;
    std::vector<int> selectedRolls_;
    std::vector<int> tableRolls_;
    HACCEL accelerators_ = nullptr;
    AcademicProcessing::GradeScale gradeScale_ = AcademicProcessing::GradeScale::GeuReference;
    int goalCeilingsRoll_ = 0;
    std::vector<int> goalCeilings_; // Optional limits for the selected student only.

    int px(int logical) const { return MulDiv(logical, dpi_, 96); }
    static LRESULT CALLBACK chartProcedure(HWND hwnd, UINT message, WPARAM w, LPARAM l) {
        if (message == WM_PAINT) {
            PAINTSTRUCT paint{};
            HDC dc = BeginPaint(hwnd, &paint);
            auto* app = reinterpret_cast<Workspace*>(
                GetWindowLongPtrW(GetParent(hwnd), GWLP_USERDATA));
            if (app) app->drawReportChart(hwnd, dc);
            EndPaint(hwnd, &paint);
            return 0;
        }
        return DefWindowProcW(hwnd, message, w, l);
    }
    void drawReportChart(HWND hwnd, HDC dc) const {
        RECT bounds{}; GetClientRect(hwnd, &bounds);
        FillRect(dc, &bounds, whiteBrush_ ? whiteBrush_ : GetSysColorBrush(COLOR_WINDOW));
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, kInk);
        HFONT previousFont = reinterpret_cast<HFONT>(SelectObject(dc, boldFont_));
        RECT caption{px(14), px(14), bounds.right - px(12), px(44)};
        DrawTextW(dc, L"Group subject averages", -1, &caption,
                  DT_LEFT | DT_TOP | DT_SINGLELINE | DT_END_ELLIPSIS);
        SelectObject(dc, normalFont_);
        const Student* student = selected();
        const AcademicGroup* group = student ? groups_.groupFor(*student) : nullptr;
        if (!group) {
            RECT message{px(14), px(60), bounds.right - px(14), bounds.bottom - px(14)};
            DrawTextW(dc, L"Select a student in a verified academic group to see subject averages.",
                      -1, &message, DT_WORDBREAK | DT_LEFT);
        } else {
            auto data = AcademicProcessing::groupSubjectResults(records_, groups_, group->id);
            if (!data || data->empty()) {
                RECT message{px(14), px(60), bounds.right - px(14), bounds.bottom - px(14)};
                DrawTextW(dc, L"No comparable subject data available.",
                          -1, &message, DT_WORDBREAK | DT_LEFT);
            } else {
                RECT sub{px(14), px(46), bounds.right - px(12), px(75)};
                const std::wstring groupName = widen(group->id) +
                    L" · marks out of 100 (assigned students only)";
                DrawTextW(dc, groupName.c_str(), -1, &sub,
                          DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
                const int step = px(29), top = px(88);
                const int rows = std::min(static_cast<int>(data->size()),
                         std::max(0, (static_cast<int>(bounds.bottom) - top - px(26)) / step));
                const int barX = px(113), valueWidth = px(53);
                const int barW = std::max(px(15), static_cast<int>(bounds.right) - barX - valueWidth - px(14));
                HBRUSH bar = CreateSolidBrush(kAccent);
                HBRUSH track = CreateSolidBrush(RGB(227, 237, 246));
                for (int i = 0; i < rows; ++i) {
                    const auto& item = (*data)[static_cast<std::size_t>(i)];
                    const int y = top + i * step;
                    std::wstring name = widen(item.code);
                    if (name.size() > 13) name = name.substr(0, 12) + L"...";
                    TextOutW(dc, px(14), y, name.c_str(), static_cast<int>(name.size()));
                    RECT trackRect{barX, y, barX + barW, y + px(17)};
                    FillRect(dc, &trackRect, track);
                    const double avg = std::clamp(item.average, 0.0, 100.0);
                    trackRect.right = barX + static_cast<int>(barW * avg / 100.0);
                    FillRect(dc, &trackRect, bar);
                    const std::wstring mark = fixed(item.average);
                    TextOutW(dc, barX + barW + px(7), y, mark.c_str(),
                             static_cast<int>(mark.size()));
                }
                if (bar) DeleteObject(bar);
                if (track) DeleteObject(track);
                if (rows < static_cast<int>(data->size())) {
                    const wchar_t* hint = L"Other subjects are listed in the report.";
                    TextOutW(dc, px(14), bounds.bottom - px(22), hint, lstrlenW(hint));
                }
            }
        }
        SelectObject(dc, previousFont);
    }
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
                fillGroups();
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
            case WM_CLOSE:
                if (queue_.size() && MessageBoxW(hwnd,
                     L"Pending FIFO requests have not been processed. Exit and discard them?",
                     L"EduTrack - unsaved queue", MB_YESNO | MB_ICONQUESTION) != IDYES)
                    return 0;
                DestroyWindow(hwnd);
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
        if (window_) updateActions();
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
    static void append(HMENU menu, UINT id, const wchar_t* title) {
        AppendMenuW(menu, MF_STRING, id, title);
    }
    void makeMenu() {
        HMENU bar = CreateMenu();
        HMENU file = CreatePopupMenu(), edit = CreatePopupMenu(),
              view = CreatePopupMenu(), help = CreatePopupMenu();
        append(file, MenuReload, L"Reload database\tCtrl+R");
        AppendMenuW(file, MF_SEPARATOR, 0, nullptr);
        append(file, MenuExit, L"Exit");
        append(edit, MenuFind, L"Find student\tCtrl+F");
        append(edit, MenuClearSearch, L"Clear search");
        append(edit, MenuUndo, L"Undo last saved change\tCtrl+Z");
        const std::pair<int, const wchar_t*> views[] = {
            {MenuDashboard, L"Dashboard"}, {MenuStudents, L"Students"},
            {MenuQueue, L"Request Queue"}, {MenuReports, L"Reports"},
            {MenuGrading, L"Grading"}, {MenuGroups, L"Academic Groups"},
            {MenuInsights, L"Insights"}, {MenuSimulator, L"Simulator"},
            {MenuGoals, L"Goal Planner"}
        };
        for (const auto& item : views) append(view, item.first, item.second);
        append(help, MenuHelp, L"How to use EduTrack\tF1");
        AppendMenuW(bar, MF_POPUP, reinterpret_cast<UINT_PTR>(file), L"File");
        AppendMenuW(bar, MF_POPUP, reinterpret_cast<UINT_PTR>(edit), L"Edit");
        AppendMenuW(bar, MF_POPUP, reinterpret_cast<UINT_PTR>(view), L"View");
        AppendMenuW(bar, MF_POPUP, reinterpret_cast<UINT_PTR>(help), L"Help");
        SetMenu(window_, bar);
    }
    void makeTable() {
        HWND h = add(StudentTable, WC_LISTVIEWW, L"", LVS_REPORT | LVS_SINGLESEL | WS_HSCROLL | WS_TABSTOP,
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
        makeMenu();
        ACCEL keys[] = {
            {static_cast<BYTE>(FVIRTKEY | FCONTROL), 'R', MenuReload},
            {static_cast<BYTE>(FVIRTKEY | FCONTROL), 'F', MenuFind},
            {static_cast<BYTE>(FVIRTKEY | FCONTROL), 'Z', MenuUndo},
            {FVIRTKEY, VK_F1, MenuHelp}
        };
        accelerators_ = CreateAcceleratorTableW(keys, 4);
        label(HeaderTitle, L"EduTrack");
        SendMessageW(ctl(HeaderTitle), WM_SETFONT, reinterpret_cast<WPARAM>(titleFont_), TRUE);
        label(HeaderNote, L"Student records  |  Performance insights  |  Academic planning");
        label(SelectLabel, L"Student:");
        combo(StudentChoice);
        button(ReloadButton, L"Reload database");
        button(StartAdd, L"Add student");
        button(StartGroup, L"Create academic group");
        const std::pair<int, const wchar_t*> pages[] = {
            {NavDashboard, L"Dashboard"}, {NavStudents, L"Students"},
            {NavQueue, L"Request Queue"}, {NavReports, L"Reports"},
            {NavGrading, L"Grading"}, {NavGroups, L"Academic Groups"},
            {NavInsights, L"Academic Insights"}, {NavSimulator, L"What-if Simulator"},
            {NavGoals, L"Goal Planner"}, {NavHelp, L"Help"}
        };
        for (const auto& item : pages) button(item.first, item.second);
        label(PageTitle, L"", true);
        label(PageDescription, L"");
        HWND output = add(MainOutput, L"EDIT", L"", ES_MULTILINE | ES_READONLY |
                         ES_AUTOVSCROLL | WS_VSCROLL | WS_TABSTOP, WS_EX_CLIENTEDGE);
        if (output) SendMessageW(output, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN,
                                 MAKELPARAM(px(14), px(14)));
        makeTable();
        WNDCLASSW chartClass{};
        chartClass.lpfnWndProc = &Workspace::chartProcedure;
        chartClass.hInstance = GetModuleHandleW(nullptr);
        chartClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        chartClass.lpszClassName = kChartClass;
        if (!RegisterClassW(&chartClass) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
            return false;
        add(ReportChart, kChartClass, L"", WS_BORDER);
        label(SearchLabel, L"Find roll:"); field(SearchInput);
        combo(SearchMethod); comboItem(SearchMethod, L"Linear"); comboItem(SearchMethod, L"Binary");
        SendMessageW(ctl(SearchMethod), CB_SETCURSEL, 0, 0);
        button(SearchButton, L"Find");
        label(SortLabel, L"Sort:"); combo(SortChoice);
        comboItem(SortChoice, L"Roll number"); comboItem(SortChoice, L"Percentage");
        comboItem(SortChoice, L"Total marks");
        SendMessageW(ctl(SortChoice), CB_SETCURSEL, 0, 0);
        label(GradeLabel, L"Display grading scheme:"); combo(GradeScaleChoice);
        comboItem(GradeScaleChoice, L"GEU reference (illustrative)");
        comboItem(GradeScaleChoice, L"Original project A-F");
        SendMessageW(ctl(GradeScaleChoice), CB_SETCURSEL, 0, 0);
        label(GroupLabel, L"Assign selected student to an existing group:");
        combo(GroupChoice); button(GroupAssign, L"Assign"); button(GroupUnassign, L"Unassign");
        button(GroupCreate, L"Create new group...");
        button(StudentAdd, L"Add student"); button(StudentEdit, L"Edit selected");
        button(StudentDelete, L"Delete selected"); button(StudentUndo, L"Undo");
        button(QueueAdd, L"Queue add"); button(QueueUpdate, L"Queue update");
        button(QueueDelete, L"Queue delete"); button(QueueNext, L"Process next");
        button(QueueUndo, L"Undo change");
        label(SubjectLabel, L"Subject"); combo(SubjectChoice);
        label(MarkLabel, L"Hypothetical mark (0–100)"); field(ProposedMark);
        button(SimulateButton, L"Calculate result");
        label(GoalModeLabel, L"Goal type"); combo(GoalMode);
        comboItem(GoalMode, L"Target percentage");
        comboItem(GoalMode, L"Target letter grade");
        SendMessageW(ctl(GoalMode), CB_SETCURSEL, 0, 0);
        label(GoalTargetLabel, L"Target (e.g. 75.25)"); field(GoalTarget, L"75");
        label(CeilingLabel, L"Subject limits (optional)");
        label(Ceilings, L"No limits (100 per subject)");
        button(CeilingEdit, L"Set subject limits...");
        button(CeilingReset, L"Clear limits");
        button(PlanButton, L"Build hypothetical plan");
        label(StatusBar, L"Ready. Calculations are hypothetical; academic-group assignments can be saved here.");
        return whiteBrush_ && sideBrush_ && normalFont_ && boldFont_ && titleFont_ &&
               ctl(MainOutput) && ctl(StudentTable) && ctl(ReportChart) && ctl(StudentChoice);
    }
    void place(int id, int x, int y, int w, int h) {
        HWND control = ctl(id);
        if (control) MoveWindow(control, x, y, std::max(1, w), std::max(1, h), TRUE);
    }
    void arrange(int w, int h) {
        if (!ctl(HeaderTitle) || w == 0 || h == 0) return;
        const int side = px(222), pad = px(25), top = px(114);
        const int x = side + pad, right = w - pad;
        const int content = std::max(px(200), right - x);
        const int bottom = h - px(52);
        place(HeaderTitle, x, px(13), px(215), px(42));
        place(HeaderNote, x, px(60), std::max(px(300), content - px(8)), px(31));
        place(SelectLabel, right - px(473), px(14), px(65), px(34));
        place(StudentChoice, right - px(408), px(14), px(253), px(280));
        place(ReloadButton, right - px(148), px(14), px(148), px(36));
        const int nav[] = {NavDashboard, NavStudents, NavQueue, NavReports, NavGrading,
                           NavGroups, NavInsights, NavSimulator, NavGoals, NavHelp};
        for (int i = 0; i < 10; ++i)
            place(nav[i], px(14), px(103 + i * 45), px(193), px(36));
        place(PageTitle, x, top, content, px(34));
        place(PageDescription, x, top + px(44), content, px(43));
        const int toolbar = top + px(107);
        int resultY = toolbar;
        if (page_ == Page::Students) {
            const int searchW = std::clamp(content / 5, px(112), px(180));
            const int comboW = px(104);
            place(SearchLabel, x, toolbar + px(5), px(77), px(27));
            place(SearchInput, x + px(78), toolbar, searchW, px(34));
            place(SearchMethod, x + px(86) + searchW, toolbar, comboW, px(220));
            place(SearchButton, x + px(198) + searchW, toolbar, px(72), px(34));
            const int sortX = x + px(290) + searchW;
            place(SortLabel, sortX, toolbar + px(6), px(48), px(24));
            place(SortChoice, sortX + px(50), toolbar,
                  std::max(px(108), right - sortX - px(50)), px(220));
            resultY += px(48);
            const int actionWidth = (content - px(28)) / 4;
            const int actions[] = {StudentAdd, StudentEdit, StudentDelete, StudentUndo};
            for (int i = 0; i < 4; ++i)
                place(actions[i], x + i * (actionWidth + px(9)), resultY,
                      actionWidth, px(36));
            resultY += px(48);
            const int gap = px(12), minProfile = px(205);
            const int preferredTable = std::max(px(605), content * 73 / 100);
            const int tablePixels = std::min(content - minProfile - gap, preferredTable);
            place(StudentTable, x, resultY, tablePixels, bottom - resultY);
            // Keep each heading readable; use the list's horizontal scrollbar
            // if a small window cannot fit the minimum total column width.
            const int columnPixels = std::max(px(660), tablePixels -
                GetSystemMetrics(SM_CXVSCROLL) - px(7));
            const int ratios[] = {12, 27, 20, 14, 15};
            int usedPixels = 0;
            for (int i = 0; i < 5; ++i) {
                const int size = columnPixels * ratios[i] / 100;
                ListView_SetColumnWidth(ctl(StudentTable), i, size);
                usedPixels += size;
            }
            ListView_SetColumnWidth(ctl(StudentTable), 5, columnPixels - usedPixels);
            place(MainOutput, x + tablePixels + gap, resultY,
                  content - tablePixels - gap, bottom - resultY);
        } else {
            place(StudentTable, x, toolbar, content, bottom - toolbar);
            if (page_ == Page::Dashboard) {
                place(StartAdd, x, toolbar, px(190), px(39));
                place(StartGroup, x + px(201), toolbar, px(222), px(39));
                resultY += px(55);
            }
            if (page_ == Page::Queue) {
                const int ids[] = {QueueAdd, QueueUpdate, QueueDelete, QueueNext, QueueUndo};
                const int buttonW = (content - px(36)) / 5;
                for (int i = 0; i < 5; ++i)
                    place(ids[i], x + i * (buttonW + px(9)), toolbar,
                          buttonW, px(36));
                resultY += px(53);
            }
            if (page_ == Page::Grading) {
                place(GradeLabel, x, toolbar + px(5), px(205), px(26));
                place(GradeScaleChoice, x + px(210), toolbar, px(285), px(250));
                resultY += px(54);
            }
            if (page_ == Page::Groups) {
                const int choiceW = std::max(px(190), content - px(560));
                place(GroupLabel, x, toolbar + px(6), px(335), px(29));
                place(GroupChoice, x + px(338), toolbar, choiceW, px(240));
                place(GroupAssign, x + px(347) + choiceW, toolbar, px(96), px(35));
                place(GroupUnassign, x + px(451) + choiceW, toolbar, px(101), px(35));
                place(GroupCreate, x, toolbar + px(46), px(185), px(36));
                resultY += px(101);
            }
            if (page_ == Page::Simulator) {
                const int first = std::max(px(245), content / 3);
                place(SubjectLabel, x, toolbar, first - px(20), px(26));
                place(SubjectChoice, x, toolbar + px(30), first - px(20), px(210));
                place(MarkLabel, x + first, toolbar, px(235), px(26));
                place(ProposedMark, x + first, toolbar + px(30), px(185), px(34));
                place(SimulateButton, x + first + px(197), toolbar + px(30),
                      std::max(px(120), content - first - px(197)), px(36));
                resultY += px(98);
            }
            if (page_ == Page::Goals) {
                const int col = (content - px(20)) / 3;
                place(GoalModeLabel, x, toolbar, col, px(26));
                place(GoalMode, x, toolbar + px(28), col, px(200));
                place(GoalTargetLabel, x + col + px(10), toolbar, col, px(26));
                place(GoalTarget, x + col + px(10), toolbar + px(28), col, px(34));
                place(CeilingLabel, x + 2 * (col + px(10)), toolbar, col, px(26));
                place(Ceilings, x + 2 * (col + px(10)), toolbar + px(28), col, px(28));
                const int limitsX = x + 2 * (col + px(10));
                place(CeilingEdit, limitsX, toolbar + px(60), col * 59 / 100, px(34));
                place(CeilingReset, limitsX + col * 59 / 100 + px(5), toolbar + px(60),
                      col - col * 59 / 100 - px(5), px(34));
                place(PlanButton, x, toolbar + px(109), px(240), px(38));
                resultY += px(166);
            }
            if (page_ == Page::Reports) {
                const int chartWidth = content * 46 / 100;
                place(MainOutput, x, resultY, content - chartWidth - px(10), bottom - resultY);
                place(ReportChart, right - chartWidth, resultY,
                      chartWidth, bottom - resultY);
            } else {
                place(MainOutput, x, resultY, content, bottom - resultY);
            }
        }
        place(StatusBar, x, h - px(36), content, px(27));
    }
    // SQL is the live source of truth. A missing database is an empty project
    // ONLY if no legacy files are waiting for an explicit migration.
    bool loadFiles(std::string& error) {
        namespace fs = std::filesystem;
        std::error_code ec;
        const bool dbExists = fs::exists("edutrack.db", ec);
        if (ec) { error = "Cannot check edutrack.db: " + ec.message(); return false; }
        if (!dbExists) {
            for (const char* legacy : {"students.txt", "academic_groups.txt"}) {
                ec.clear();
                if (fs::exists(legacy, ec) || ec) {
                    error = "Legacy records found; run the SQLite migration tool first: " +
                            std::string(legacy);
                    return false;
                }
            }
            StudentManagement empty;
            AcademicGroups emptyGroups;
            if (!SqliteStorage::save("edutrack.db", empty, emptyGroups, error)) return false;
        }
        StudentManagement newRecords;
        AcademicGroups newGroups;
        if (!SqliteStorage::load("edutrack.db", newRecords, newGroups, error)) return false;
        records_ = std::move(newRecords);
        groups_ = std::move(newGroups);
        history_.clear();
        queue_.clear();
        writesBlocked_ = false;
        return true;
    }
    const Student* selected() const {
        const int index = static_cast<int>(SendMessageW(ctl(StudentChoice), CB_GETCURSEL, 0, 0));
        if (index <= 0 || static_cast<std::size_t>(index) > selectedRolls_.size()) return nullptr;
        return records_.findStudent(selectedRolls_[static_cast<std::size_t>(index - 1)]);
    }
    void fillGroups() {
        SendMessageW(ctl(GroupChoice), CB_RESETCONTENT, 0, 0);
        comboItem(GroupChoice, L"Choose existing group");
        for (const auto& [id, group] : groups_.groups()) {
            (void)group;
            comboItem(GroupChoice, widen(id));
        }
        SendMessageW(ctl(GroupChoice), CB_SETCURSEL, 0, 0);
    }
    void syncSelectedGroup() {
        const Student* s = selected();
        const AcademicGroup* current = s ? groups_.groupFor(*s) : nullptr;
        int index = 0;
        if (current) {
            for (const auto& [key, group] : groups_.groups()) {
                (void)group;
                ++index;
                if (key == current->id) break;
            }
            if (static_cast<std::size_t>(index) > groups_.groups().size()) index = 0;
        }
        SendMessageW(ctl(GroupChoice), CB_SETCURSEL, index, 0);
    }
    // Keep the dropdown and table selection synchronized. A previous version
    // could show student B in the detail pane while highlighting student A.
    void selectTableRoll(int roll) {
        HWND table = ctl(StudentTable);
        if (!table) return;
        updating_ = true; // Suppress re-entrant LVN_ITEMCHANGED notifications.
        ListView_SetItemState(table, -1, 0, LVIS_SELECTED | LVIS_FOCUSED);
        for (std::size_t i = 0; i < tableRolls_.size(); ++i) {
            if (tableRolls_[i] != roll) continue;
            ListView_SetItemState(table, static_cast<int>(i),
                                  LVIS_SELECTED | LVIS_FOCUSED,
                                  LVIS_SELECTED | LVIS_FOCUSED);
            ListView_EnsureVisible(table, static_cast<int>(i), FALSE);
            break;
        }
        updating_ = false;
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
        RecordAlgorithms::StudentView sorted;
        const int sort = static_cast<int>(SendMessageW(ctl(SortChoice), CB_GETCURSEL, 0, 0));
        if (sort == 1) sorted = RecordAlgorithms::sortByPercentage(records_.allStudents());
        else if (sort == 2) sorted = RecordAlgorithms::sortByTotalMarks(records_.allStudents());
        else sorted = RecordAlgorithms::sortByRollNumber(records_.allStudents());
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
                widen(AcademicProcessing::grade(RecordAlgorithms::percentage(*s), gradeScale_))
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
        if (selectIndex > 0) selectTableRoll(selectedRolls_[static_cast<std::size_t>(selectIndex - 1)]);
        fillSubjects();
        syncSelectedGroup();
    }
    void selectRoll(int roll) {
        for (std::size_t i = 0; i < selectedRolls_.size(); ++i) {
            if (selectedRolls_[i] != roll) continue;
            SendMessageW(ctl(StudentChoice), CB_SETCURSEL, i + 1, 0);
            selectTableRoll(roll);
            fillSubjects();
            syncSelectedGroup();
            renderCurrent();
            return;
        }
    }
    void fillSubjects() {
        SendMessageW(ctl(SubjectChoice), CB_RESETCONTENT, 0, 0);
        const Student* s = selected();
        const int roll = s ? s->rollNumber() : 0;
        if (goalCeilingsRoll_ != roll) {
            goalCeilingsRoll_ = roll;
            goalCeilings_.clear();
        }
        updateCeilingSummary();
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
        if (!s) return L"NO STUDENTS YET\r\n\r\nClick Add student above to create your first record.\r\n"
                       L"Use Edit subject marks in the student form to enter marks individually.\r\n"
                       L"To show subject names, first create an academic group and then assign the student.";
        const AcademicGroup* g = groups_.groupFor(*s);
        std::wostringstream out;
        out << L"STUDENT PROFILE\r\n\r\n" << widen(s->name()) << L"  |  Roll " << s->rollNumber()
            << L"\r\nBranch: " << widen(s->branch()) << L"  |  Semester: " << s->semester()
            << L"\r\nAcademic group: " << (g ? widen(g->id) : L"Unassigned") << L"\r\n\r\n"
            << L"Average: " << fixed(RecordAlgorithms::percentage(*s)) << L"%\r\n"
            << L"Total: " << RecordAlgorithms::totalMarks(*s) << L" / "
            << s->marks().size() * 100 << L"\r\n"
            << L"Grade (illustrative reference): "
            << widen(AcademicProcessing::grade(RecordAlgorithms::percentage(*s), gradeScale_)) << L"\r\n\r\n"
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
        if (students.empty()) {
            out << L"\r\nGET STARTED\r\n\r\n"
                << L"1. Click Create academic group to name the subjects (recommended).\r\n"
                << L"2. Click Add student and enter the student details and marks.\r\n"
                << L"3. Open Academic Groups to assign matching students.\r\n"
                << L"4. Open Reports, Insights and Goal Planner after records exist.\r\n\r\n"
                << L"Create and edit operations save to the local SQLite database.\r\n"
                << L"Use fictional records for your initial test.\r\n";
            return out.str();
        }
        out << L"\r\nHOW TO USE THIS WORKSPACE\r\n\r\n"
            << L"1. Choose a student in the upper-right selector.\r\n"
            << L"2. Open Students to see marks and named subjects.\r\n"
            << L"3. Open Academic insights to compare with assigned peers.\r\n"
            << L"4. Test alternative marks in What-if simulator.\r\n"
            << L"5. Set an achievable percentage or grade in Goal planner.\r\n\r\n"
            << L"The analytics pages never save hypothetical marks or grades.\r\n"
            << L"Record edits and group assignments are committed to edutrack.db in SQLite transactions.\r\n\r\n"
            << L"The overall record average is descriptive, NOT an average of a comparable group.";
        return out.str();
    }
    std::wstring reports(const Student* s) const {
        std::wostringstream out;
        const auto& list = records_.allStudents();
        out << L"CLASS REPORT\r\n\r\nStudents: " << list.size() << L"\r\n";
        const auto avg = AcademicProcessing::classAverage(list);
        if (avg) out << L"All-record average (not a comparable-group mean): "
                     << fixed(*avg) << L"%\r\n";
        if (const Student* top = AcademicProcessing::topStudent(list))
            out << L"Highest current percentage: " << widen(top->name()) << L" (roll "
                << top->rollNumber() << L")\r\n";
        if (s) {
            out << L"\r\nSELECTED STUDENT\r\n" << widen(s->name()) << L" (roll "
                << s->rollNumber() << L")\r\n"
                << L"Total: " << RecordAlgorithms::totalMarks(*s) << L" / "
                << (100 * s->marks().size()) << L" | Average: "
                << fixed(RecordAlgorithms::percentage(*s)) << L"%\r\n"
                << L"Illustrative grade: " << widen(AcademicProcessing::grade(
                   RecordAlgorithms::percentage(*s), gradeScale_)) << L"\r\n";
            if (auto rank = RecordAlgorithms::rankOf(list, s->rollNumber()))
                out << L"Overall list rank: " << *rank
                    << L" (not a comparable academic-group rank)\r\n";
        }
        out << L"\r\nCOMPARABLE SUBJECT RESULTS\r\n";
        const AcademicGroup* group = s ? groups_.groupFor(*s) : nullptr;
        if (group) {
            const auto results = AcademicProcessing::groupSubjectResults(
                records_, groups_, group->id);
            if (!results) {
                out << L"Group assignments are inconsistent; results unavailable.\r\n";
            } else if (results->empty()) {
                out << L"No students are assigned to this group.\r\n";
            } else {
                out << L"Group " << widen(group->id) << L" | "
                    << widen(group->branch) << L" | Cohort " << widen(group->cohort)
                    << L"\r\n";
                for (const auto& result : *results)
                    out << widen(result.code) << L" - " << widen(result.name)
                        << L": " << result.students << L" students | Average "
                        << fixed(result.average) << L" | Low " << result.lowest
                        << L" | High " << result.highest << L"\r\n";
            }
        } else {
            out << L"Select a student assigned to an academic group to compare named subjects.\r\n";
        }
        out << L"\r\nUnassigned and unrelated students are not mixed into subject averages.";
        return out.str();
    }
    std::wstring grading() const {
        std::wostringstream out;
        out << L"GRADING REFERENCE (NOT VERIFIED FOR B.TECH)\r\n\r\n"
            << L"Selected display scale: " << widen(AcademicProcessing::scaleName(gradeScale_))
            << L"\r\n\r\nPUBLISHED DISTANCE-LEARNING REFERENCE (ILLUSTRATIVE)\r\n"
            << L"Above 85% : O (10) | 75–85 : A+ (9) | 65–below 75 : A (8)\r\n"
            << L"55–below 65 : B+ (7) | 50–below 55 : B (6)\r\n"
            << L"45–below 50 : C (5) | 40–below 45 : P (4)\r\n"
            << L"Below 40 : F (0)\r\n\r\n"
            << L"The exact 85% boundary is undefined in the cited table; this project maps it to A+.\r\n\r\n"
            << L"ORIGINAL PROJECT SCALE\r\n"
            << L"A: 90+ | B: 80+ | C: 70+ | D: 60+ | F: below 60\r\n\r\n"
            << L"Switching schemes only changes displayed results. It never changes stored marks.\r\n"
            << L"A real university SGPA/grade cannot be inferred from this average alone.";
        return out.str();
    }
    std::wstring queuePage() const {
        std::wostringstream out;
        out << L"FIFO REQUEST QUEUE\r\n\r\n"
            << L"Pending requests: " << queue_.size()
            << L"  |  Changes available to undo: " << history_.undoCount() << L"\r\n\r\n";
        const auto pending = queue_.pending();
        if (pending.empty()) out << L"No queued operations.\r\n";
        for (std::size_t i = 0; i < pending.size(); ++i) {
            const auto& item = pending[i];
            out << i + 1 << L". "
                << (item.type == RequestType::Add ? L"Add" :
                    item.type == RequestType::Update ? L"Update" : L"Delete")
                << L" | Student roll " << item.rollNumber << L"\r\n";
        }
        out << L"\r\nQUEUE RULES\r\n"
            << L"Queue Add/Update opens the student form. Queue Delete uses the selected student.\r\n"
            << L"Process next applies the oldest request first. A rejected invalid request is\r\n"
            << L"removed; an academic-group conflict leaves it pending until resolved.\r\n"
            << L"Undo reverses a successful change only when group assignments remain valid.\r\n"
            << L"Successful changes are committed to SQLite. The queue and undo history\r\n"
            << L"are not stored across reloads or restarts.\r\n\r\n"
            << L"SQLite transactions guard concurrent database writes. Reload stale views before editing.";
        return out.str();
    }

    std::wstring insights(const Student* s) const {
        if (!s) return L"NO STUDENT SELECTED\r\n\r\nAdd a student on the Students page, then assign\r\n"
                       L"a matching academic group with at least one other student to compare results.";
        const AcademicGroup* g = groups_.groupFor(*s);
        if (!g) return L"This student has no valid academic-group assignment.\r\n\r\n"
                       L"Use Academic Groups to assign a matching existing group, or\r\n"
                       L"create a new academic group on the Groups page.";
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
        if (groups_.groups().empty())
            out << L"NO GROUPS YET\r\n\r\nClick Create new group above. Enter an ID, branch,\r\n"
                << L"semester, cohort and ordered subject codes.\r\n";

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
        out << L"Use the controls above to assign or unassign the selected student.\r\n"
            << L"Use Create new group above to define subjects, then assign matching students.\r\n"
            << L"Close the console before using this window; reload after external changes.\r\n"
            << L"Only confirmed academic-group assignments can be saved here.";
        return out.str();
    }
    void display(const std::wstring& value) {
        text(MainOutput, value);
        SendMessageW(ctl(MainOutput), EM_SETSEL, 0, 0);
        SendMessageW(ctl(MainOutput), EM_SCROLLCARET, 0, 0);
    }
    void updateActions() {
        const Student* s = selected();
        const bool hasStudent = s != nullptr;
        const bool canWrite = !writesBlocked_;
        const bool undoAvailable = history_.undoCount() > 0 && canWrite;
        const bool groupSelected = SendMessageW(ctl(GroupChoice), CB_GETCURSEL, 0, 0) > 0;
        const bool isAssigned = hasStudent && groups_.groupFor(*s) != nullptr;
        const std::pair<int, bool> states[] = {
            {StudentAdd, canWrite}, {StudentEdit, hasStudent && canWrite},
            {StudentDelete, hasStudent && canWrite}, {StudentUndo, undoAvailable},
            {QueueAdd, canWrite}, {QueueUpdate, hasStudent && canWrite},
            {QueueDelete, hasStudent && canWrite}, {QueueNext, queue_.size() > 0 && canWrite},
            {QueueUndo, undoAvailable}, {GroupCreate, canWrite},
            {GroupAssign, hasStudent && groupSelected && canWrite},
            {GroupUnassign, isAssigned && canWrite},
            {SimulateButton, hasStudent}, {PlanButton, hasStudent},
            {CeilingEdit, hasStudent}, {CeilingReset, hasStudent && !goalCeilings_.empty()},
            {StartAdd, canWrite}, {StartGroup, canWrite}
        };
        for (const auto& [id, enabled] : states)
            if (HWND h = ctl(id)) EnableWindow(h, enabled ? TRUE : FALSE);
        if (HMENU menu = GetMenu(window_)) {
            EnableMenuItem(menu, MenuUndo, MF_BYCOMMAND |
                           (undoAvailable ? MF_ENABLED : MF_GRAYED));
            DrawMenuBar(window_);
        }
    }
    void renderCurrent() {
        if (HWND chart = ctl(ReportChart)) InvalidateRect(chart, nullptr, TRUE);
        const Student* s = selected();
        switch (page_) {
            case Page::Dashboard: display(dashboard()); break;
            case Page::Students: display(profile(s)); break;
            case Page::Queue: display(queuePage()); break;
            case Page::Reports: display(reports(s)); break;
            case Page::Grading: display(grading()); break;
            case Page::Insights: display(insights(s)); break;
            case Page::Simulator:
                display(s ? L"Select a subject and type a hypothetical mark, then Calculate result.\r\n\r\n"
                    L"If you have a verified comparable group with at least one peer, your group rank\r\n"
                    L"will also be recalculated. The selected student record is never modified."
                    : L"NO STUDENT SELECTED\r\n\r\nAdd a student on the Students page before using the simulator."); break;
            case Page::Goals:
                display(s ? L"Choose a target percentage (e.g. 75.25) or a letter grade (e.g. A+).\r\n\r\n"
                     L"Optionally click Set subject limits to edit labelled per-subject ceilings.\r\n"
                     L"Without limits, each subject can reach 100.\r\n"
                     L"The result is a mathematical example, not a learning prediction."
                     : L"NO STUDENT SELECTED\r\n\r\nAdd a student on the Students page to calculate hypothetical goals."); break;
            case Page::Groups: display(groupOverview()); break;
            case Page::Help:
                display(L"WORKFLOW AND DATA SAFETY\r\n\r\n"
                  L"Students: Add, edit or delete a record using the table and separate form.\r\n"
                  L"Assigned students cannot be renamed, moved to a different semester\r\n"
                  L"or deleted while that would invalidate their academic group.\r\n"
                  L"Unassign first, or keep branch/semester/subject count unchanged.\r\n\r\n"
                  L"Request Queue: queue Add, Update or Delete; process in FIFO order.\r\n"
                  L"Academic-group conflicts leave requests pending. Invalid requests are rejected.\r\n"
                  L"Undo is in the Students page, Queue page or Edit > Undo (Ctrl+Z).\r\n"
                  L"Queue and undo history are only in memory. Reload clears both.\r\n\r\n"
                  L"Academic Groups: create groups and assign/unassign matching students.\r\n"
                  L"Analytics simulations never save marks.\r\n\r\n"
                  L"Every actual record save checks SQLite for external modifications,\r\n"
                  L"and rejects stale database changes. Concurrent writers are transaction-protected\r\n"
                  L"by SQLite write transactions; avoid editing in two windows concurrently.\r\n"
                  L"A failed database verification disables further writes until Reload succeeds.\r\n\r\n"
                  L"Only the illustrative grading scales are available; these are NOT\r\n"
                  L"verified B.Tech grade policies, and no ML or SGPA is calculated."); break;
        }
        updateActions();
    }
    void showPage(Page next) {
        page_ = next;
        const bool sim = next == Page::Simulator, goal = next == Page::Goals;
        const bool list = next == Page::Students, grade = next == Page::Grading,
                   group = next == Page::Groups;
        ShowWindow(ctl(StudentTable), list ? SW_SHOW : SW_HIDE);
        ShowWindow(ctl(StartAdd), next == Page::Dashboard ? SW_SHOW : SW_HIDE);
        ShowWindow(ctl(StartGroup), next == Page::Dashboard ? SW_SHOW : SW_HIDE);
        ShowWindow(ctl(ReportChart), next == Page::Reports ? SW_SHOW : SW_HIDE);
        for (int id : {SearchLabel, SearchInput, SearchMethod, SearchButton, SortLabel, SortChoice,
                       StudentAdd, StudentEdit, StudentDelete, StudentUndo})
            ShowWindow(ctl(id), list ? SW_SHOW : SW_HIDE);
        for (int id : {QueueAdd, QueueUpdate, QueueDelete, QueueNext, QueueUndo})
            ShowWindow(ctl(id), next == Page::Queue ? SW_SHOW : SW_HIDE);
        for (int id : {GradeLabel, GradeScaleChoice})
            ShowWindow(ctl(id), grade ? SW_SHOW : SW_HIDE);
        for (int id : {GroupLabel, GroupChoice, GroupAssign, GroupUnassign, GroupCreate})
            ShowWindow(ctl(id), group ? SW_SHOW : SW_HIDE);
        for (int id : {SubjectLabel, SubjectChoice, MarkLabel, ProposedMark, SimulateButton})
            ShowWindow(ctl(id), sim ? SW_SHOW : SW_HIDE);
        for (int id : {GoalModeLabel, GoalMode, GoalTargetLabel, GoalTarget,
                       CeilingLabel, Ceilings, CeilingEdit, CeilingReset, PlanButton})
            ShowWindow(ctl(id), goal ? SW_SHOW : SW_HIDE);
        const wchar_t* headings[] = {
            L"Academic dashboard", L"Students", L"Request queue", L"Academic reports",
            L"Grading reference", L"Academic groups", L"Academic insights",
            L"What-if simulator", L"Improvement goal planner", L"Help and safety"
        };
        const wchar_t* descriptions[] = {
            L"Class overview and academic analysis tools.",
            L"Manage student records safely, with searchable table and edit form.",
            L"In-memory FIFO requests, guarded processing and undo of saved student changes.",
            L"Selected student, comparable group averages and a subject bar chart.",
            L"View both illustrative grading schemes; select one to change the displayed calculations.",
            L"Create named academic groups, then assign matching student records.",
            L"Subject gaps compared only with valid assigned peers, excluding the selected student.",
            L"Try hypothetical marks, percentages, grades and comparable-group ranks.",
            L"Calculate achievable targets and an optional per-subject ceiling plan.",
            L"Application workflow, SQLite protection, grading and limitations."
        };
        const auto i = static_cast<std::size_t>(next);
        text(PageTitle, headings[i]); text(PageDescription, descriptions[i]);
        const int nav[] = {NavDashboard, NavStudents, NavQueue, NavReports, NavGrading,
                           NavGroups, NavInsights, NavSimulator, NavGoals, NavHelp};
        const wchar_t* names[] = {L"Dashboard",L"Students",L"Request Queue",L"Reports",
                                  L"Grading",L"Academic Groups",L"Academic Insights",
                                  L"What-if Simulator",L"Goal Planner",L"Help"};
        for (std::size_t n = 0; n < 10; ++n)
            text(nav[n], std::wstring(n == i ? L"> " : L"   ") + names[n]);
        RECT rect{}; GetClientRect(window_, &rect); arrange(rect.right, rect.bottom);
        renderCurrent();
        status(L"Record changes are saved after safety checks; calculations never change actual marks.");
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
        const auto base = ImprovementSimulator::simulate(*s, subject, mark, gradeScale_);
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
                          s->rollNumber(), subject, mark, gradeScale_);
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
    void updateCeilingSummary() {
        if (goalCeilings_.empty()) text(Ceilings, L"No limits (100 per subject)");
        else text(Ceilings, std::to_wstring(goalCeilings_.size()) + L" subject limits set");
    }
    void editCeilings() {
        const Student* s = selected();
        if (!s) return;
        const auto initial = goalCeilings_.empty()
            ? std::vector<int>(s->marks().size(), 100) : goalCeilings_;
        const auto result = SubjectMarksDialog::show(window_, GetModuleHandleW(nullptr),
                  dpi_, initial, groups_.groupFor(*s), SubjectMarksDialog::Mode::Ceilings);
        if (!result) return;
        for (std::size_t i = 0; i < s->marks().size(); ++i) {
            if ((*result)[i] < s->marks()[i]) {
                warning(L"A ceiling cannot be below the student's current mark. Check subject " +
                        std::to_wstring(i + 1) + L".");
                return;
            }
        }
        goalCeilingsRoll_ = s->rollNumber();
        goalCeilings_ = *result;
        updateCeilingSummary();
        status(L"Subject limits are hypothetical and are not saved to the database.");
    }
    void clearCeilings() {
        goalCeilings_.clear();
        updateCeilingSummary();
        status(L"Hypothetical subject limits cleared.");
    }
    void plan() {
        const Student* s = selected();
        if (!s) { warning(L"Choose a student before planning a goal."); return; }
        const std::vector<int> ceilings = goalCeilingsRoll_ == s->rollNumber()
            ? goalCeilings_ : std::vector<int>{};
        const int mode = static_cast<int>(SendMessageW(ctl(GoalMode), CB_GETCURSEL, 0, 0));
        std::optional<ImprovementGoalPlanner::Result> result;
        if (mode == 0) {
            const auto target = hundredths(readText(ctl(GoalTarget)));
            if (!target) { warning(L"Enter a target percentage between 0 and 100 with at most two decimals."); return; }
            result = ImprovementGoalPlanner::forPercentage(*s, *target,
                        gradeScale_, ceilings);
        } else {
            std::wstring grade = trim(readText(ctl(GoalTarget)));
            for (wchar_t& c : grade) if (c >= L'a' && c <= L'z') c = static_cast<wchar_t>(c - 32);
            std::string ascii;
            for (wchar_t c : grade) {
                if (c > 127) { warning(L"Grade must use simple Latin letters, e.g. A+."); return; }
                ascii += static_cast<char>(c);
            }
            result = ImprovementGoalPlanner::forGrade(*s, ascii,
                        gradeScale_, ceilings);
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
    void findRecord() {
        int roll = 0;
        if (!number(readText(ctl(SearchInput)), 1, 999999999, roll)) {
            warning(L"Enter a valid roll number to find a student."); return;
        }
        const bool binary = SendMessageW(ctl(SearchMethod), CB_GETCURSEL, 0, 0) == 1;
        const Student* found = binary
            ? RecordAlgorithms::binarySearch(records_.allStudents(), roll)
            : RecordAlgorithms::linearSearch(records_.allStudents(), roll);
        if (!found) { warning(L"No record matches the entered roll number."); return; }
        selectRoll(roll);
        status(std::wstring(binary ? L"Binary" : L"Linear") + L" search found roll " +
               std::to_wstring(roll) + L".");
    }
    bool filesFresh(std::string& error) const {
        error.clear();
        if (writesBlocked_) {
            error = "A previous write requires inspection. Reload the database first.";
            return false;
        }
        // commit() repeats this check after BEGIN IMMEDIATE, closing the
        // read/write race even when two applications are open simultaneously.
        return SqliteStorage::matches("edutrack.db", records_, groups_, error);
    }
    bool commitRecords(StudentManagement& nextRecords, ChangeHistory& nextHistory,
                       RequestQueue& nextQueue, int selectedRoll) {
        std::string error;
        if (!filesFresh(error)) {
            warning(L"Record change refused: " + widen(error)); return false;
        }
        if (!groups_.consistentWith(nextRecords)) {
            warning(L"Change would invalidate an academic-group assignment. Unassign first.");
            return false;
        }
        if (!SqliteStorage::commit("edutrack.db", records_, groups_,
                                   nextRecords, groups_, error)) {
            writesBlocked_ = true;
            warning(L"SQLite commit failed; inspect the database and Reload before further edits. "
                    + widen(error));
            return false;
        }
        // SQLite COMMIT succeeded: never roll back to stale memory on a later
        // verification error. Keep the user-visible records aligned with disk.
        records_ = std::move(nextRecords);
        history_ = std::move(nextHistory);
        queue_ = std::move(nextQueue);
        if (!SqliteStorage::matches("edutrack.db", records_, groups_, error)) {
            writesBlocked_ = true;
            warning(L"SQLite committed but could not be verified. Reload before editing. "
                    + widen(error));
            fillStudents(selectedRoll); renderCurrent(); return false;
        }
        goalCeilings_.clear();
        fillStudents(selectedRoll);
        renderCurrent();
        status(L"Student changes committed to SQLite. Undo/queue are session-only.");
        return true;
    }
    void changeStudent(GuardedRecordWorkflow::Change change) {
        const Student* chosen = selected();
        const bool isAdd = change == GuardedRecordWorkflow::Change::Add;
        const bool isUndo = change == GuardedRecordWorkflow::Change::Undo;
        if (!isAdd && !isUndo && !chosen) { warning(L"Choose a student first."); return; }
        const int roll = chosen ? chosen->rollNumber() : 0;
        std::optional<Student> replacement;
        if (isAdd || change == GuardedRecordWorkflow::Change::Update) {
            replacement = StudentFormDialog::show(window_, GetModuleHandleW(nullptr), dpi_,
                  isAdd ? std::optional<Student>{} : std::optional<Student>{*chosen},
                  !isAdd, isAdd ? nullptr : groups_.groupFor(*chosen));
            if (!replacement) return;
        }
        if (change == GuardedRecordWorkflow::Change::Delete &&
            MessageBoxW(window_, L"Permanently delete the selected student record?\nYou can use Undo during this session.",
                        L"Confirm student deletion", MB_YESNO | MB_ICONWARNING) != IDYES) return;
        StudentManagement nextRecords = records_;
        ChangeHistory nextHistory = history_;
        RequestQueue nextQueue = queue_;
        const auto result = GuardedRecordWorkflow::apply(change, roll, replacement,
                                                         groups_, nextRecords, nextHistory);
        if (result != GuardedRecordWorkflow::Status::Applied) {
            warning(result == GuardedRecordWorkflow::Status::GroupConflict
                ? L"Blocked by an academic-group assignment. Unassign first."
                : result == GuardedRecordWorkflow::Status::NoHistory
                ? L"No successful student change to undo."
                : L"Invalid, duplicate or missing student record; no changes saved.");
            return;
        }
        const int selectRoll = replacement ? replacement->rollNumber() :
                               change == GuardedRecordWorkflow::Change::Delete ? 0 : roll;
        commitRecords(nextRecords, nextHistory, nextQueue, selectRoll);
    }
    void enqueue(RequestType type) {
        const Student* chosen = selected();
        if (type != RequestType::Add && !chosen) { warning(L"Select a student first."); return; }
        std::optional<Student> proposal;
        if (type == RequestType::Add || type == RequestType::Update) {
            proposal = StudentFormDialog::show(window_, GetModuleHandleW(nullptr), dpi_,
                  type == RequestType::Update ? std::optional<Student>{*chosen}
                                              : std::optional<Student>{},
                  type == RequestType::Update,
                  type == RequestType::Update ? groups_.groupFor(*chosen) : nullptr);
            if (!proposal) return;
        }
        if (type == RequestType::Add) queue_.enqueueAdd(*proposal);
        else if (type == RequestType::Update) queue_.enqueueUpdate(*proposal);
        else queue_.enqueueDelete(chosen->rollNumber());
        renderCurrent();
        status(L"Request queued in memory. Process Next applies the oldest request.");
    }
    void processQueued() {
        std::string error;
        if (!filesFresh(error)) { warning(L"Queue processing refused: " + widen(error)); return; }
        StudentManagement nextRecords = records_;
        ChangeHistory nextHistory = history_;
        RequestQueue nextQueue = queue_;
        const auto outcome = GuardedRecordWorkflow::processNext(groups_, nextRecords,
                                                               nextHistory, nextQueue);
        if (outcome == GuardedRecordWorkflow::Status::QueueEmpty) {
            warning(L"No pending requests."); return;
        }
        if (outcome == GuardedRecordWorkflow::Status::GroupConflict) {
            warning(L"Request blocked by academic-group assignment. It remains first in the queue.");
            return;
        }
        if (outcome == GuardedRecordWorkflow::Status::RejectedRemoved) {
            queue_ = std::move(nextQueue);
            renderCurrent();
            status(L"Invalid or duplicate request rejected and removed. Student records unchanged.");
            return;
        }
        const int keep = selected() ? selected()->rollNumber() : 0;
        commitRecords(nextRecords, nextHistory, nextQueue, keep);
    }
    void createGroup() {
        if (writesBlocked_) {
            warning(L"Editing is disabled until the database is checked and reloaded.");
            return;
        }
        auto newGroup = GroupFormDialog::show(window_, GetModuleHandleW(nullptr), dpi_);
        if (!newGroup) return;
        std::string error;
        if (!filesFresh(error)) {
            warning(L"Database changed while editing. Reload before saving. " + widen(error));
            return;
        }
        AcademicGroups proposed = groups_;
        if (!proposed.addGroup(*newGroup) || !proposed.consistentWith(records_)) {
            warning(L"Invalid or duplicate group definition. No database changes were made.");
            return;
        }
        if (!SqliteStorage::commit("edutrack.db", records_, groups_,
                                   records_, proposed, error)) {
            writesBlocked_ = true;
            warning(L"Group creation failed. Inspect and reload the database. " + widen(error));
            return;
        }
        groups_ = std::move(proposed);
        if (!SqliteStorage::matches("edutrack.db", records_, groups_, error)) {
            writesBlocked_ = true;
            warning(L"Group saved but verification failed. Reload before editing. " + widen(error));
            return;
        }
        const int previous = selected() ? selected()->rollNumber() : 0;
        fillGroups();
        fillStudents(previous);
        renderCurrent();
        status(L"Academic group and its subject definitions saved to SQLite.");
    }
    void changeGroup(bool assign) {
        const Student* current = selected();
        if (!current) { warning(L"Choose a student first."); return; }
        const int roll = current->rollNumber();
        std::string desired;
        if (assign) {
            int choice = static_cast<int>(SendMessageW(ctl(GroupChoice), CB_GETCURSEL, 0, 0));
            if (choice <= 0 || static_cast<std::size_t>(choice) > groups_.groups().size()) {
                warning(L"Choose an existing academic group first."); return;
            }
            auto it = groups_.groups().begin();
            std::advance(it, choice - 1);
            desired = it->first;
            if (!groups_.findGroup(desired)) return;
        } else if (!groups_.groupFor(*current)) {
            warning(L"This student is not currently assigned to an academic group."); return;
        }
        if (MessageBoxW(window_, assign
                 ? L"Save this academic-group assignment? Stored student marks will not change."
                 : L"Remove this student's academic-group assignment? Stored marks will not change.",
                 L"Confirm academic group change", MB_YESNO | MB_ICONQUESTION) != IDYES)
            return;
        if (writesBlocked_) { warning(L"Editing disabled until saved files are checked and reloaded."); return; }
        std::string error;
        if (!filesFresh(error)) {
            warning(L"SQLite data changed (or could not be checked). Reload before editing. "
                    + widen(error));
            return;
        }
        AcademicGroups proposed = groups_;
        if ((assign && !proposed.assign(*current, desired)) ||
            (!assign && !proposed.unassign(roll)) ||
            !proposed.consistentWith(records_)) {
            warning(L"Assignment rejected: branch, semester, subject count or identity "
                    L"does not match the group. No files changed.");
            return;
        }
        if (!SqliteStorage::commit("edutrack.db", records_, groups_,
                                   records_, proposed, error)) {
            writesBlocked_ = true;
            warning(L"SQLite assignment commit failed. Reload before editing. " + widen(error));
            return;
        }
        groups_ = std::move(proposed);
        if (!SqliteStorage::matches("edutrack.db", records_, groups_, error)) {
            writesBlocked_ = true;
            warning(L"Assignment committed but verification failed. Reload before editing. "
                    + widen(error));
            fillStudents(roll); renderCurrent(); return;
        }
        fillStudents(roll);
        renderCurrent();
        status(L"Academic-group assignment committed to SQLite. Marks unchanged.");
    }
    void reload() {
        if ((queue_.size() || history_.undoCount()) &&
            MessageBoxW(window_, L"Reloading will clear pending requests and undo history. Continue?",
                 L"Confirm reload", MB_YESNO | MB_ICONQUESTION) != IDYES) return;
        const int previous = selected() ? selected()->rollNumber() : 0;
        std::string error;
        if (!loadFiles(error)) { warning(L"Reload refused; existing in-memory records remain intact.\n" + widen(error)); return; }
        fillGroups();
        fillStudents(previous);
        renderCurrent();
        status(L"Student records and academic groups reloaded from SQLite.");
    }
    void command(int id, int notification) {
        if (id == StudentChoice && notification == CBN_SELCHANGE && !updating_) {
            const Student* student = selected();
            selectTableRoll(student ? student->rollNumber() : 0);
            fillSubjects(); syncSelectedGroup(); renderCurrent(); return;
        }
        if (id == GroupChoice && notification == CBN_SELCHANGE) {
            updateActions(); return;
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
        if (id == GradeScaleChoice && notification == CBN_SELCHANGE) {
            const int choice = static_cast<int>(SendMessageW(ctl(GradeScaleChoice), CB_GETCURSEL, 0, 0));
            gradeScale_ = choice == 1 ? AcademicProcessing::GradeScale::OriginalProject
                                     : AcademicProcessing::GradeScale::GeuReference;
            const int roll = selected() ? selected()->rollNumber() : 0;
            fillStudents(roll); renderCurrent();
            status(L"Display grade scheme changed. Saved student marks remain unchanged.");
            return;
        }
        if (id == SortChoice && notification == CBN_SELCHANGE) {
            const int roll = selected() ? selected()->rollNumber() : 0;
            fillStudents(roll); renderCurrent(); return;
        }
        if (notification != BN_CLICKED && id < MenuReload) return;
        if (id >= NavDashboard && id <= NavHelp) {
            showPage(static_cast<Page>(id - NavDashboard)); return;
        }
        if (id >= MenuDashboard && id <= MenuGoals) {
            showPage(static_cast<Page>(id - MenuDashboard)); return;
        }
        switch (id) {
            case ReloadButton: case MenuReload: reload(); break;
            case SearchButton: findRecord(); break;
            case GroupCreate: case StartGroup: createGroup(); break;
            case StartAdd: changeStudent(GuardedRecordWorkflow::Change::Add); break;
            case GroupAssign: changeGroup(true); break;
            case GroupUnassign: changeGroup(false); break;
            case StudentAdd: changeStudent(GuardedRecordWorkflow::Change::Add); break;
            case StudentEdit: changeStudent(GuardedRecordWorkflow::Change::Update); break;
            case StudentDelete: changeStudent(GuardedRecordWorkflow::Change::Delete); break;
            case StudentUndo: case QueueUndo: case MenuUndo:
                changeStudent(GuardedRecordWorkflow::Change::Undo); break;
            case QueueAdd: enqueue(RequestType::Add); break;
            case QueueUpdate: enqueue(RequestType::Update); break;
            case QueueDelete: enqueue(RequestType::Delete); break;
            case QueueNext: processQueued(); break;
            case SimulateButton: simulate(); break;
            case CeilingEdit: editCeilings(); updateActions(); break;
            case CeilingReset: clearCeilings(); updateActions(); break;
            case PlanButton: plan(); break;
            case MenuHelp: showPage(Page::Help); break;
            case MenuFind: showPage(Page::Students); SetFocus(ctl(SearchInput)); break;
            case MenuClearSearch: text(SearchInput, L""); status(L"Search field cleared."); break;
            case MenuExit: SendMessageW(window_, WM_CLOSE, 0, 0); break;
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
        if (!TranslateAcceleratorW(app.window(), app.accelerators(), &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
    return code == -1 ? 1 : static_cast<int>(msg.wParam);
}
#else
// Windows GUI only. Include all other source files to compile the portable console.
#endif
