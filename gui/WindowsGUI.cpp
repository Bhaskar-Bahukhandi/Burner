// Windows desktop interface for the EduTrack C++ project.
// Record handling, searching, ranking and file storage use the existing modules.
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

#include "AcademicProcessing.h"
#include "ChangeHistory.h"
#include "RecordAlgorithms.h"
#include "RecordStorage.h"
#include "RequestQueue.h"
#include "StudentManagement.h"

#include <cctype>
#include <filesystem>
#include <iomanip>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace {
constexpr wchar_t kWindowClass[] = L"EduTrackMainWindow";
constexpr char kDataFile[] = "students.txt";

enum Id {
    NavStudents = 100, NavQueue, NavReports, NavGrading,
    FieldRoll = 200, FieldName, FieldBranch, FieldSemester, FieldMarks,
    AddStudent = 300, UpdateStudent, DeleteStudent, ClearForm, Undo,
    QueueAdd, QueueUpdate, QueueDelete, QueueNext, QueueAll,
    FindStudent, SearchRoll, SearchMethod, SortChoice, StudentList, QueueList,
    ReportRoll, ReportStudent, ReportClass, ReportOutput,
    GradeChoice, GradeExplanation,
    TitleText, SubtitleText, SummaryText, StatusText,
    FormHeading, FormHelp, RollLabel, NameLabel, BranchLabel, SemesterLabel, MarksLabel,
    RecordsHeading, SearchLabel, SortLabel, ListHint, QueueHeading, QueueHelp,
    ReportsHeading, ReportsHelp, GradingHeading, GradingHelp,
    MenuSave = 600, MenuReload, MenuExit, MenuUndo, MenuClear,
    MenuFind, MenuHelp, MenuAbout, MenuStudents, MenuQueue, MenuReports, MenuGrading
};

enum class Page { Students, Queue, Reports, Grading };
enum class Sort { Rank, Roll, Total };

std::wstring wide(const std::string& text) {
    if (text.empty()) return {};
    const int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                            text.data(), static_cast<int>(text.size()), nullptr, 0);
    if (length <= 0) return L"[text error]";
    std::wstring result(static_cast<std::size_t>(length), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
                        result.data(), length);
    return result;
}

std::string utf8(const std::wstring& text) {
    if (text.empty()) return {};
    const int length = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
                          text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    if (length <= 0) return {};
    std::string result(static_cast<std::size_t>(length), '\0');
    WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(),
                        static_cast<int>(text.size()), result.data(), length, nullptr, nullptr);
    return result;
}

std::wstring textOf(HWND window) {
    if (!window) return {};
    const int size = GetWindowTextLengthW(window);
    if (size < 0 || size > 10000) return {};
    std::wstring value(static_cast<std::size_t>(size) + 1, L'\0');
    const int n = GetWindowTextW(window, value.data(), size + 1);
    value.resize(static_cast<std::size_t>(n));
    return value;
}

bool parseNumber(const std::wstring& text, int min, int max, int& result) {
    std::wistringstream in(text);
    long long value = 0;
    wchar_t extra = 0;
    if (!(in >> value) || (in >> extra) || value < min || value > max) return false;
    result = static_cast<int>(value);
    return true;
}

std::string readName(const std::wstring& value) {
    const auto first = value.find_first_not_of(L" \t\r\n");
    if (first == std::wstring::npos) return {};
    const auto last = value.find_last_not_of(L" \t\r\n");
    for (wchar_t character : value) {
        if (character < 32 || character == 127) return {};
    }
    return utf8(value.substr(first, last - first + 1));
}

bool parseMarks(const std::wstring& text, std::vector<int>& marks) {
    if (text.empty() || text.size() > 256) return false;
    std::size_t start = 0;
    while (start < text.size()) {
        const auto end = text.find(L',', start);
        const std::wstring part = text.substr(start, end == std::wstring::npos
                                                         ? end : end - start);
        int mark = 0;
        if (!parseNumber(part, 0, 100, mark) || marks.size() >= 20) return false;
        marks.push_back(mark);
        if (end == std::wstring::npos) return true;
        start = end + 1;
    }
    return false; // Reject a trailing comma, including whitespace-only fields.
}

std::wstring decimal(double value) {
    std::wostringstream out;
    out << std::fixed << std::setprecision(2) << value;
    return out.str();
}

std::wstring marksText(const std::vector<int>& marks) {
    std::wostringstream out;
    for (std::size_t i = 0; i < marks.size(); ++i) {
        if (i) out << L", ";
        out << marks[i];
    }
    return out.str();
}

class DesktopApp {
public:
    bool open(HINSTANCE instance, int show) {
        std::string error;
        if (!loadAtStartup(error)) {
            MessageBoxW(nullptr, wide(error).c_str(), L"EduTrack - cannot open records", MB_OK | MB_ICONERROR);
            return false;
        }
        WNDCLASSW wc{};
        wc.lpfnWndProc = &DesktopApp::windowProc;
        wc.hInstance = instance;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
        wc.lpszClassName = kWindowClass;
        if (!RegisterClassW(&wc)) return false;
        window_ = CreateWindowExW(0, kWindowClass, L"EduTrack - Student Records",
                    WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
                    CW_USEDEFAULT, CW_USEDEFAULT, 1300, 820,
                    nullptr, nullptr, instance, this);
        if (!window_) return false;
        ShowWindow(window_, show);
        UpdateWindow(window_);
        return true;
    }

    HACCEL accelerators() const { return accelerators_; }
    HWND window() const { return window_; }
    ~DesktopApp() {
        if (accelerators_) DestroyAcceleratorTable(accelerators_);
    }

private:
    StudentManagement records_;
    ChangeHistory history_;
    RequestQueue requests_;
    AcademicProcessing::GradeScale scale_ = AcademicProcessing::GradeScale::GeuReference;
    Page page_ = Page::Students;
    Sort sort_ = Sort::Rank;
    bool dirty_ = false;
    bool refreshing_ = false;
    HWND window_ = nullptr;
    HFONT normalFont_ = nullptr, titleFont_ = nullptr, sectionFont_ = nullptr;
    HACCEL accelerators_ = nullptr;
    HBRUSH headerBrush_ = nullptr, sideBrush_ = nullptr, footerBrush_ = nullptr;
    std::map<int, HWND> controls_;
    std::vector<int> visibleRolls_;

