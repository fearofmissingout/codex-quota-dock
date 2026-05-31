@echo off
setlocal

cd /d "%~dp0.."

set CGO_ENABLED=0
go test ./...
if errorlevel 1 exit /b %errorlevel%

set "GCC_PATH="
for /f "delims=" %%G in ('where gcc 2^>nul') do (
  if not defined GCC_PATH set "GCC_PATH=%%G"
)
if not defined GCC_PATH (
  echo Fyne desktop builds require CGO and a C compiler. Install MinGW-w64 or another gcc and make sure gcc is in PATH.
  exit /b 1
)
for %%G in ("%GCC_PATH%") do set "GCC_DIR=%%~dpG"
set "PATH=%GCC_DIR%;%PATH%"
set "CC=%GCC_PATH%"
set "COMPILER_PATH=%GCC_DIR%"

set CGO_ENABLED=1
go build -buildvcs=false -trimpath -ldflags="-s -w -H=windowsgui" -o codex-quota-dock.exe ./cmd/codex-quota-dock
if errorlevel 1 exit /b %errorlevel%

where strip >nul 2>nul
if not errorlevel 1 (
  strip --strip-all codex-quota-dock.exe
  if errorlevel 1 exit /b %errorlevel%
) else (
  echo strip was not found in PATH; built executable still contains debug symbols and will be larger.
)

echo Built %CD%\codex-quota-dock.exe
