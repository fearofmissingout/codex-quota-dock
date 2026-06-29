param(
    [string]$Configuration = "Release",
    [string]$Arch = "x64",
    [string]$Generator = $env:CQD_CMAKE_GENERATOR
)

$ErrorActionPreference = "Stop"
if ($PSVersionTable.PSVersion.Major -ge 7) {
    $PSNativeCommandUseErrorActionPreference = $true
}

$Root = Resolve-Path (Join-Path $PSScriptRoot "..")
$Source = Join-Path $Root "native\windows\CodexQuotaDock"
$Build = Join-Path $Root "build\native-windows-$Arch"
$Dist = Join-Path $Root "dist"
$Zip = Join-Path $Dist "codex-quota-dock-native-windows-amd64.zip"

function Invoke-Native {
    param([Parameter(Mandatory = $true)][string]$FilePath, [string[]]$Arguments)
    & $FilePath @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "$FilePath failed with exit code $LASTEXITCODE"
    }
}

function Select-CMakeGenerator {
    if (-not [string]::IsNullOrWhiteSpace($Generator)) {
        return $Generator
    }

    $capabilitiesJson = & cmake -E capabilities
    if ($LASTEXITCODE -ne 0) {
        throw "cmake -E capabilities failed with exit code $LASTEXITCODE"
    }

    $capabilities = $capabilitiesJson | ConvertFrom-Json
    $names = @($capabilities.generators | ForEach-Object { $_.name })
    foreach ($candidate in @("Visual Studio 18 2026", "Visual Studio 17 2022")) {
        if ($names -contains $candidate) {
            return $candidate
        }
    }

    return ($names | Where-Object { $_ -like "Visual Studio *" } | Select-Object -First 1)
}

New-Item -ItemType Directory -Force $Build | Out-Null
New-Item -ItemType Directory -Force $Dist | Out-Null

$selectedGenerator = Select-CMakeGenerator
$configureArgs = @("-S", $Source, "-B", $Build)
if (-not [string]::IsNullOrWhiteSpace($selectedGenerator)) {
    $configureArgs += @("-G", $selectedGenerator)
    if ($selectedGenerator -like "Visual Studio *") {
        $configureArgs += @("-A", $Arch)
    }
} else {
    $configureArgs += @("-DCMAKE_BUILD_TYPE=$Configuration")
}

Invoke-Native "cmake" $configureArgs
Invoke-Native "cmake" @("--build", $Build, "--config", $Configuration)
Invoke-Native "ctest" @("--test-dir", $Build, "-C", $Configuration, "--output-on-failure")

$exeCandidates = @(
    (Join-Path $Build "$Configuration\codex-quota-dock-native.exe"),
    (Join-Path $Build "codex-quota-dock-native.exe")
)
$Exe = $exeCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $Exe) {
    throw "codex-quota-dock-native.exe was not produced under $Build"
}

if (Test-Path $Zip) {
    Remove-Item $Zip -Force
}
Compress-Archive -Path $Exe -DestinationPath $Zip
Write-Host "Built $Zip"