    bool loadAtStartup(std::string& error) {
        namespace fs = std::filesystem;
        for (const char* suffix : {".bak", ".tmp"}) {
            std::error_code ec;
            const std::string filename = std::string(kDataFile) + suffix;
            const bool found = fs::exists(filename, ec);
            if (ec) { error = "Cannot check " + filename + ": " + ec.message(); return false; }
            if (found) { error = "Recovery file " + filename + " exists. Check saved data first."; return false; }
        }
        std::error_code ec;
        const bool found = fs::exists(kDataFile, ec);
        if (ec) { error = "Cannot check students.txt: " + ec.message(); return false; }
        return !found || RecordStorage::load(kDataFile, records_, error);
    }

    static LRESULT CALLBACK windowProc(HWND hwnd, UINT msg, WPARAM w, LPARAM l) {
        DesktopApp* app = reinterpret_cast<DesktopApp*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (msg == WM_NCCREATE) {
            auto* data = reinterpret_cast<CREATESTRUCTW*>(l);
            app = static_cast<DesktopApp*>(data->lpCreateParams);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
        }
        return app ? app->handle(hwnd, msg, w, l) : DefWindowProcW(hwnd, msg, w, l);
    }

    LRESULT handle(HWND hwnd, UINT msg, WPARAM w, LPARAM l) {
        switch (msg) {
            case WM_CREATE:
                window_ = hwnd;
                if (!createInterface()) return -1;
                refreshRecords();
                refreshQueue();
                showPage(Page::Students);
                return 0;
            case WM_SIZE:
                layout(LOWORD(l), HIWORD(l));
                InvalidateRect(hwnd, nullptr, TRUE);
                return 0;
            case WM_ERASEBKGND: {
                HDC dc = reinterpret_cast<HDC>(w);
                RECT area{};
                GetClientRect(hwnd, &area);
                FillRect(dc, &area, GetSysColorBrush(COLOR_WINDOW));
                RECT header{0, 0, area.right, 78};
                RECT sidebar{0, 78, 219, area.bottom - 47};
                RECT footer{0, area.bottom - 47, area.right, area.bottom};
                FillRect(dc, &header, headerBrush_ ? headerBrush_ : GetSysColorBrush(COLOR_WINDOW));
                if (sidebar.bottom > sidebar.top)
                    FillRect(dc, &sidebar, sideBrush_ ? sideBrush_ : GetSysColorBrush(COLOR_WINDOW));
                FillRect(dc, &footer, footerBrush_ ? footerBrush_ : GetSysColorBrush(COLOR_WINDOW));
                return 1;
            }
            case WM_CTLCOLORSTATIC: {
                HDC dc = reinterpret_cast<HDC>(w);
                const int id = GetDlgCtrlID(reinterpret_cast<HWND>(l));
                SetBkMode(dc, TRANSPARENT);
                if (id == TitleText || id == SubtitleText || id == SummaryText) {
                    SetTextColor(dc, RGB(33, 49, 68));
                    return reinterpret_cast<LRESULT>(headerBrush_);
                }
                if (id == StatusText) {
                    SetTextColor(dc, RGB(45, 59, 70));
                    return reinterpret_cast<LRESULT>(footerBrush_);
                }
                return reinterpret_cast<LRESULT>(GetSysColorBrush(COLOR_WINDOW));
            }
            case WM_GETMINMAXINFO: {
                auto* size = reinterpret_cast<MINMAXINFO*>(l);
                size->ptMinTrackSize.x = 1020;
                size->ptMinTrackSize.y = 735;
                return 0;
            }
            case WM_COMMAND:
                onCommand(LOWORD(w), HIWORD(w));
                return 0;
            case WM_NOTIFY: {
                auto* event = reinterpret_cast<NMHDR*>(l);
                if (event && event->idFrom == StudentList && event->code == LVN_ITEMCHANGED && !refreshing_) {
                    auto* changed = reinterpret_cast<NMLISTVIEW*>(l);
                    if ((changed->uChanged & LVIF_STATE) && (changed->uNewState & LVIS_SELECTED)) {
                        selectRecord(changed->iItem);
                    }
                }
                return 0;
            }
            case WM_CLOSE:
                if (confirmExit()) DestroyWindow(hwnd);
                return 0;
            case WM_DESTROY:
                if (normalFont_) DeleteObject(normalFont_);
                if (titleFont_) DeleteObject(titleFont_);
                if (sectionFont_) DeleteObject(sectionFont_);
                if (headerBrush_) DeleteObject(headerBrush_);
                if (sideBrush_) DeleteObject(sideBrush_);
                if (footerBrush_) DeleteObject(footerBrush_);
                PostQuitMessage(0);
                return 0;
        }
        return DefWindowProcW(hwnd, msg, w, l);
    }

    HWND control(int id) const {
        const auto it = controls_.find(id);
        return it == controls_.end() ? nullptr : it->second;
    }
    void setText(int id, const std::wstring& text) {
        if (HWND h = control(id)) SetWindowTextW(h, text.c_str());
    }
    std::wstring get(int id) const { return textOf(control(id)); }

