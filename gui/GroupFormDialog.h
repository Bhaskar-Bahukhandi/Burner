#ifndef EDUTRACK_GROUP_FORM_DIALOG_H
#define EDUTRACK_GROUP_FORM_DIALOG_H

#ifdef _WIN32
#include <windows.h>
#include "AcademicGroups.h"
#include "GroupDefinitionInput.h"

#include <optional>
#include <string>
#include <utility>

// Create-only form. AcademicGroups::addGroup performs the final validation.
class GroupFormDialog {
public:
    static std::optional<AcademicGroup> show(HWND owner, HINSTANCE instance, int dpi) {
        GroupFormDialog dialog(owner, instance, dpi);
        if (!dialog.open()) return std::nullopt;
        return dialog.result_;
    }

private:
    enum { GroupId = 1501, Branch, Semester, Cohort, Subjects };
    HWND owner_ = nullptr, hwnd_ = nullptr, fields_[5]{};
    HINSTANCE instance_ = nullptr;
    HFONT font_ = nullptr;
    bool controlsReady_ = true;
    int dpi_ = 96;
    std::optional<AcademicGroup> result_;

    GroupFormDialog(HWND owner, HINSTANCE instance, int dpi)
        : owner_(owner), instance_(instance), dpi_(dpi) {}
    int px(int amount) const { return MulDiv(amount, dpi_, 96); }
    static std::wstring read(HWND h) {
        int n = GetWindowTextLengthW(h);
        if (n < 0 || n > 4096) return {};
        std::wstring text(static_cast<std::size_t>(n) + 1, L'\0');
        text.resize(static_cast<std::size_t>(GetWindowTextW(h, text.data(), n + 1)));
        return text;
    }
    static std::optional<std::string> toUtf8(const std::wstring& text) {
        if (text.empty()) return std::string{};
        int n = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(),
                      static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
        if (n <= 0) return std::nullopt;
        std::string converted(static_cast<std::size_t>(n), '\0');
        if (!WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(),
                 static_cast<int>(text.size()), converted.data(), n, nullptr, nullptr))
            return std::nullopt;
        return converted;
    }
    HWND make(const wchar_t* cls, const wchar_t* caption, DWORD style,
              int id, int x, int y, int w, int h) {
        HWND child = CreateWindowExW(0, cls, caption, WS_CHILD | WS_VISIBLE | style,
                   px(x), px(y), px(w), px(h), hwnd_,
                   reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), instance_, nullptr);
        if (!child) controlsReady_ = false;
        if (child && font_) SendMessageW(child, WM_SETFONT, reinterpret_cast<WPARAM>(font_), TRUE);
        return child;
    }
    bool populate() {
        font_ = CreateFontW(-px(16), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                      DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        if (!font_) return false;
        const wchar_t* labels[] = {L"Group ID", L"Branch", L"Semester", L"Cohort year"};
        for (int i = 0; i < 4; ++i) {
            int y = 22 + i * 52;
            make(L"STATIC", labels[i], SS_LEFT, 0, 20, y + 7, 120, 28);
            fields_[i] = make(L"EDIT", L"", WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL,
                              GroupId + i, 150, y, 440, 35);
            SendMessageW(fields_[i], EM_LIMITTEXT, 80, 0);
        }
        SetWindowTextW(fields_[2], L"3");
        make(L"STATIC", L"Subjects in marks order (one per line): CODE | Subject name",
             SS_LEFT, 0, 20, 236, 600, 28);
        fields_[4] = make(L"EDIT", L"DS | Data Structures\r\nMATH | Mathematics",
                          WS_BORDER | WS_TABSTOP | ES_MULTILINE | ES_AUTOVSCROLL |
                          WS_VSCROLL | ES_WANTRETURN, Subjects, 20, 268, 570, 235);
        SendMessageW(fields_[4], EM_LIMITTEXT, 4095, 0);
        make(L"STATIC", L"Use unique uppercase subject codes (A-Z, 0-9, _ or -). 1-20 subjects.",
             SS_LEFT, 0, 20, 512, 570, 25);
        make(L"BUTTON", L"Create group", BS_DEFPUSHBUTTON | WS_TABSTOP, IDOK, 340, 550, 120, 38);
        make(L"BUTTON", L"Cancel", BS_PUSHBUTTON | WS_TABSTOP, IDCANCEL, 470, 550, 120, 38);
        return controlsReady_ && fields_[0] && fields_[1] && fields_[2] &&
               fields_[3] && fields_[4];
    }
    void accept() {
        auto id = toUtf8(read(fields_[0]));
        auto branch = toUtf8(read(fields_[1]));
        auto semesterRaw = toUtf8(read(fields_[2]));
        auto cohort = toUtf8(read(fields_[3]));
        auto subjects = toUtf8(read(fields_[4]));
        int semester = 0;
        if (!id || !branch || !semesterRaw || !cohort || !subjects ||
            semesterRaw->empty() || semesterRaw->size() > 2) {
            MessageBoxW(hwnd_, L"Check group details and subject definitions.", L"Invalid group", MB_OK | MB_ICONWARNING);
            return;
        }
        for (char c : *semesterRaw) {
            if (c < '0' || c > '9') { semester = 0; break; }
            semester = 10 * semester + c - '0';
        }
        AcademicGroup group{GroupDefinitionInput::trim(*id),
                            GroupDefinitionInput::trim(*branch), semester,
                            GroupDefinitionInput::trim(*cohort), {}};
        AcademicGroups check;
        if (!GroupDefinitionInput::parseSubjects(*subjects, group.subjects) ||
            !check.addGroup(group)) {
            MessageBoxW(hwnd_, L"Invalid group. Check ID, branch, semester (1-12), year (2000-2100),\n"
                      L"and unique uppercase subject codes. Each line needs CODE | Subject name.",
                      L"Invalid group", MB_OK | MB_ICONWARNING);
            return;
        }
        result_ = std::move(group);
        DestroyWindow(hwnd_);
    }
    static LRESULT CALLBACK procedure(HWND hwnd, UINT msg, WPARAM w, LPARAM l) {
        auto* self = reinterpret_cast<GroupFormDialog*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (msg == WM_NCCREATE) {
            self = static_cast<GroupFormDialog*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);
            self->hwnd_ = hwnd; // Set the parent before WM_CREATE builds child controls.
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        }
        if (!self) return DefWindowProcW(hwnd, msg, w, l);
        switch (msg) {
            case WM_CREATE: return self->populate() ? 0 : -1;
            case WM_COMMAND:
                if (LOWORD(w) == IDOK) { self->accept(); return 0; }
                if (LOWORD(w) == IDCANCEL) { DestroyWindow(hwnd); return 0; }
                break;
            case WM_CLOSE: DestroyWindow(hwnd); return 0;
            case WM_NCDESTROY: self->hwnd_ = nullptr; break;
        }
        return DefWindowProcW(hwnd, msg, w, l);
    }
    bool open() {
        const wchar_t name[] = L"EduTrackGroupForm";
        WNDCLASSW cls{};
        cls.lpfnWndProc = procedure; cls.hInstance = instance_;
        cls.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        cls.hbrBackground = GetSysColorBrush(COLOR_WINDOW);
        cls.lpszClassName = name;
        if (!RegisterClassW(&cls) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return false;
        RECT bounds{0, 0, px(610), px(610)};
        AdjustWindowRectEx(&bounds, WS_POPUP | WS_CAPTION | WS_SYSMENU, FALSE, WS_EX_DLGMODALFRAME);
        RECT parent{}; GetWindowRect(owner_, &parent);
        const int width = bounds.right - bounds.left, height = bounds.bottom - bounds.top;
        HWND created = CreateWindowExW(WS_EX_DLGMODALFRAME, name, L"Create academic group",
                  WS_POPUP | WS_CAPTION | WS_SYSMENU,
                  parent.left + ((parent.right - parent.left - width) / 2),
                  parent.top + ((parent.bottom - parent.top - height) / 2),
                  width, height, owner_, nullptr, instance_, this);
        if (!created) {
            hwnd_ = nullptr;
            if (font_) { DeleteObject(font_); font_ = nullptr; }
            MessageBoxW(owner_, L"The academic-group form could not create its input controls.",
                        L"EduTrack - form error", MB_OK | MB_ICONERROR);
            return false;
        }
        hwnd_ = created;
        EnableWindow(owner_, FALSE);
        ShowWindow(hwnd_, SW_SHOW); SetForegroundWindow(hwnd_);
        SetFocus(fields_[0]);
        MSG msg{};
        while (IsWindow(hwnd_)) {
            BOOL value = GetMessageW(&msg, nullptr, 0, 0);
            if (value <= 0) {
                if (value == 0) PostQuitMessage(static_cast<int>(msg.wParam));
                if (IsWindow(hwnd_)) DestroyWindow(hwnd_);
                break;
            }
            if (msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE &&
                (msg.hwnd == hwnd_ || IsChild(hwnd_, msg.hwnd))) {
                DestroyWindow(hwnd_); continue;
            }
            if (!IsDialogMessageW(hwnd_, &msg)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
        }
        EnableWindow(owner_, TRUE); SetActiveWindow(owner_);
        if (font_) DeleteObject(font_);
        return true;
    }
};
#endif // _WIN32
#endif
