<#
.SYNOPSIS
Downloads the Microsoft Visual C++ 2015-2022 Redistributable (x64) for RTRT game packages.

.PARAMETER OutPath
Target path or directory to save vc_redist.x64.exe. Default is the current directory.

.PARAMETER Force
Forces redownload even if the file already exists.
#>
param(
    [string]$OutPath = ".",
    [switch]$Force
)

$ErrorActionPreference = "Stop"

$url = "https://aka.ms/vs/17/release/vc_redist.x64.exe"

# Resolve target file path
if (Test-Path -Path $OutPath -PathType Container) {
    $targetFile = Join-Path -Path $OutPath -ChildPath "vc_redist.x64.exe"
} else {
    $targetFile = $OutPath
}

$parentDir = Split-Path -Path $targetFile -Parent
if ($parentDir -and !(Test-Path -Path $parentDir)) {
    New-Item -ItemType Directory -Path $parentDir -Force | Out-Null
}

if ((Test-Path -Path $targetFile) -and !$Force) {
    $existingSize = (Get-Item -Path $targetFile).Length
    if ($existingSize -gt 10MB) {
        Write-Host "vc_redist.x64.exe already exists ($([math]::Round($existingSize / 1MB, 2)) MB). Skipping download." -ForegroundColor Green
        return $targetFile
    }
}

Write-Host "Downloading Microsoft Visual C++ 2015-2022 Redistributable (x64)..." -ForegroundColor Cyan
Write-Host "URL: $url" -ForegroundColor Gray
Write-Host "Target: $targetFile" -ForegroundColor Gray

try {
    # Prefer curl.exe if available to show native progress, otherwise Invoke-WebRequest
    if (Get-Command curl.exe -ErrorAction SilentlyContinue) {
        & curl.exe -L -o $targetFile $url
    } else {
        [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
        Invoke-WebRequest -Uri $url -OutFile $targetFile -UseBasicParsing
    }

    if (!(Test-Path -Path $targetFile) -or ((Get-Item -Path $targetFile).Length -lt 5MB)) {
        throw "Downloaded file appears invalid or incomplete (< 5 MB)."
    }

    $finalSize = (Get-Item -Path $targetFile).Length
    Write-Host "Successfully downloaded vc_redist.x64.exe ($([math]::Round($finalSize / 1MB, 2)) MB)!" -ForegroundColor Green
    return $targetFile
} catch {
    Write-Error "Failed to download Visual C++ Redistributable: $_"
    exit 1
}
