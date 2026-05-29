$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
Set-Location $repoRoot

if (-not $env:GOCACHE) {
    $env:GOCACHE = Join-Path $repoRoot ".gocache"
}

go test ./...
go build -ldflags="-H=windowsgui" -o codex-quota-dock.exe ./cmd/codex-quota-dock

Write-Host "Built $repoRoot\codex-quota-dock.exe"
