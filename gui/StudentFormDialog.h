#ifndef EDUTRACK_STUDENT_FORM_DIALOG_H
#define EDUTRACK_STUDENT_FORM_DIALOG_H

#ifdef _WIN32
#include <windows.h>
#include "StudentManagement.h"

#include <optional>
#include <sstream>
#include <string>
#include <vector>

// A compact modal Win32 form keeps the student table readable even when the
// main window is at its minimum size. All input passes StudentManagement::isValid.
class StudentFormDialog {
public:
    static std::optional<Student> show(HWND owner, HINSTANCE instance, int dpi,
                                       const std::optional<Student>& initial,
                                       bool lockRollNumber) {
        StudentFormDialog form(owner, instance, dpi, initial, lockRollNumber);
        if (!form.open()) return std::nullopt;
        return form.result_;
    }

private:
    enum { RollField = 1101, NameField, BranchField, SemesterField, MarksField };
    HWND owner_ = nullptr, window_ = nullptr;
    HWND fields_[5]{};
    HINSTANCE instance_ = nullptr;
    HFONT font_ = nullptr;
    int dpi_ = 96;
    bool lockRoll_ = false;
    std::optional<Student> initial_, result_;

    StudentFormDialog(HWND owner, HINSTANCE instance, int dpi,
                      const std::optional<Student>& initial, bool lockRoll)
        : owner_(owner), instance_(instance), dpi_(dpi), lockRoll_(lockRoll), initial_(initial) {}
    int px(int n) const { return MulDiv(n, dpi_, 96); }

