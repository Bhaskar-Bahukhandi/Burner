#ifndef EDUTRACK_SUBJECT_MARKS_DIALOG_H
#define EDUTRACK_SUBJECT_MARKS_DIALOG_H

#ifdef _WIN32
#include <windows.h>
#include "AcademicGroups.h"

#include <array>
#include <optional>
#include <string>
#include <vector>

// Up to 20 fields, presented in two columns. A named group locks the
// subject count and shows actual subject names; otherwise labels are numbered.
class SubjectMarksDialog {
public:
    enum class Mode { Marks, Ceilings };
    static std::optional<std::vector<int>> show(HWND owner, HINSTANCE instance, int dpi,
                                 const std::vector<int>& current,
                                 const AcademicGroup* group,
                                 Mode mode = Mode::Marks) {
        SubjectMarksDialog dialog(owner, instance, dpi, current, group, mode);
        if (!dialog.open()) return std::nullopt;
        return dialog.result_;
    }
private:
    enum { CountInput = 1601, ApplyCount, FirstMark = 1700 };
    HWND owner_ = nullptr, hwnd_ = nullptr, countInput_ = nullptr;
    HINSTANCE instance_ = nullptr;
    HFONT font_ = nullptr;
    bool controlsReady_ = true;
    int dpi_ = 96, count_ = 5;
    const AcademicGroup* group_ = nullptr;
    Mode mode_ = Mode::Marks;
    std::array<HWND, 20> edits_{}, labels_{};
    std::vector<int> original_;
    std::optional<std::vector<int>> result_;

