$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
Set-Location $repoRoot

if (-not $env:GOCACHE) {
    $env:GOCACHE = Join-Path $repoRoot ".gocache"
}

go test ./...

if (-not (Get-Command gcc -ErrorAction SilentlyContinue)) {
    throw "Fyne desktop builds require CGO and a C compiler. Install MinGW-w64 or another gcc and make sure gcc is in PATH."
}

$env:CGO_ENABLED = "1"
go build -o codex-quota-dock.exe ./cmd/codex-quota-dock

Write-Host "Built $repoRoot\codex-quota-dock.exe"
