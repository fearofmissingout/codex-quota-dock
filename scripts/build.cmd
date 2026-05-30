@echo off
setlocal

cd /d "%~dp0.."

set CGO_ENABLED=0
go test ./...
if errorlevel 1 exit /b %errorlevel%

where gcc >nul 2>nul
if errorlevel 1 (
  echo Fyne desktop builds require CGO and a C compiler. Install MinGW-w64 or another gcc and make sure gcc is in PATH.
  exit /b 1
)

set CGO_ENABLED=1
go build -ldflags="-H=windowsgui" -o codex-quota-dock.exe ./cmd/codex-quota-dock
if errorlevel 1 exit /b %errorlevel%

where strip >nul 2>nul
if not errorlevel 1 (
  strip --strip-all codex-quota-dock.exe
) else (
  echo strip was not found in PATH; built executable still contains debug symbols and will be larger.
)

echo Built %CD%\codex-quota-dock.exe
