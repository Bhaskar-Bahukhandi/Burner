# Build EduTrack using one consistent MSYS2 UCRT64 toolchain.
# Run from PowerShell: .\build_windows.ps1
param(
    [ValidateSet('GUI', 'Console', 'Importer', 'Maintenance', 'Tests', 'All')]
    [string]$Target = 'GUI',
    [string]$Ucrt64 = 'C:\msys64\ucrt64'
)

$ErrorActionPreference = 'Stop'
$Root = $PSScriptRoot
$Cxx = Join-Path $Ucrt64 'bin\g++.exe'
$SQLiteHeader = Join-Path $Ucrt64 'include\sqlite3.h'
$SQLiteImport = Join-Path $Ucrt64 'lib\libsqlite3.dll.a'
$SQLiteStatic = Join-Path $Ucrt64 'lib\libsqlite3.a'

if (-not (Test-Path -LiteralPath $Cxx -PathType Leaf)) {
    throw "MSYS2 UCRT64 g++ was not found at $Cxx. Install MSYS2 and mingw-w64-ucrt-x86_64-gcc."
}
if (-not (Test-Path -LiteralPath $SQLiteHeader -PathType Leaf) -or
    (-not (Test-Path -LiteralPath $SQLiteImport -PathType Leaf) -and
     -not (Test-Path -LiteralPath $SQLiteStatic -PathType Leaf))) {
    throw "SQLite3 headers/import library are missing from $Ucrt64. Open the MSYS2 UCRT64 terminal and run: pacman -S --needed mingw-w64-ucrt-x86_64-sqlite3"
}

# Use the same UCRT64 runtime DLLs as the compiler and SQLite library.
$env:PATH = (Join-Path $Ucrt64 'bin') + ';' + $env:PATH
$Include = '-I' + (Join-Path $Root 'include')
$SQLiteInclude = '-I' + (Join-Path $Ucrt64 'include')
$GuiInclude = '-I' + (Join-Path $Root 'gui')
$SQLiteLib = '-L' + (Join-Path $Ucrt64 'lib')
$Std = @('-std=c++17', '-Wall', '-Wextra', '-pedantic', $Include, $GuiInclude, $SQLiteInclude, $SQLiteLib)
$Src = @(Get-ChildItem -LiteralPath (Join-Path $Root 'src') -Filter '*.cpp' -File | ForEach-Object { $_.FullName })
$Base = Join-Path $Root 'database\SqliteStorage.cpp'
$Support = @('Student.cpp', 'StudentManagement.cpp', 'AcademicGroups.cpp', 'RecordStorage.cpp', 'AcademicGroupStorage.cpp') |
    ForEach-Object { Join-Path $Root ('src\' + $_) }

function Build([string]$Label, [string[]]$Files, [string[]]$Flags, [string[]]$Libraries, [string]$Output) {
    Write-Host "Building $Label..."
    & $Cxx @Std @Flags @Files '-lsqlite3' @Libraries '-o' (Join-Path $Root $Output)
    if ($LASTEXITCODE -ne 0) { throw "$Label compilation failed (exit $LASTEXITCODE)." }
    Write-Host "Built $Output" -ForegroundColor Green
}

if ($Target -eq 'GUI' -or $Target -eq 'All') {
    $Library = @($Src | Where-Object { (Split-Path $_ -Leaf) -ne 'main.cpp' })
    Build 'Windows desktop' (@((Join-Path $Root 'gui\WindowsGUI.cpp')) + $Library + @($Base)) @('-municode', '-mwindows') @('-lcomctl32') 'EduTrack.exe'
}
if ($Target -eq 'Console' -or $Target -eq 'All') {
    Build 'Console' ($Src + @($Base)) @() @() 'EduTrackConsole.exe'
}
if ($Target -eq 'Importer' -or $Target -eq 'All') {
    Build 'Legacy importer' (@((Join-Path $Root 'tools\migrate_to_sqlite.cpp'), $Base) + $Support) @() @() 'edutrack-migrate.exe'
}
if ($Target -eq 'Maintenance' -or $Target -eq 'All') {
    Build 'Database maintenance' (@((Join-Path $Root 'tools\database_maintenance.cpp'),
            (Join-Path $Root 'database\DatabaseMaintenance.cpp'), $Base) + $Support) @() @() 'edutrack-db.exe'
}

if ($Target -eq 'Tests' -or $Target -eq 'All') {
    $Core = @('Student.cpp', 'StudentManagement.cpp', 'RecordAlgorithms.cpp',
              'AcademicGroups.cpp', 'AcademicProcessing.cpp', 'AcademicInsights.cpp',
              'GroupRankSimulation.cpp', 'ImprovementSimulator.cpp',
              'ImprovementGoalPlanner.cpp') |
        ForEach-Object { Join-Path $Root ('src\' + $_) }
    Build 'Core regression tests' (@((Join-Path $Root 'tests\test_core.cpp')) + $Core) @() @() 'EduTrackTests.exe'
    Build 'SQLite integration tests' (@((Join-Path $Root 'tests\test_sqlite.cpp'), $Base) + $Support) @() @() 'EduTrackSqliteTests.exe'
    $Workflow = @('Student.cpp', 'StudentManagement.cpp', 'ChangeHistory.cpp',
                  'RequestQueue.cpp', 'GuardedRecordWorkflow.cpp', 'AcademicGroups.cpp') |
        ForEach-Object { Join-Path $Root ('src\' + $_) }
    Build 'Undo and queue regression tests' (@((Join-Path $Root 'tests\test_workflow.cpp')) + $Workflow) @() @() 'EduTrackWorkflowTests.exe'
    Build 'SQLite backup and restore tests' (@((Join-Path $Root 'tests\test_maintenance.cpp'),
            (Join-Path $Root 'database\DatabaseMaintenance.cpp'), $Base) + $Support) @() @() 'EduTrackMaintenanceTests.exe'
    $GuiCore = @('Student.cpp', 'StudentManagement.cpp', 'AcademicGroups.cpp') |
        ForEach-Object { Join-Path $Root ('src\' + $_) }
    Build 'Windows dialog smoke tests' (@((Join-Path $Root 'tests\test_gui_windows.cpp')) + $GuiCore) `
        @() @('-lcomctl32') 'EduTrackGuiTests.exe'
    Write-Host 'Run all five tests with the matching DLLs: .\run_windows.ps1 -Target Tests'
}

if ($Target -eq 'GUI' -or $Target -eq 'All') {
    Write-Host 'Launch the desktop with: .\run_windows.ps1 -Target GUI'
}