    static std::wstring widen(const std::string& s) {
        if (s.empty()) return {};
        const int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                              s.data(), static_cast<int>(s.size()), nullptr, 0);
        if (n <= 0) return L"";
        std::wstring result(static_cast<std::size_t>(n), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), result.data(), n);
        return result;
    }
    static std::optional<std::string> narrow(const std::wstring& s) {
        if (s.empty()) return std::string{};
        const int n = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
                            s.data(), static_cast<int>(s.size()), nullptr, 0, nullptr, nullptr);
        if (n <= 0) return std::nullopt;
        std::string result(static_cast<std::size_t>(n), '\0');
        if (!WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, s.data(),
                              static_cast<int>(s.size()), result.data(), n, nullptr, nullptr))
            return std::nullopt;
        return result;
    }
    static std::wstring trim(const std::wstring& s) {
        const auto first = s.find_first_not_of(L" \t\r\n");
        if (first == std::wstring::npos) return {};
        return s.substr(first, s.find_last_not_of(L" \t\r\n") - first + 1);
    }
    static bool decimal(const std::wstring& raw, int minimum, int maximum, int& number) {
        const std::wstring s = trim(raw);
        if (s.empty() || s.size() > 10) return false;
        unsigned long long value = 0;
        for (wchar_t c : s) {
            if (c < L'0' || c > L'9') return false;
            value = value * 10 + (c - L'0');
            if (value > static_cast<unsigned long long>(maximum)) return false;
        }
        if (value < static_cast<unsigned long long>(minimum)) return false;
        number = static_cast<int>(value);
        return true;
    }
    static std::wstring content(HWND h) {
        const int n = GetWindowTextLengthW(h);
        if (n < 0 || n > 1024) return {};
        std::wstring s(static_cast<std::size_t>(n) + 1, L'\0');
        s.resize(static_cast<std::size_t>(GetWindowTextW(h, s.data(), n + 1)));
        return s;
    }
    static bool marksList(const std::wstring& raw, std::vector<int>& result) {
        if (raw.empty()) return false;
        std::size_t first = 0;
        while (first <= raw.size()) {
            const auto comma = raw.find(L',', first);
            const auto item = raw.substr(first, comma == std::wstring::npos ? comma : comma - first);
            int mark = 0;
            if (!decimal(item, 0, 100, mark) || result.size() >= 20) return false;
            result.push_back(mark);
            if (comma == std::wstring::npos) return true;
            first = comma + 1;
        }
        return false;
    }
    static LRESULT CALLBACK procedure(HWND hwnd, UINT message, WPARAM w, LPARAM l) {
        auto* form = reinterpret_cast<StudentFormDialog*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (message == WM_NCCREATE) {
            auto* create = reinterpret_cast<CREATESTRUCTW*>(l);
            form = static_cast<StudentFormDialog*>(create->lpCreateParams);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(form));
        }
        if (!form) return DefWindowProcW(hwnd, message, w, l);
        switch (message) {
            case WM_CREATE: form->populate(); return 0;
            case WM_COMMAND:
                if (LOWORD(w) == IDOK) { form->accept(); return 0; }
                if (LOWORD(w) == IDCANCEL) { DestroyWindow(hwnd); return 0; }
                break;
            case WM_CLOSE: DestroyWindow(hwnd); return 0;
        }
        return DefWindowProcW(hwnd, message, w, l);
    }
    HWND make(const wchar_t* className, const wchar_t* caption, DWORD styles,
              int id, int x, int y, int width, int height) {
        HWND h = CreateWindowExW(0, className, caption, WS_CHILD | WS_VISIBLE | styles,
                        px(x), px(y), px(width), px(height), window_,
                        reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), instance_, nullptr);
        if (h && font_) SendMessageW(h, WM_SETFONT, reinterpret_cast<WPARAM>(font_), TRUE);
        return h;
    }
    void populate() {
        font_ = CreateFontW(-px(16), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                            CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        make(L"STATIC", L"Enter student information (subject marks are each out of 100).",
             SS_LEFT, 0, 20, 16, 505, 30);
        const wchar_t* captions[] = {L"Roll number", L"Student name", L"Branch", L"Semester"};
        for (int i = 0; i < 4; ++i) {
            const int y = 57 + i * 58;
            make(L"STATIC", captions[i], SS_LEFT, 0, 20, y + 6, 125, 26);
            fields_[i] = make(L"EDIT", L"", WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL,
                               RollField + i, 154, y, 342, 35);
            if (fields_[i]) SendMessageW(fields_[i], EM_LIMITTEXT, 160, 0);
        }
        make(L"STATIC", L"Subject marks: comma-separated integers, e.g. 75, 60, 92",
             SS_LEFT, 0, 20, 295, 500, 29);
        fields_[4] = make(L"EDIT", L"", WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL,
                           MarksField, 20, 325, 476, 35);
        if (fields_[4]) SendMessageW(fields_[4], EM_LIMITTEXT, 159, 0);
        make(L"BUTTON", L"Save", BS_DEFPUSHBUTTON | WS_TABSTOP, IDOK, 286, 385, 100, 39);
        make(L"BUTTON", L"Cancel", BS_PUSHBUTTON | WS_TABSTOP, IDCANCEL, 396, 385, 100, 39);
        if (initial_) {
            SetWindowTextW(fields_[0], std::to_wstring(initial_->rollNumber()).c_str());
            SetWindowTextW(fields_[1], widen(initial_->name()).c_str());
            SetWindowTextW(fields_[2], widen(initial_->branch()).c_str());
            SetWindowTextW(fields_[3], std::to_wstring(initial_->semester()).c_str());
            std::wstring marks;
            for (int value : initial_->marks()) {
                if (!marks.empty()) marks += L", ";
                marks += std::to_wstring(value);
            }
            SetWindowTextW(fields_[4], marks.c_str());
        }
        if (lockRoll_) SendMessageW(fields_[0], EM_SETREADONLY, TRUE, 0);
    }
    void accept() {
        int roll = 0, semester = 0;
        std::vector<int> marks;
        auto name = narrow(trim(content(fields_[1])));
        auto branch = narrow(trim(content(fields_[2])));
        if (!decimal(content(fields_[0]), 1, 999999999, roll) ||
            !decimal(content(fields_[3]), 1, 12, semester) ||
            !marksList(content(fields_[4]), marks) || !name || !branch) {
            MessageBoxW(window_, L"Check roll number (1-999999999), semester (1-12), and 1-20 marks between 0 and 100.",
                        L"Invalid student details", MB_OK | MB_ICONWARNING);
            return;
        }
        Student student(roll, *name, *branch, semester, std::move(marks));
        if (!StudentManagement::isValid(student) ||
            (lockRoll_ && initial_ && initial_->rollNumber() != student.rollNumber())) {
            MessageBoxW(window_, L"The name, branch, subject marks or record details are invalid.",
                        L"Invalid student details", MB_OK | MB_ICONWARNING);
            return;
        }
        result_ = std::move(student);
        DestroyWindow(window_);
    }
    bool open() {
        constexpr wchar_t className[] = L"EduTrackStudentForm3F3";
        WNDCLASSW klass{};
        klass.lpfnWndProc = &StudentFormDialog::procedure;
        klass.hInstance = instance_;
        klass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        klass.hbrBackground = GetSysColorBrush(COLOR_WINDOW);
        klass.lpszClassName = className;
        if (!RegisterClassW(&klass) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
            return false;
        RECT client{0, 0, px(520), px(450)};
        AdjustWindowRectEx(&client, WS_POPUP | WS_CAPTION | WS_SYSMENU, FALSE, WS_EX_DLGMODALFRAME);
        const int width = client.right - client.left, height = client.bottom - client.top;
        RECT parent{};
        GetWindowRect(owner_, &parent);
        const int x = parent.left + (parent.right - parent.left - width) / 2;
        const int y = parent.top + (parent.bottom - parent.top - height) / 2;
        window_ = CreateWindowExW(WS_EX_DLGMODALFRAME, className,
                    initial_ ? L"Edit student / queue update" : L"New student / queue add",
                    WS_POPUP | WS_CAPTION | WS_SYSMENU, x, y, width, height,
                    owner_, nullptr, instance_, this);
        if (!window_) return false;
        EnableWindow(owner_, FALSE);
        ShowWindow(window_, SW_SHOW);
        SetForegroundWindow(window_);
        SetFocus(fields_[lockRoll_ ? 1 : 0]);
        MSG message{};
        while (IsWindow(window_)) {
            const BOOL code = GetMessageW(&message, nullptr, 0, 0);
            if (code <= 0) {
                if (code == 0) PostQuitMessage(static_cast<int>(message.wParam));
                if (IsWindow(window_)) DestroyWindow(window_);
                break;
            }
            if (!IsDialogMessageW(window_, &message)) {
                TranslateMessage(&message);
                DispatchMessageW(&message);
            }
        }
        EnableWindow(owner_, TRUE);
        SetActiveWindow(owner_);
        if (font_) DeleteObject(font_);
        return true;
    }
};
#endif // _WIN32
#endif