    HWND add(int id, const wchar_t* type, const wchar_t* title, DWORD style,
             DWORD extra = 0) {
        HWND h = CreateWindowExW(extra, type, title, style | WS_CHILD | WS_VISIBLE,
                  0, 0, 100, 28, window_,
                  reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                  GetModuleHandleW(nullptr), nullptr);
        if (h) {
            controls_[id] = h;
            SendMessageW(h, WM_SETFONT, reinterpret_cast<WPARAM>(normalFont_), TRUE);
        }
        return h;
    }
    void label(int id, const wchar_t* text, bool heading = false) {
        HWND h = add(id, L"STATIC", text, SS_LEFT);
        if (h && heading) SendMessageW(h, WM_SETFONT,
                     reinterpret_cast<WPARAM>(sectionFont_), TRUE);
    }
    void button(int id, const wchar_t* caption) {
        add(id, L"BUTTON", caption, BS_PUSHBUTTON | WS_TABSTOP);
    }
    void edit(int id, DWORD style = 0) {
        HWND h = add(id, L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP | style,
                     WS_EX_CLIENTEDGE);
        if (h) SendMessageW(h, EM_LIMITTEXT, 256, 0);
    }
    void combo(int id, const std::vector<const wchar_t*>& values) {
        HWND h = add(id, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP);
        if (!h) return;
        for (const wchar_t* value : values) SendMessageW(h, CB_ADDSTRING, 0,
                                                          reinterpret_cast<LPARAM>(value));
        SendMessageW(h, CB_SETCURSEL, 0, 0);
    }
    void list(int id, const std::vector<std::pair<const wchar_t*, int>>& columns) {
        HWND h = add(id, WC_LISTVIEWW, L"", LVS_REPORT | LVS_SINGLESEL | WS_TABSTOP,
                     WS_EX_CLIENTEDGE);
        if (!h) return;
        ListView_SetExtendedListViewStyle(h, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);
        for (std::size_t i = 0; i < columns.size(); ++i) {
            LVCOLUMNW column{};
            column.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
            column.cx = columns[i].second;
            column.iSubItem = static_cast<int>(i);
            column.pszText = const_cast<wchar_t*>(columns[i].first);
            ListView_InsertColumn(h, static_cast<int>(i), &column);
        }
    }
    static void menuItem(HMENU menu, UINT id, const wchar_t* name) {
        AppendMenuW(menu, MF_STRING, id, name);
    }
    void makeMenu() {
        HMENU bar = CreateMenu();
        HMENU file = CreatePopupMenu(), editMenu = CreatePopupMenu();
        HMENU view = CreatePopupMenu(), help = CreatePopupMenu();
        menuItem(file, MenuSave, L"Save records\tCtrl+S");
        menuItem(file, MenuReload, L"Reload saved records");
        AppendMenuW(file, MF_SEPARATOR, 0, nullptr);
        menuItem(file, MenuExit, L"Exit");
        menuItem(editMenu, MenuUndo, L"Undo last change\tCtrl+Z");
        menuItem(editMenu, MenuClear, L"Clear student form");
        menuItem(editMenu, MenuFind, L"Find student\tCtrl+F");
        menuItem(view, MenuStudents, L"Students");
        menuItem(view, MenuQueue, L"Request queue");
        menuItem(view, MenuReports, L"Reports");
        menuItem(view, MenuGrading, L"Grading rules");
        menuItem(help, MenuHelp, L"How to use EduTrack\tF1");
        menuItem(help, MenuAbout, L"About");
        AppendMenuW(bar, MF_POPUP, reinterpret_cast<UINT_PTR>(file), L"File");
        AppendMenuW(bar, MF_POPUP, reinterpret_cast<UINT_PTR>(editMenu), L"Edit");
        AppendMenuW(bar, MF_POPUP, reinterpret_cast<UINT_PTR>(view), L"View");
        AppendMenuW(bar, MF_POPUP, reinterpret_cast<UINT_PTR>(help), L"Help");
        SetMenu(window_, bar);
        ACCEL keys[] = {
            {static_cast<BYTE>(FVIRTKEY | FCONTROL), 'S', MenuSave},
            {static_cast<BYTE>(FVIRTKEY | FCONTROL), 'Z', MenuUndo},
            {static_cast<BYTE>(FVIRTKEY | FCONTROL), 'F', MenuFind},
            {FVIRTKEY, VK_F1, MenuHelp}
        };
        accelerators_ = CreateAcceleratorTableW(keys, 4);
    }

