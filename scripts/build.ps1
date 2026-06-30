$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
Set-Location $repoRoot

function Invoke-Checked {
    param(
        [Parameter(Mandatory = $true)]
        [string]$FilePath,

        [string[]]$Arguments
    )

    & $FilePath @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "$FilePath failed with exit code $LASTEXITCODE"
    }
}

Invoke-Checked "go" @("run", "./cmd/generate-icons")

$previousCGO = $env:CGO_ENABLED
$env:CGO_ENABLED = "0"
Invoke-Checked "go" @("test", "./...")
$env:CGO_ENABLED = $previousCGO

$gcc = Get-Command gcc -ErrorAction SilentlyContinue
if (-not $gcc) {
    $fallbackGcc = "C:\msys64\ucrt64\bin\gcc.exe"
    if (Test-Path $fallbackGcc) {
        $gcc = Get-Item $fallbackGcc
    }
}
if (-not $gcc) {
    throw "Fyne desktop builds require CGO and a C compiler. Install MinGW-w64 or another gcc and make sure gcc is in PATH."
}

$gccPath = $gcc.Source
if (-not $gccPath) {
    $gccPath = $gcc.FullName
}
$gccDir = Split-Path -Parent $gccPath
$env:Path = "$gccDir;$env:Path"
$env:PATH = $env:Path
$env:CC = $gccPath
$env:COMPILER_PATH = $gccDir
$env:CGO_ENABLED = "1"
$windres = Get-Command windres -ErrorAction SilentlyContinue
if (-not $windres) {
    $fallbackWindres = "C:\msys64\ucrt64\bin\windres.exe"
    if (Test-Path $fallbackWindres) {
        $windres = Get-Item $fallbackWindres
        $env:Path = "$(Split-Path -Parent $fallbackWindres);$env:Path"
        $env:PATH = $env:Path
    }
}
if ($windres) {
    $windresPath = $windres.Source
    if (-not $windresPath) {
        $windresPath = $windres.FullName
    }
    Invoke-Checked $windresPath @("-i", "cmd/codex-quota-dock/app_windows.rc", "-O", "coff", "-o", "cmd/codex-quota-dock/app_windows.syso")
} else {
    Write-Warning "windres was not found; the built executable may not contain the taskbar icon resource."
}
Invoke-Checked "go" @("build", "-buildvcs=false", "-trimpath", "-ldflags=-s -w -H=windowsgui -X github.com/fearofmissingout/codex-quota-dock/internal/version.Version=0.6.1", "-o", "codex-quota-dock.exe", "./cmd/codex-quota-dock")

$strip = Get-Command strip -ErrorAction SilentlyContinue
if ($strip) {
    Invoke-Checked $strip.Source @("--strip-all", "codex-quota-dock.exe")
} else {
    Write-Warning "strip was not found in PATH; built executable still contains debug symbols and will be larger."
}

Write-Host "Built $repoRoot\codex-quota-dock.exe"
