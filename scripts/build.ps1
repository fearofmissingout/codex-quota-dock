$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
Set-Location $repoRoot

$previousCGO = $env:CGO_ENABLED
$env:CGO_ENABLED = "0"
go test ./...
$env:CGO_ENABLED = $previousCGO

if (-not (Get-Command gcc -ErrorAction SilentlyContinue)) {
    throw "Fyne desktop builds require CGO and a C compiler. Install MinGW-w64 or another gcc and make sure gcc is in PATH."
}

$env:CGO_ENABLED = "1"
go build -ldflags="-H=windowsgui" -o codex-quota-dock.exe ./cmd/codex-quota-dock

$strip = Get-Command strip -ErrorAction SilentlyContinue
if ($strip) {
    & $strip.Source --strip-all codex-quota-dock.exe
} else {
    Write-Warning "strip was not found in PATH; built executable still contains debug symbols and will be larger."
}

Write-Host "Built $repoRoot\codex-quota-dock.exe"
