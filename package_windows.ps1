param(
    [string]$Ucrt64 = 'C:\msys64\ucrt64',
    [string]$Destination = (Join-Path $PSScriptRoot 'dist\EduTrack')
)

$ErrorActionPreference = 'Stop'
$Root = $PSScriptRoot
$Bin = Join-Path $Ucrt64 'bin'
$Objdump = Join-Path $Bin 'objdump.exe'
$Exe = Join-Path $Root 'EduTrack.exe'
$Output = [System.IO.Path]::GetFullPath($Destination)

if (-not (Test-Path -LiteralPath $Exe -PathType Leaf)) {
    throw 'EduTrack.exe not found. Run .\build_windows.ps1 -Target GUI first.'
}
if (-not (Test-Path -LiteralPath $Objdump -PathType Leaf)) {
    throw "UCRT64 objdump was not found: $Objdump"
}
if ($Output.TrimEnd('\') -ieq $Root.TrimEnd('\')) {
    throw 'Choose a separate output folder. The project folder must not be used as the package destination.'
}
if (Test-Path -LiteralPath $Output) {
    throw "Package destination already exists: $Output. Rename it or choose a fresh -Destination. Existing data is not removed."
}

$Created = $false
try {
    New-Item -ItemType Directory -Path $Output -Force | Out-Null
    $Created = $true

    $PackagedExe = Join-Path $Output 'EduTrack.exe'
    Copy-Item -LiteralPath $Exe -Destination $PackagedExe -ErrorAction Stop

    $Seen = @{}
    $Pending = [System.Collections.Generic.Queue[string]]::new()
    $Pending.Enqueue($PackagedExe)

    while ($Pending.Count -gt 0) {
        $Binary = $Pending.Dequeue()
        $Imports = @(& $Objdump '-p' $Binary)
        if ($LASTEXITCODE -ne 0) {
            throw "Could not inspect DLL dependencies for $Binary (exit $LASTEXITCODE)."
        }

        foreach ($Line in $Imports) {
            if ($Line -notmatch '^\s*DLL Name:\s*(\S+)') { continue }
            $DllName = $Matches[1]
            if ($Seen.ContainsKey($DllName)) { continue }
            $Seen[$DllName] = $true

            $RuntimeDll = Join-Path $Bin $DllName
            if (Test-Path -LiteralPath $RuntimeDll -PathType Leaf) {
                $TargetDll = Join-Path $Output $DllName
                Copy-Item -LiteralPath $RuntimeDll -Destination $TargetDll -ErrorAction Stop
                $Pending.Enqueue($TargetDll)
                continue
            }

            if ($DllName -match '^(api-ms-win-|ext-ms-win-)') { continue }
            if ($env:SystemRoot) {
                $WindowsDll = Join-Path (Join-Path $env:SystemRoot 'System32') $DllName
                if (Test-Path -LiteralPath $WindowsDll -PathType Leaf) { continue }
            }
            throw "Required DLL '$DllName' was not found in UCRT64 or Windows System32."
        }
    }

    if (-not (Test-Path -LiteralPath (Join-Path $Output 'libsqlite3-0.dll') -PathType Leaf)) {
        throw 'SQLite runtime was not packaged. Check that the executable uses the expected UCRT64 SQLite library.'
    }

    $Dlls = @(Get-ChildItem -LiteralPath $Output -Filter '*.dll' -File | Sort-Object Name)
    $Manifest = @(
        'EduTrack for Windows'
        ''
        'Double-click EduTrack.exe inside this folder.'
        'Keep EduTrack.exe and all of the supplied DLL files together.'
        'The DLLs are runtime dependencies from the matching MSYS2 UCRT64 installation.'
        'User records are not included in this package.'
        ''
        'Bundled DLLs (SHA256):'
    )
    foreach ($Dll in $Dlls) {
        $Hash = (Get-FileHash -LiteralPath $Dll.FullName -Algorithm SHA256).Hash
        $Manifest += ('{0}  {1}' -f $Dll.Name, $Hash)
    }
    $Manifest | Set-Content -LiteralPath (Join-Path $Output 'README_FIRST.txt') -Encoding UTF8

    Write-Host "Packaged EduTrack.exe and $($Dlls.Count) runtime DLL(s) in $Output" -ForegroundColor Green
    Write-Host 'Open the packaged EduTrack.exe, not the standalone executable in the source folder.'
} catch {
    if ($Created) {
        Remove-Item -LiteralPath $Output -Recurse -Force -ErrorAction SilentlyContinue
    }
    throw
}
