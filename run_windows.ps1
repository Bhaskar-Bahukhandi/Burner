# Run EduTrack with the matching MSYS2 UCRT64 runtime DLLs.
# Examples:
#   .\run_windows.ps1 -Target GUI
#   .\run_windows.ps1 -Target Tests
#   .\run_windows.ps1 -Target Maintenance -Arguments @('verify', 'edutrack.db')
param(
    [ValidateSet('GUI', 'Tests', 'Console', 'Importer', 'Maintenance')]
    [string]$Target = 'GUI',
    [string]$Ucrt64 = 'C:\msys64\ucrt64',
    [string[]]$Arguments = @()
)

$ErrorActionPreference = 'Stop'
$Root = $PSScriptRoot
$Bin = Join-Path $Ucrt64 'bin'
$SQLiteDll = Join-Path $Bin 'libsqlite3-0.dll'

if (-not (Test-Path -LiteralPath $Bin -PathType Container)) {
    throw "MSYS2 UCRT64 runtime folder not found: $Bin. Pass -Ucrt64 with your installation folder."
}
if (-not (Test-Path -LiteralPath $SQLiteDll -PathType Leaf)) {
    throw "SQLite runtime DLL missing: $SQLiteDll. In MSYS2 UCRT64, run: pacman -S --needed mingw-w64-ucrt-x86_64-sqlite3"
}

# Keep the working directory predictable so database paths resolve inside
# the project folder, even when this script is run from somewhere else.
$OldPath = $env:PATH
$env:PATH = $Bin + ';' + $OldPath
Push-Location -LiteralPath $Root
try {
    function Invoke-EduTrack([string]$Executable, [string]$BuildTarget, [string[]]$ProgramArgs) {
        $Path = Join-Path $Root $Executable
        if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
            throw "$Executable not found. Build it using: .\build_windows.ps1 -Target $BuildTarget"
        }
        if ($BuildTarget -eq 'GUI') {
            # A Windows GUI program can return control to PowerShell before it exits.
            # Start-Process reports the new process without relying on $LASTEXITCODE.
            $StartOptions = @{
                FilePath = $Path
                WorkingDirectory = $Root
                PassThru = $true
            }
            if ($ProgramArgs.Count -gt 0) {
                $StartOptions.ArgumentList = $ProgramArgs
            }
            $Process = Start-Process @StartOptions
            Write-Host "Started $Executable (PID $($Process.Id))." -ForegroundColor Green
            return
        }
        Write-Host "Running $Executable..."
        & $Path @ProgramArgs
        $ExitCode = $LASTEXITCODE
        if ($ExitCode -ne 0) {
            if ($ExitCode -eq -1073741515) {
                throw "$Executable could not load a DLL (0xC0000135). Verify the UCRT64/SQLite runtime in $Bin."
            }
            throw "$Executable exited with code $ExitCode."
        }
        Write-Host "PASS: $Executable (exit 0)" -ForegroundColor Green
    }
    switch ($Target) {
        'GUI' { Invoke-EduTrack 'EduTrack.exe' 'GUI' $Arguments }
        'Console' { Invoke-EduTrack 'EduTrackConsole.exe' 'Console' $Arguments }
        'Importer' { Invoke-EduTrack 'edutrack-migrate.exe' 'Importer' $Arguments }
        'Maintenance' { Invoke-EduTrack 'edutrack-db.exe' 'Maintenance' $Arguments }
        'Tests' {
            if ($Arguments.Count -gt 0) { throw 'Do not pass -Arguments with -Target Tests.' }
            foreach ($Name in @('EduTrackTests.exe', 'EduTrackValidationTests.exe', 'EduTrackSqliteTests.exe',
                                'EduTrackWorkflowTests.exe', 'EduTrackMaintenanceTests.exe',
                                'EduTrackGuiTests.exe')) {
                Invoke-EduTrack $Name 'Tests' @()
            }
            Write-Host 'All six tests completed successfully.' -ForegroundColor Green
        }
    }
} finally {
    Pop-Location
    $env:PATH = $OldPath
}
