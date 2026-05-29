@echo off
setlocal

cd /d "%~dp0.."

if not defined GOCACHE set "GOCACHE=%CD%\.gocache"

go test ./...
if errorlevel 1 exit /b %errorlevel%

where gcc >nul 2>nul
if errorlevel 1 (
  echo Fyne desktop builds require CGO and a C compiler. Install MinGW-w64 or another gcc and make sure gcc is in PATH.
  exit /b 1
)

set CGO_ENABLED=1
go build -o codex-quota-dock.exe ./cmd/codex-quota-dock
if errorlevel 1 exit /b %errorlevel%

echo Built %CD%\codex-quota-dock.exe
