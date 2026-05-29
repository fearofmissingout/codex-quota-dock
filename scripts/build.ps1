$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
Set-Location $repoRoot

if (-not $env:GOCACHE) {
    $env:GOCACHE = Join-Path $repoRoot ".gocache"
}

go test ./...
go build -ldflags="-H=windowsgui" -o codex-quota-monitor.exe ./cmd/codex-quota-monitor

Write-Host "Built $repoRoot\codex-quota-monitor.exe"
