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

$previousCGO = $env:CGO_ENABLED
$env:CGO_ENABLED = "0"
Invoke-Checked "go" @("test", "./...")
$env:CGO_ENABLED = $previousCGO

$gcc = Get-Command gcc -ErrorAction SilentlyContinue
if (-not $gcc) {
    throw "Fyne desktop builds require CGO and a C compiler. Install MinGW-w64 or another gcc and make sure gcc is in PATH."
}

$gccDir = Split-Path -Parent $gcc.Source
$env:Path = "$gccDir;$env:Path"
$env:PATH = $env:Path
$env:CC = $gcc.Source
$env:COMPILER_PATH = $gccDir
$env:CGO_ENABLED = "1"
Invoke-Checked "go" @("build", "-buildvcs=false", "-trimpath", "-ldflags=-s -w -H=windowsgui", "-o", "codex-quota-dock.exe", "./cmd/codex-quota-dock")

$strip = Get-Command strip -ErrorAction SilentlyContinue
if ($strip) {
    Invoke-Checked $strip.Source @("--strip-all", "codex-quota-dock.exe")
} else {
    Write-Warning "strip was not found in PATH; built executable still contains debug symbols and will be larger."
}

Write-Host "Built $repoRoot\codex-quota-dock.exe"
