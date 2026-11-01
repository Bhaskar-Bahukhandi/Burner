// Native Windows smoke test for the three modal editors.
// A child-window regression can pass every portable algorithm test, so this
// test checks that the actual Win32 dialog creates its visible controls.
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include "StudentFormDialog.h"
#include "GroupFormDialog.h"
#include "SubjectMarksDialog.h"

#include <atomic>
#include <iostream>
#include <optional>
#include <thread>
#include <vector>

namespace {
bool inspectAndClose(const wchar_t* className, const std::vector<int>& ids) {
    for (int attempt = 0; attempt < 250; ++attempt) {
        const HWND hwnd = FindWindowW(className, nullptr);
        if (hwnd && IsWindowVisible(hwnd)) {
            bool controlsReady = true;
            for (int id : ids) {
                const HWND child = GetDlgItem(hwnd, id);
                controlsReady = controlsReady && child && IsWindow(child) &&
                                GetParent(child) == hwnd;
            }
            PostMessageW(hwnd, WM_COMMAND, MAKEWPARAM(IDCANCEL, BN_CLICKED), 0);
            return controlsReady;
        }
        Sleep(20);
    }
    return false;
}
}

int main() {
    const HINSTANCE instance = GetModuleHandleW(nullptr);
    WNDCLASSW ownerClass{};
    ownerClass.lpfnWndProc = DefWindowProcW;
    ownerClass.hInstance = instance;
    ownerClass.lpszClassName = L"EduTrackGuiSmokeOwner";
    if (!RegisterClassW(&ownerClass)) return 2;
    const HWND owner = CreateWindowW(ownerClass.lpszClassName, L"EduTrack test owner",
                       WS_OVERLAPPEDWINDOW, 100, 100, 500, 500,
                       nullptr, nullptr, instance, nullptr);
    if (!owner) return 2;
    bool groupControls = false;
    std::thread groupCheck([&] {
        groupControls = inspectAndClose(L"EduTrackGroupForm",
                             {1501, 1502, 1503, 1504, 1505, IDOK, IDCANCEL});
    });
    auto group = GroupFormDialog::show(owner, instance, 96);
    groupCheck.join();
    const bool groupPassed = !group && groupControls;

    bool studentControls = false;
    std::thread studentCheck([&] {
        studentControls = inspectAndClose(L"EduTrackStudentForm",
                               {1101, 1102, 1103, 1104, 1105, IDOK, IDCANCEL});
    });
    auto student = StudentFormDialog::show(owner, instance, 96, std::nullopt, false);
    studentCheck.join();
    const bool studentPassed = !student && studentControls;

    bool marksControls = false;
    std::thread marksCheck([&] {
        marksControls = inspectAndClose(L"EduTrackSubjectMarks",
                             {1601, 1602, 1700, 1704, IDOK, IDCANCEL});
    });
    auto marks = SubjectMarksDialog::show(owner, instance, 96, {60, 70, 80, 90, 100}, nullptr);
    marksCheck.join();
    const bool marksPassed = !marks && marksControls;

    // The limits editor reuses the exact same labelled controls.
    bool limitsControls = false;
    std::thread limitsCheck([&] {
        limitsControls = inspectAndClose(L"EduTrackSubjectMarks",
                              {1601, 1602, 1700, 1704, IDOK, IDCANCEL});
    });
    auto limits = SubjectMarksDialog::show(owner, instance, 96,
                   {100, 100, 100, 100, 100}, nullptr,
                   SubjectMarksDialog::Mode::Ceilings);
    limitsCheck.join();
    const bool limitsPassed = !limits && limitsControls;

    DestroyWindow(owner);
    if (!groupPassed || !studentPassed || !marksPassed || !limitsPassed) {
        std::cerr << "Windows dialog smoke test failed:";
        if (!groupPassed) std::cerr << " group";
        if (!studentPassed) std::cerr << " student";
        if (!marksPassed) std::cerr << " marks";
        if (!limitsPassed) std::cerr << " limits";
        std::cerr << "\n";
        return 1;
    }
    std::cout << "Windows group, student, subject and limits dialogs passed.\n";
    return 0;
}
#else
int main() { return 0; }
#endif
