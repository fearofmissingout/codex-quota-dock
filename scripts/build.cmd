@echo off
setlocal

cd /d "%~dp0.."

if not defined GOCACHE set "GOCACHE=%CD%\.gocache"

go test ./...
if errorlevel 1 exit /b %errorlevel%

go build -ldflags="-H=windowsgui" -o codex-quota-dock.exe ./cmd/codex-quota-dock
if errorlevel 1 exit /b %errorlevel%

echo Built %CD%\codex-quota-dock.exe
