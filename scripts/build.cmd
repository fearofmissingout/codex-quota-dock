@echo off
setlocal

cd /d "%~dp0.."

if not defined GOCACHE set "GOCACHE=%CD%\.gocache"

go test ./...
if errorlevel 1 exit /b %errorlevel%

go build -ldflags="-H=windowsgui" -o codex-quota-monitor.exe ./cmd/codex-quota-monitor
if errorlevel 1 exit /b %errorlevel%

echo Built %CD%\codex-quota-monitor.exe
