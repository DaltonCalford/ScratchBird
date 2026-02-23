param(
    [string]$RepoRoot = "",
    [string]$VcpkgRoot = "",
    [string]$Baseline = "05442024c3fda64320bd25d2251cc9807b84fb6f",
    [string]$Triplet = "x64-windows"
)

$ErrorActionPreference = "Stop"

if ([string]::IsNullOrWhiteSpace($RepoRoot)) {
    $RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
}
if ([string]::IsNullOrWhiteSpace($VcpkgRoot)) {
    $VcpkgRoot = (Join-Path $RepoRoot "build\vcpkg")
}

Write-Host "Repo root: $RepoRoot"
Write-Host "Vcpkg root: $VcpkgRoot"
Write-Host "Baseline : $Baseline"
Write-Host "Triplet  : $Triplet"

if (-not (Test-Path $VcpkgRoot)) {
    git clone https://github.com/microsoft/vcpkg.git $VcpkgRoot
}

Push-Location $VcpkgRoot
git fetch --all --tags
git checkout $Baseline
.\bootstrap-vcpkg.bat -disableMetrics
Pop-Location

$env:VCPKG_ROOT = $VcpkgRoot

& (Join-Path $VcpkgRoot "vcpkg.exe") install `
    --x-manifest-root=$RepoRoot `
    --triplet $Triplet `
    --x-feature=manifests

Write-Host "vcpkg bootstrap complete."
Write-Host "Next: cmake --preset windows-msvc-debug"