    bool createInterface() {
        headerBrush_ = CreateSolidBrush(RGB(234, 242, 250));
        sideBrush_ = CreateSolidBrush(RGB(241, 245, 249));
        footerBrush_ = CreateSolidBrush(RGB(241, 245, 249));
        normalFont_ = CreateFontW(-18, 0, 0, 0, FW_NORMAL, 0, 0, 0,
                    DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        titleFont_ = CreateFontW(-29, 0, 0, 0, FW_BOLD, 0, 0, 0,
                    DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        sectionFont_ = CreateFontW(-21, 0, 0, 0, FW_SEMIBOLD, 0, 0, 0,
                    DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        makeMenu();
        label(TitleText, L"EduTrack");
        SendMessageW(control(TitleText), WM_SETFONT,
                     reinterpret_cast<WPARAM>(titleFont_), TRUE);
        label(SubtitleText, L"Student records and results");
        label(SummaryText, L"");
        label(StatusText, L"Ready. Select a section to begin.");
        button(NavStudents, L"Students"); button(NavQueue, L"Request Queue");
        button(NavReports, L"Reports"); button(NavGrading, L"Grading");
        label(FormHeading, L"Student details", true);
        label(FormHelp, L"Enter marks in the same subject order for everyone.");
        label(RollLabel, L"Roll number"); edit(FieldRoll);
        label(NameLabel, L"Student name"); edit(FieldName);
        label(BranchLabel, L"Branch"); edit(FieldBranch);
        label(SemesterLabel, L"Semester (1 to 12)"); edit(FieldSemester);
        label(MarksLabel, L"Marks out of 100 (comma-separated)"); edit(FieldMarks);
        button(AddStudent, L"Add student"); button(UpdateStudent, L"Update");
        button(DeleteStudent, L"Delete"); button(ClearForm, L"Clear form");
        button(QueueAdd, L"Queue add"); button(QueueUpdate, L"Queue update");
        button(QueueDelete, L"Queue delete");
        label(RecordsHeading, L"Student records", true);
        label(SearchLabel, L"Find roll number"); edit(SearchRoll);
        combo(SearchMethod, {L"Linear search", L"Binary search"});
        button(FindStudent, L"Find"); label(SortLabel, L"Sort by");
        combo(SortChoice, {L"Class rank", L"Roll number", L"Total marks"});
        list(StudentList, {{L"Rank", 58}, {L"Roll", 73}, {L"Name", 140},
                           {L"Branch", 108}, {L"Sem", 50}, {L"Total", 58},
                           {L"%", 63}, {L"Grade", 63}, {L"Band", 140}});
        label(ListHint, L"Select a row to edit it. Sorting changes the display only.");
        label(QueueHeading, L"Request queue", true);
        label(QueueHelp, L"Queue changes on the left. Process them in order. Undo reverses completed changes.");
        list(QueueList, {{L"Position", 100}, {L"Action", 160}, {L"Roll number", 155}});
        button(QueueNext, L"Process next"); button(QueueAll, L"Process all");
        button(Undo, L"Undo last change");
        label(ReportsHeading, L"Academic reports", true);
        label(ReportsHelp, L"Enter a roll number for one student, or view the whole class report.");
        edit(ReportRoll); button(ReportStudent, L"Student report");
        button(ReportClass, L"Class report");
        add(ReportOutput, L"EDIT", L"Choose a report to view its details.",
            ES_MULTILINE | ES_READONLY | WS_VSCROLL | ES_AUTOVSCROLL | WS_TABSTOP,
            WS_EX_CLIENTEDGE);
        label(GradingHeading, L"Grading", true);
        label(GradingHelp, L"Choose how EduTrack displays grades. Student marks never change.");
        combo(GradeChoice, {L"GEU published reference (illustrative)", L"Original EduTrack A-F"});
        add(GradeExplanation, L"EDIT", L"", ES_MULTILINE | ES_READONLY |
             WS_VSCROLL | ES_AUTOVSCROLL | WS_TABSTOP, WS_EX_CLIENTEDGE);
        updateGradeText();
        setSummary();
        return headerBrush_ && sideBrush_ && footerBrush_ && normalFont_ &&
               titleFont_ && sectionFont_ && control(StudentList) &&
               control(QueueList) && control(ReportOutput) && control(GradeExplanation);
    }

    void place(int id, int x, int y, int width, int height) {
        if (HWND h = control(id)) MoveWindow(h, x, y, width > 1 ? width : 1,
                                           height > 1 ? height : 1, TRUE);
    }

    void layout(int width, int height) {
        if (!control(TitleText) || width == 0 || height == 0) return;
        const int navX = 20, navW = 190;
        const int left = 239, top = 96;
        const int formW = 320;
        const int rightX = left + formW + 27;
        const int rightW = width - rightX - 26;
        const int bodyBottom = height - 56;
        place(TitleText, 24, 18, 260, 39);
        place(SubtitleText, 248, 26, width - 660, 28);
        place(SummaryText, width - 385, 30, 360, 28);
        place(StatusText, 23, height - 36, width - 46, 29);
        place(NavStudents, navX, 102, navW, 44);
        place(NavQueue, navX, 156, navW, 44);
        place(NavReports, navX, 210, navW, 44);
        place(NavGrading, navX, 264, navW, 44);

        place(FormHeading, left, top, formW, 34);
        place(FormHelp, left, top + 41, formW, 42);
        const int formLabels[] = {RollLabel, NameLabel, BranchLabel, SemesterLabel, MarksLabel};
        const int formFields[] = {FieldRoll, FieldName, FieldBranch, FieldSemester, FieldMarks};
        for (int i = 0; i < 5; ++i) {
            place(formLabels[i], left, top + 96 + i * 59, formW, 24);
            place(formFields[i], left, top + 119 + i * 59, formW, 32);
        }
        const int half = (formW - 10) / 2;
        place(AddStudent, left, top + 413, half, 39);
        place(UpdateStudent, left + half + 10, top + 413, half, 39);
        place(DeleteStudent, left, top + 461, half, 39);
        place(ClearForm, left + half + 10, top + 461, half, 39);
        place(QueueAdd, left, top + 413, formW, 39);
        place(QueueUpdate, left, top + 461, half, 39);
        place(QueueDelete, left + half + 10, top + 461, half, 39);

        place(RecordsHeading, rightX, top, rightW, 34);
        place(SearchLabel, rightX, top + 51, 144, 26);
        const int searchY = top + 77;
        const int searchWidth = rightW / 3;
        place(SearchRoll, rightX, searchY, searchWidth, 34);
        place(SearchMethod, rightX + searchWidth + 9, searchY,
              rightW - searchWidth - 118, 250);
        place(FindStudent, rightX + rightW - 100, searchY, 100, 35);
        place(SortLabel, rightX, top + 125, 73, 30);
        place(SortChoice, rightX + 78, top + 121, 210, 220);
        place(StudentList, rightX, top + 171, rightW,
              bodyBottom - (top + 171) - 42);
        place(ListHint, rightX, bodyBottom - 38, rightW, 38);

        place(QueueHeading, rightX, top, rightW, 34);
        place(QueueHelp, rightX, top + 44, rightW, 62);
        place(QueueList, rightX, top + 117, rightW,
              bodyBottom - (top + 117) - 51);
        const int qw = (rightW - 20) / 3;
        place(QueueNext, rightX, bodyBottom - 41, qw, 38);
        place(QueueAll, rightX + qw + 10, bodyBottom - 41, qw, 38);
        place(Undo, rightX + 2 * (qw + 10), bodyBottom - 41, qw, 38);

        place(ReportsHeading, left, top, width - left - 27, 35);
        place(ReportsHelp, left, top + 44, width - left - 27, 25);
        place(ReportRoll, left, top + 85, 170, 34);
        place(ReportStudent, left + 182, top + 83, 178, 38);
        place(ReportClass, left + 372, top + 83, 178, 38);
        place(ReportOutput, left, top + 143, width - left - 27,
              bodyBottom - (top + 143));

        place(GradingHeading, left, top, width - left - 27, 35);
        place(GradingHelp, left, top + 44, width - left - 27, 26);
        place(GradeChoice, left, top + 89, 365, 230);
        place(GradeExplanation, left, top + 147, width - left - 27,
              bodyBottom - (top + 147));
    }

    void showPage(Page page) {
        page_ = page;
        const bool form = page == Page::Students || page == Page::Queue;
        const int shared[] = {FormHeading, FormHelp, RollLabel, NameLabel, BranchLabel,
               SemesterLabel, MarksLabel, FieldRoll, FieldName, FieldBranch,
               FieldSemester, FieldMarks};
        for (int id : shared) ShowWindow(control(id), form ? SW_SHOW : SW_HIDE);
        const int records[] = {AddStudent, UpdateStudent, DeleteStudent, ClearForm,
               RecordsHeading, SearchLabel, SearchRoll, SearchMethod, FindStudent,
               SortLabel, SortChoice, StudentList, ListHint};
        for (int id : records) ShowWindow(control(id), page == Page::Students ? SW_SHOW : SW_HIDE);
        const int queue[] = {QueueAdd, QueueUpdate, QueueDelete, QueueHeading,
               QueueHelp, QueueList, QueueNext, QueueAll, Undo};
        for (int id : queue) ShowWindow(control(id), page == Page::Queue ? SW_SHOW : SW_HIDE);
        const int reports[] = {ReportsHeading, ReportsHelp, ReportRoll,
               ReportStudent, ReportClass, ReportOutput};
        for (int id : reports) ShowWindow(control(id), page == Page::Reports ? SW_SHOW : SW_HIDE);
        const int grading[] = {GradingHeading, GradingHelp, GradeChoice, GradeExplanation};
        for (int id : grading) ShowWindow(control(id), page == Page::Grading ? SW_SHOW : SW_HIDE);
        setText(NavStudents, page == Page::Students ? L"> Students" : L"Students");
        setText(NavQueue, page == Page::Queue ? L"> Request Queue" : L"Request Queue");
        setText(NavReports, page == Page::Reports ? L"> Reports" : L"Reports");
        setText(NavGrading, page == Page::Grading ? L"> Grading" : L"Grading");
        setText(FormHeading, page == Page::Queue ? L"Request details" : L"Student details");
        setText(FormHelp, page == Page::Queue
                ? L"Enter the record to queue. A delete only needs the roll number."
                : L"Marks use the same subject order for every student.");
        RECT rect{};
        GetClientRect(window_, &rect);
        layout(rect.right, rect.bottom);
    }

    void status(const std::wstring& value) { setText(StatusText, value); }
    void error(const std::wstring& message) {
        MessageBoxW(window_, message.c_str(), L"EduTrack", MB_OK | MB_ICONWARNING);
        status(message);
    }
    bool ask(const std::wstring& message) {
        return MessageBoxW(window_, message.c_str(), L"EduTrack", MB_ICONQUESTION | MB_YESNO) == IDYES;
    }
    void setSummary() {
        setText(SummaryText, std::to_wstring(records_.allStudents().size()) + L" students  |  " +
                   std::to_wstring(requests_.size()) + L" queued  |  " +
                   (dirty_ ? L"Unsaved changes" : L"No unsaved edits"));
        if (HWND undo = control(Undo)) EnableWindow(undo, history_.undoCount() != 0);
        if (HWND next = control(QueueNext)) EnableWindow(next, requests_.size() != 0);
        if (HWND all = control(QueueAll)) EnableWindow(all, requests_.size() != 0);
    }

    bool rollFrom(int field, int& value) {
        if (!parseNumber(get(field), 1, 999999999, value)) {
            error(L"Enter a roll number between 1 and 999999999.");
            SetFocus(control(field));
            return false;
        }
        return true;
    }
    std::optional<Student> studentFromForm() {
        int roll = 0, semester = 0;
        if (!rollFrom(FieldRoll, roll)) return std::nullopt;
        const std::string name = readName(get(FieldName));
        const std::string branch = readName(get(FieldBranch));
        if (name.empty() || name.size() > 80 || branch.empty() || branch.size() > 60) {
            error(L"Name and branch cannot be empty. Maximum: 80/60 UTF-8 bytes.");
            return std::nullopt;
        }
        if (!parseNumber(get(FieldSemester), 1, 12, semester)) {
            error(L"Semester must be a whole number from 1 to 12.");
            return std::nullopt;
        }
        std::vector<int> marks;
        if (!parseMarks(get(FieldMarks), marks)) {
            error(L"Enter 1 to 20 whole-number marks (0 to 100), separated by commas.\n"
                  L"Example: 80, 72, 91. Blank values and extra commas are not allowed.");
            return std::nullopt;
        }
        return Student(roll, name, branch, semester, std::move(marks));
    }
    void clearForm() {
        for (int id : {FieldRoll, FieldName, FieldBranch, FieldSemester, FieldMarks})
            setText(id, L"");
        setText(ReportRoll, L"");
        if (page_ == Page::Students) SetFocus(control(FieldRoll));
        status(L"Form cleared. Student records were not changed.");
    }
    void fillForm(const Student& student) {
        setText(FieldRoll, std::to_wstring(student.rollNumber()));
        setText(FieldName, wide(student.name()));
        setText(FieldBranch, wide(student.branch()));
        setText(FieldSemester, std::to_wstring(student.semester()));
        setText(FieldMarks, marksText(student.marks()));
        setText(ReportRoll, std::to_wstring(student.rollNumber()));
    }

    static void insertRow(HWND table, int index, const std::vector<std::wstring>& cells) {
        LVITEMW row{};
        row.mask = LVIF_TEXT;
        row.iItem = index;
        row.pszText = const_cast<wchar_t*>(cells[0].c_str());
        ListView_InsertItem(table, &row);
        for (int i = 1; i < static_cast<int>(cells.size()); ++i)
            ListView_SetItemText(table, index, i,
                                 const_cast<wchar_t*>(cells[static_cast<std::size_t>(i)].c_str()));
    }
    void refreshRecords() {
        HWND table = control(StudentList);
        if (!table) return;
        refreshing_ = true;
        SendMessageW(table, WM_SETREDRAW, FALSE, 0);
        ListView_DeleteAllItems(table);
        visibleRolls_.clear();
        const auto& students = records_.allStudents();
        const auto ranked = RecordAlgorithms::rankList(students);
        std::map<int, std::size_t> ranks;
        for (const auto& entry : ranked) ranks[entry.student->rollNumber()] = entry.rank;
        RecordAlgorithms::StudentView view;
        if (sort_ == Sort::Roll) view = RecordAlgorithms::sortByRollNumber(students);
        else if (sort_ == Sort::Total) view = RecordAlgorithms::sortByTotalMarks(students);
        else for (const auto& entry : ranked) view.push_back(entry.student);
        for (const Student* student : view) {
            const int roll = student->rollNumber();
            const double pct = RecordAlgorithms::percentage(*student);
            visibleRolls_.push_back(roll);
            insertRow(table, static_cast<int>(visibleRolls_.size() - 1), {
                std::to_wstring(ranks[roll]), std::to_wstring(roll), wide(student->name()),
                wide(student->branch()), std::to_wstring(student->semester()),
                std::to_wstring(RecordAlgorithms::totalMarks(*student)), decimal(pct),
                wide(AcademicProcessing::grade(pct, scale_)),
                wide(AcademicProcessing::category(pct, scale_))
            });
        }
        SendMessageW(table, WM_SETREDRAW, TRUE, 0);
        InvalidateRect(table, nullptr, TRUE);
        refreshing_ = false;
        setSummary();
    }
    void refreshQueue() {
        HWND table = control(QueueList);
        if (!table) return;
        ListView_DeleteAllItems(table);
        auto items = requests_.pending();
        for (std::size_t i = 0; i < items.size(); ++i) {
            const wchar_t* action = items[i].type == RequestType::Add ? L"Add" :
                                    items[i].type == RequestType::Update ? L"Update" : L"Delete";
            insertRow(table, static_cast<int>(i), {std::to_wstring(i + 1), action,
                                                   std::to_wstring(items[i].rollNumber)});
        }
        setSummary();
    }
    void selectRecord(int index) {
        if (index < 0 || static_cast<std::size_t>(index) >= visibleRolls_.size()) return;
        const Student* student = records_.findStudent(visibleRolls_[static_cast<std::size_t>(index)]);
        if (student) {
            fillForm(*student);
            status(L"Student selected. You can edit details or open Reports.");
        }
    }
    void findRecord() {
        int roll = 0;
        if (!rollFrom(SearchRoll, roll)) return;
        const bool binary = SendMessageW(control(SearchMethod), CB_GETCURSEL, 0, 0) == 1;
        const Student* found = binary
            ? RecordAlgorithms::binarySearch(records_.allStudents(), roll)
            : RecordAlgorithms::linearSearch(records_.allStudents(), roll);
        if (!found) {
            ListView_SetItemState(control(StudentList), -1, 0, LVIS_SELECTED);
            error(L"No student found with that roll number.");
            return;
        }
        fillForm(*found);
        for (std::size_t i = 0; i < visibleRolls_.size(); ++i) {
            if (visibleRolls_[i] != roll) continue;
            ListView_SetItemState(control(StudentList), static_cast<int>(i),
                                  LVIS_SELECTED | LVIS_FOCUSED,
                                  LVIS_SELECTED | LVIS_FOCUSED);
            ListView_EnsureVisible(control(StudentList), static_cast<int>(i), FALSE);
            break;
        }
        status(std::wstring(binary ? L"Binary" : L"Linear") + L" search found roll " +
               std::to_wstring(roll) + L".");
    }

    void afterEdit(const std::wstring& message) {
        dirty_ = true;
        refreshRecords();
        refreshQueue();
        setText(ReportOutput, L"Records changed. Choose a report to view the latest results.");
        status(message);
    }
    void studentReport() {
        int roll = 0;
        if (!rollFrom(ReportRoll, roll)) return;
        const Student* student = records_.findStudent(roll);
        if (!student) { error(L"No student found with that roll number."); return; }
        const double pct = RecordAlgorithms::percentage(*student);
        const auto grade = AcademicProcessing::gradeInfo(pct, scale_);
        std::wostringstream out;
        out << L"STUDENT REPORT\r\n\r\n"
            << L"Name: " << wide(student->name()) << L"\r\n"
            << L"Roll number: " << roll << L"\r\n"
            << L"Branch: " << wide(student->branch()) << L"\r\n"
            << L"Semester: " << student->semester() << L"\r\n\r\n"
            << L"Subject marks: " << marksText(student->marks()) << L"\r\n"
            << L"Total marks: " << RecordAlgorithms::totalMarks(*student) << L" / "
            << student->marks().size() * 100 << L"\r\n"
            << L"Percentage: " << decimal(pct) << L"%\r\n"
            << L"Grade: " << wide(grade.letter) << L" (" << wide(grade.rating) << L")\r\n";
        if (grade.points >= 0) out << L"Reference grade points: " << grade.points << L"\r\n";
        out << L"Performance band: " << wide(AcademicProcessing::category(pct, scale_))
            << L"\r\n\r\n";
        auto rank = RecordAlgorithms::rankOf(records_.allStudents(), roll);
        if (rank) out << L"Class rank: " << *rank << L"\r\n";
        auto average = AcademicProcessing::classAverage(records_.allStudents());
        if (average) out << L"Class average: " << decimal(*average) << L"%\r\n"
                         << L"Difference: " << decimal(pct - *average) << L" percentage points\r\n";
        out << L"\r\nGrading used: " << wide(AcademicProcessing::scaleName(scale_)) << L"\r\n"
            << L"Subject labels are based on mark position, not stored subject names.";
        setText(ReportOutput, out.str());
        status(L"Student report ready for roll " + std::to_wstring(roll) + L".");
    }
    void classReport() {
        const auto& students = records_.allStudents();
        const auto average = AcademicProcessing::classAverage(students);
        if (!average) { error(L"Add student records before opening the class report."); return; }
        const Student* top = AcademicProcessing::topStudent(students);
        std::map<std::string, int> bands;
        for (const Student& student : students)
            ++bands[AcademicProcessing::category(RecordAlgorithms::percentage(student), scale_)];
        std::wostringstream out;
        out << L"CLASS REPORT\r\n\r\n"
            << L"Students: " << students.size() << L"\r\n"
            << L"Class average: " << decimal(*average) << L"%\r\n"
            << L"Top student: " << wide(top->name()) << L" (roll " << top->rollNumber()
            << L")\r\n\r\nPERFORMANCE BANDS\r\n"
            << L"Excellent: " << bands["Excellent"] << L"\r\n"
            << L"Good: " << bands["Good"] << L"\r\n"
            << L"Average: " << bands["Average"] << L"\r\n"
            << L"Needs Improvement: " << bands["Needs Improvement"] << L"\r\n\r\n"
            << L"SUBJECT-WISE MARKS\r\n";
        bool mixed = false;
        for (const Student& student : students)
            if (student.marks().size() != students.front().marks().size()) mixed = true;
        if (mixed) out << L"Note: Not all students have the same number of subjects.\r\n"
                       << L"Each average includes only students with a mark at that position.\r\n\r\n";
        for (const auto& subject : AcademicProcessing::subjectResults(students))
            out << L"Subject " << subject.number << L" - " << subject.students
                << (subject.students == 1 ? L" student" : L" students")
                << L" | Average " << decimal(subject.average)
                << L" | Min " << subject.lowest << L" | Max " << subject.highest << L"\r\n";
        out << L"\r\nGrading used: " << wide(AcademicProcessing::scaleName(scale_));
        setText(ReportOutput, out.str());
        status(L"Class report ready.");
    }
    void updateGradeText() {
        std::wstring explanation =
            L"GEU PUBLISHED REFERENCE (ILLUSTRATIVE)\r\n\r\n"
            L"Above 85%: O (10 points) - Outstanding\r\n"
            L"75% to 85%: A+ (9 points) - Excellent\r\n"
            L"65% to below 75%: A (8 points) - Very Good\r\n"
            L"55% to below 65%: B+ (7 points) - Good\r\n"
            L"50% to below 55%: B (6 points) - Above Average\r\n"
            L"45% to below 50%: C (5 points) - Average\r\n"
            L"40% to below 45%: P (4 points) - Pass\r\n"
            L"Below 40%: F (0 points) - Fail\r\n\r\n"
            L"This comes from published GEU distance-learning material.\r\n"
            L"It is NOT confirmed as the grading policy for our B.Tech batch.\r\n"
            L"The reference does not state exactly 85%; EduTrack uses A+ there.\r\n\r\n"
            L"ORIGINAL PROJECT SCALE\r\n\r\n"
            L"90% and above: A\r\n80% and above: B\r\n70% and above: C\r\n"
            L"60% and above: D\r\nBelow 60%: F\r\n\r\n"
            L"The performance bands are separate project categories.\r\n"
            L"Changing the displayed grading scheme does not edit marks or saved files.\r\n"
            L"Please confirm the applicable grading policy with our mentor.";
        setText(GradeExplanation, explanation);
    }
    void showHelp() {
        showPage(Page::Students);
        MessageBoxW(window_,
            L"STUDENTS\nEnter roll, name, branch, semester and comma-separated marks.\n"
            L"Select a row to edit it. Use one search box and choose Linear or Binary.\n\n"
            L"REQUEST QUEUE\nOpen Request Queue and use the same form to queue an add/update/delete.\n"
            L"Process next applies the oldest request first. Undo reverts the last successful change.\n\n"
            L"REPORTS\nSelect a student, open Reports, and use Student report or Class report.\n\n"
            L"GRADING\nChoose a display scheme. GEU reference is not confirmed for B.Tech.\n\n"
            L"FILE\nSave writes students.txt in the working folder. Reload replaces unsaved edits\n"
            L"after confirmation. Pending requests and undo history are not saved.\n\n"
            L"Shortcuts: Ctrl+S Save, Ctrl+F Find, Ctrl+Z Undo, F1 Help.",
            L"How to use EduTrack", MB_OK | MB_ICONINFORMATION);
    }
    bool save() {
        std::string message;
        if (!RecordStorage::save(kDataFile, records_, message)) {
            error(L"Unable to save records: " + wide(message));
            return false;
        }
        dirty_ = false;
        setSummary();
        status(message.empty() ? L"Saved to students.txt." : L"Saved. " + wide(message));
        return true;
    }
    void reload() {
        if ((dirty_ || requests_.size() || history_.undoCount()) &&
            !ask(L"Reload saved student records?\n\nUnsaved edits, queued requests and undo history will be discarded."))
            return;
        StudentManagement loaded;
        std::string message;
        if (!RecordStorage::load(kDataFile, loaded, message)) {
            error(L"Reload failed. Current records are unchanged.\n" + wide(message));
            return;
        }
        records_ = std::move(loaded);
        requests_.clear();
        history_.clear();
        dirty_ = false;
        clearForm();
        refreshRecords();
        refreshQueue();
        setText(ReportOutput, L"Choose a report to view its details.");
        status(L"Saved records loaded. Queue and undo history have been cleared.");
    }
    bool confirmExit() {
        if (requests_.size() && !ask(L"There are unprocessed requests. They will not be saved. Exit?"))
            return false;
        return !dirty_ || save();
    }
    void processOne() {
        const auto result = requests_.processNext(records_, history_);
        if (!result) { status(L"No requests are waiting."); return; }
        if (result->applied) {
            dirty_ = true;
            refreshRecords();
            setText(ReportOutput, L"Records changed. Choose a report to view the latest results.");
        }
        refreshQueue();
        status(result->applied ? L"Request completed." :
               L"Request rejected. Student records were not changed.");
    }
    void processAll() {
        std::size_t success = 0, rejected = 0;
        while (requests_.size()) {
            const auto result = requests_.processNext(records_, history_);
            if (result && result->applied) ++success;
            else ++rejected;
        }
        if (success) {
            dirty_ = true;
            refreshRecords();
            setText(ReportOutput, L"Records changed. Choose a report to view the latest results.");
        }
        refreshQueue();
        status(L"Queue finished: " + std::to_wstring(success) + L" completed, " +
               std::to_wstring(rejected) + L" rejected.");
    }
    void onCommand(int id, int notification) {
        if (id == GradeChoice && notification == CBN_SELCHANGE) {
            const bool oldScale = SendMessageW(control(GradeChoice), CB_GETCURSEL, 0, 0) == 1;
            scale_ = oldScale ? AcademicProcessing::GradeScale::OriginalProject
                              : AcademicProcessing::GradeScale::GeuReference;
            refreshRecords();
            setText(ReportOutput, L"Grading scheme changed. Choose a report to update the displayed grade.");
            status(L"Grading display changed. Stored student marks have not changed.");
            return;
        }
        if (id == SortChoice && notification == CBN_SELCHANGE) {
            const int index = static_cast<int>(SendMessageW(control(SortChoice), CB_GETCURSEL, 0, 0));
            sort_ = index == 1 ? Sort::Roll : index == 2 ? Sort::Total : Sort::Rank;
            refreshRecords();
            status(L"Student table sorted.");
            return;
        }
        if (notification != BN_CLICKED && notification != 0 && notification != 1) return;
        switch (id) {
            case NavStudents: case MenuStudents: showPage(Page::Students); return;
            case NavQueue: case MenuQueue: showPage(Page::Queue); return;
            case NavReports: case MenuReports: showPage(Page::Reports); return;
            case NavGrading: case MenuGrading: showPage(Page::Grading); return;
            case MenuSave: save(); return;
            case MenuReload: reload(); return;
            case MenuExit: SendMessageW(window_, WM_CLOSE, 0, 0); return;
            case MenuHelp: showHelp(); return;
            case MenuAbout:
                MessageBoxW(window_,
                    L"EduTrack - Student Record Management\n"
                    L"A C++ project using Data Structures and Object-Oriented Programming.\n"
                    L"Local files only. There is no login, database or internet service.",
                    L"About EduTrack", MB_OK | MB_ICONINFORMATION);
                return;
            case MenuClear: case ClearForm: clearForm(); return;
            case MenuFind:
                showPage(Page::Students);
                SetFocus(control(SearchRoll));
                return;
            case FindStudent: findRecord(); return;
            case AddStudent: {
                auto student = studentFromForm();
                if (!student) return;
                if (!history_.addStudent(records_, *student)) {
                    error(L"Cannot add: the roll number already exists, or the record is invalid.");
                    return;
                }
                afterEdit(L"Student added. Use File > Save to keep the change.");
                return;
            }
            case UpdateStudent: {
                auto student = studentFromForm();
                if (!student) return;
                if (!history_.updateStudent(records_, student->rollNumber(), *student)) {
                    error(L"Cannot update: student not found, or the record is invalid.");
                    return;
                }
                afterEdit(L"Student updated. Undo is available under Edit.");
                return;
            }
            case DeleteStudent: {
                int roll = 0;
                if (!rollFrom(FieldRoll, roll)) return;
                if (!records_.findStudent(roll)) { error(L"Student not found."); return; }
                if (!ask(L"Delete this student record? You can undo the deletion.")) return;
                if (history_.deleteStudent(records_, roll)) {
                    clearForm();
                    afterEdit(L"Student deleted. Use Undo to restore it.");
                }
                return;
            }
            case Undo: case MenuUndo:
                if (!history_.undo(records_)) { status(L"No successful change to undo."); return; }
                afterEdit(L"Last change undone.");
                return;
            case QueueAdd: case QueueUpdate: {
                auto student = studentFromForm();
                if (!student) return;
                if (id == QueueAdd) requests_.enqueueAdd(*student);
                else requests_.enqueueUpdate(*student);
                refreshQueue();
                status(L"Request queued. It has not changed student records yet.");
                return;
            }
            case QueueDelete: {
                int roll = 0;
                if (!rollFrom(FieldRoll, roll)) return;
                requests_.enqueueDelete(roll);
                refreshQueue();
                status(L"Delete request queued. It has not run yet.");
                return;
            }
            case QueueNext: processOne(); return;
            case QueueAll: processAll(); return;
            case ReportStudent: studentReport(); return;
            case ReportClass: classReport(); return;
        }
    }
};
} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) {
    INITCOMMONCONTROLSEX common{};
    common.dwSize = sizeof(common);
    common.dwICC = ICC_LISTVIEW_CLASSES;
    if (!InitCommonControlsEx(&common)) {
        MessageBoxW(nullptr, L"Windows common controls are unavailable.", L"EduTrack", MB_OK | MB_ICONERROR);
        return 1;
    }
    DesktopApp app;
    if (!app.open(instance, show)) return 1;
    MSG msg{};
    BOOL result = 0;
    while ((result = GetMessageW(&msg, nullptr, 0, 0)) > 0) {
        if (!app.accelerators() || !TranslateAcceleratorW(app.window(), app.accelerators(), &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
    return result == -1 ? 1 : static_cast<int>(msg.wParam);
}
#else
// The desktop window is Windows-only. All core classes also work in the console version.
#endif