    SubjectMarksDialog(HWND owner, HINSTANCE instance, int dpi,
                 const std::vector<int>& current, const AcademicGroup* group, Mode mode)
        : owner_(owner), instance_(instance), dpi_(dpi), group_(group), mode_(mode), original_(current) {
        if (group_) count_ = static_cast<int>(group_->subjects.size());
        else if (!current.empty()) count_ = static_cast<int>(current.size());
    }
    int px(int n) const { return MulDiv(n, dpi_, 96); }
    static std::wstring read(HWND h) {
        int n = GetWindowTextLengthW(h);
        if (n < 0 || n > 32) return {};
        std::wstring str(static_cast<std::size_t>(n) + 1, L'\0');
        str.resize(static_cast<std::size_t>(GetWindowTextW(h, str.data(), n + 1)));
        return str;
    }
    static bool integer(const std::wstring& text, int lower, int upper, int& result) {
        if (text.empty() || text.size() > 3) return false;
        int value = 0;
        for (wchar_t c : text) {
            if (c < L'0' || c > L'9') return false;
            value = value * 10 + c - L'0';
            if (value > upper) return false;
        }
        if (value < lower) return false;
        result = value; return true;
    }
    static std::wstring utf8(const std::string& bytes) {
        if (bytes.empty()) return {};
        int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                   bytes.data(), static_cast<int>(bytes.size()), nullptr, 0);
        if (n <= 0) return L"[subject]";
        std::wstring value(static_cast<std::size_t>(n), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, bytes.data(), static_cast<int>(bytes.size()), value.data(), n);
        return value;
    }
    HWND make(const wchar_t* cls, const wchar_t* caption, DWORD style,
              int id, int x, int y, int w, int h) {
        HWND control = CreateWindowExW(0, cls, caption, WS_CHILD | WS_VISIBLE | style,
                       px(x), px(y), px(w), px(h), hwnd_,
                       reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), instance_, nullptr);
        if (!control) controlsReady_ = false;
        if (control && font_) SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(font_), TRUE);
        return control;
    }
    void showRows() {
        for (int i = 0; i < 20; ++i) {
            const int display = i < count_ ? SW_SHOW : SW_HIDE;
            ShowWindow(labels_[i], display);
            ShowWindow(edits_[i], display);
        }
    }
    bool populate() {
        font_ = CreateFontW(-px(15), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                     DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        if (!font_) return false;
        make(L"STATIC", mode_ == Mode::Ceilings
                           ? L"Enter the highest mark allowed for each subject in this scenario."
                           : group_ ? L"Subject names follow the assigned academic group."
                                    : L"Subjects are numbered until you assign a matching academic group.",
             SS_LEFT, 0, 18, 15, 750, 28);
        make(L"STATIC", L"Number of subjects (1-20):", SS_LEFT, 0, 18, 58, 220, 27);
        countInput_ = make(L"EDIT", std::to_wstring(count_).c_str(),
                          WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP,
                          CountInput, 238, 52, 65, 34);
        SendMessageW(countInput_, EM_LIMITTEXT, 2, 0);
        make(L"BUTTON", L"Apply count", BS_PUSHBUTTON | WS_TABSTOP,
             ApplyCount, 313, 52, 114, 34);
        if (group_ || mode_ == Mode::Ceilings) {
            SendMessageW(countInput_, EM_SETREADONLY, TRUE, 0);
            EnableWindow(GetDlgItem(hwnd_, ApplyCount), FALSE);
        }
        for (int i = 0; i < 20; ++i) {
            int x = (i / 10) * 384 + 18;
            int y = 105 + (i % 10) * 43;
            std::wstring name = group_ && i < count_
                ? utf8(group_->subjects[static_cast<std::size_t>(i)].code) + L" - " +
                  utf8(group_->subjects[static_cast<std::size_t>(i)].name)
                : L"Subject " + std::to_wstring(i + 1);
            // Source labels can be long; full details remain on the Groups page.
            if (name.size() > 29) name = name.substr(0, 26) + L"...";
            labels_[i] = make(L"STATIC", name.c_str(), SS_LEFT | SS_NOPREFIX,
                              0, x, y + 7, 270, 28);
            edits_[i] = make(L"EDIT", i < static_cast<int>(original_.size())
                         ? std::to_wstring(original_[static_cast<std::size_t>(i)]).c_str() : L"0",
                        WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL,
                        FirstMark + i, x + 276, y, 70, 34);
            SendMessageW(edits_[i], EM_LIMITTEXT, 3, 0);
        }
        make(L"BUTTON", mode_ == Mode::Ceilings ? L"Apply limits" : L"Apply marks",
             BS_DEFPUSHBUTTON | WS_TABSTOP,
             IDOK, 536, 552, 120, 40);
        make(L"BUTTON", L"Cancel", BS_PUSHBUTTON | WS_TABSTOP,
             IDCANCEL, 663, 552, 105, 40);
        showRows();
        if (!controlsReady_ || !countInput_) return false;
        for (int i = 0; i < 20; ++i)
            if (!labels_[i] || !edits_[i]) return false;
        return true;
    }
    void adjustCount() {
        int chosen = 0;
        if (!integer(read(countInput_), 1, 20, chosen)) {
            MessageBoxW(hwnd_, L"Choose a whole number from 1 to 20.", L"Subject count", MB_OK | MB_ICONWARNING);
            return;
        }
        count_ = chosen;
        showRows();
    }
    void accept() {
        std::vector<int> marks;
        for (int i = 0; i < count_; ++i) {
            int value = 0;
            if (!integer(read(edits_[i]), 0, 100, value)) {
                std::wstring error = std::wstring(mode_ == Mode::Ceilings ? L"Limit" : L"Mark") +
                                     L" for subject " + std::to_wstring(i + 1) +
                                     L" must be a whole number from 0 to 100.";
                MessageBoxW(hwnd_, error.c_str(), L"Invalid mark", MB_OK | MB_ICONWARNING);
                SetFocus(edits_[i]);
                return;
            }
            marks.push_back(value);
        }
        result_ = std::move(marks);
        DestroyWindow(hwnd_);
    }
    static LRESULT CALLBACK proc(HWND hwnd, UINT message, WPARAM w, LPARAM l) {
        auto* self = reinterpret_cast<SubjectMarksDialog*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (message == WM_NCCREATE) {
            self = static_cast<SubjectMarksDialog*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);
            self->hwnd_ = hwnd; // Available to child controls during WM_CREATE.
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        }
        if (!self) return DefWindowProcW(hwnd, message, w, l);
        switch (message) {
            case WM_CREATE: return self->populate() ? 0 : -1;
            case WM_COMMAND:
                if (LOWORD(w) == IDOK) { self->accept(); return 0; }
                if (LOWORD(w) == IDCANCEL) { DestroyWindow(hwnd); return 0; }
                if (LOWORD(w) == ApplyCount && HIWORD(w) == BN_CLICKED) {
                    self->adjustCount(); return 0;
                }
                break;
            case WM_CLOSE: DestroyWindow(hwnd); return 0;
            case WM_NCDESTROY: self->hwnd_ = nullptr; break;
        }
        return DefWindowProcW(hwnd, message, w, l);
    }
    bool open() {
        constexpr wchar_t className[] = L"EduTrackSubjectMarks";
        WNDCLASSW wc{}; wc.lpfnWndProc = proc; wc.hInstance = instance_;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = GetSysColorBrush(COLOR_WINDOW);
        wc.lpszClassName = className;
        if (!RegisterClassW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return false;
        RECT size{0, 0, px(785), px(612)};
        AdjustWindowRectEx(&size, WS_POPUP | WS_CAPTION | WS_SYSMENU, FALSE, WS_EX_DLGMODALFRAME);
        RECT parent{}; GetWindowRect(owner_, &parent);
        const int w = size.right - size.left, h = size.bottom - size.top;
        HWND created = CreateWindowExW(WS_EX_DLGMODALFRAME, className,
              mode_ == Mode::Ceilings ? L"Goal planner - subject limits" : L"Subject marks (0-100)",
              WS_POPUP | WS_CAPTION | WS_SYSMENU,
              parent.left + (parent.right - parent.left - w) / 2,
              parent.top + (parent.bottom - parent.top - h) / 2,
              w, h, owner_, nullptr, instance_, this);
        if (!created) {
            hwnd_ = nullptr;
            if (font_) { DeleteObject(font_); font_ = nullptr; }
            MessageBoxW(owner_, L"The subject editor could not create its input controls.",
                        L"EduTrack - form error", MB_OK | MB_ICONERROR);
            return false;
        }
        hwnd_ = created;
        EnableWindow(owner_, FALSE);
        ShowWindow(hwnd_, SW_SHOW); SetForegroundWindow(hwnd_);
        SetFocus(edits_[0]);
        MSG message{};
        while (IsWindow(hwnd_)) {
            BOOL code = GetMessageW(&message, nullptr, 0, 0);
            if (code <= 0) {
                if (code == 0) PostQuitMessage(static_cast<int>(message.wParam));
                if (IsWindow(hwnd_)) DestroyWindow(hwnd_);
                break;
            }
            if (message.message == WM_KEYDOWN && message.wParam == VK_ESCAPE &&
                (message.hwnd == hwnd_ || IsChild(hwnd_, message.hwnd))) {
                DestroyWindow(hwnd_); continue;
            }
            if (!IsDialogMessageW(hwnd_, &message)) {
                TranslateMessage(&message); DispatchMessageW(&message);
            }
        }
        EnableWindow(owner_, TRUE); SetActiveWindow(owner_);
        if (font_) DeleteObject(font_);
        return true;
    }
};
#endif // _WIN32
#endif
