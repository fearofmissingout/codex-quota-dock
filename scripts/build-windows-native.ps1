param(
    [string]$Configuration = "Release",
    [string]$Arch = "x64"
)

$ErrorActionPreference = "Stop"

$Root = Resolve-Path (Join-Path $PSScriptRoot "..")
$Source = Join-Path $Root "native\windows\CodexQuotaDock"
$Build = Join-Path $Root "build\native-windows-$Arch"
$Dist = Join-Path $Root "dist"
$Exe = Join-Path $Build "$Configuration\codex-quota-dock-native.exe"
$Zip = Join-Path $Dist "codex-quota-dock-native-windows-amd64.zip"

New-Item -ItemType Directory -Force $Build | Out-Null
New-Item -ItemType Directory -Force $Dist | Out-Null

cmake -S $Source -B $Build -G "Visual Studio 17 2022" -A $Arch
cmake --build $Build --config $Configuration
ctest --test-dir $Build -C $Configuration --output-on-failure

if (Test-Path $Zip) {
    Remove-Item $Zip -Force
}
Compress-Archive -Path $Exe -DestinationPath $Zip
Write-Host "Built $Zip"
