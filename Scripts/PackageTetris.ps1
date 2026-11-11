<#
.SYNOPSIS
Automated build, test, and CPack packaging script for RTRT Tetris Game.

.PARAMETER Config
Build configuration: Release (default), RelWithDebInfo, or Debug.

.PARAMETER Generator
CPack generator: ZIP (default), 7Z, or NSIS.

.PARAMETER OutputDir
Target directory for generated packages. Default: "Dist".

.PARAMETER IncludeRedist
Download and bundle the official Microsoft Visual C++ 2015-2022 Redistributable (x64). Enabled by default.

.PARAMETER SkipBuild
Skip CMake compilation if the target is already built.

.PARAMETER SkipTests
Skip pre-package headless unit tests.

.PARAMETER NoVerify
Skip the post-package sandbox extraction and test verification.
#>
param(
    [string]$Config = "Release",
    [string]$Generator = "ZIP",
    [string]$OutputDir = "Dist",
    [bool]$IncludeRedist = $true,
    [switch]$SkipBuild,
    [switch]$SkipTests,
    [switch]$NoVerify
)

$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = (Resolve-Path (Join-Path $scriptDir "..")).Path
Set-Location $repoRoot

Write-Host "========================================================================" -ForegroundColor Cyan
Write-Host "             RTRT TETRIS GAME - PACKAGING PIPELINE                      " -ForegroundColor Cyan
Write-Host "========================================================================" -ForegroundColor Cyan
Write-Host "Configuration : $Config" -ForegroundColor Gray
Write-Host "CPack Format  : $Generator" -ForegroundColor Gray
Write-Host "Output Folder : $OutputDir" -ForegroundColor Gray
Write-Host "Repo Root     : $repoRoot" -ForegroundColor Gray
Write-Host ""

# -----------------------------------------------------------------------------
# 1. Visual C++ Redistributable Download
# -----------------------------------------------------------------------------
if ($IncludeRedist) {
    $redistTarget = Join-Path $repoRoot "Build\vc_redist.x64.exe"
    if (!(Test-Path $redistTarget) -or ((Get-Item $redistTarget).Length -lt 10MB)) {
        Write-Host "--> [1/5] Downloading Microsoft Visual C++ Redistributable..." -ForegroundColor Yellow
        & powershell -ExecutionPolicy Bypass -File (Join-Path $scriptDir "DownloadVcRedist.ps1") -OutPath (Join-Path $repoRoot "Build")
    } else {
        Write-Host "--> [1/5] Visual C++ Redistributable already staged in Build/." -ForegroundColor Green
    }
} else {
    Write-Host "--> [1/5] Skipping VC++ Redistributable bundling (-IncludeRedist:$false)." -ForegroundColor Gray
}

# -----------------------------------------------------------------------------
# 2. CMake Configure & Build
# -----------------------------------------------------------------------------
Write-Host "--> [2/5] Configuring CMake..." -ForegroundColor Yellow
& cmake -B Build -S .
if ($LASTEXITCODE -ne 0) {
    throw "CMake configuration failed."
}

if (!$SkipBuild) {
    Write-Host "--> Building TetrisGame ($Config)..." -ForegroundColor Yellow
    & cmake --build Build --config $Config --target TetrisGame
    if ($LASTEXITCODE -ne 0) {
        throw "Build failed for target TetrisGame ($Config)."
    }
} else {
    Write-Host "--> Skipping build step (-SkipBuild specified)." -ForegroundColor Gray
}

# -----------------------------------------------------------------------------
# 3. Pre-Package Headless Tests
# -----------------------------------------------------------------------------
$exePath = Join-Path $repoRoot "Build\$Config\TetrisGame.exe"
if (!(Test-Path $exePath)) {
    $exePath = Join-Path $repoRoot "Build\TetrisGame.exe"
}
if (!(Test-Path $exePath)) {
    throw "Could not locate built executable at: $exePath"
}

if (!$SkipTests) {
    Write-Host "--> [3/5] Running headless Tetris Guideline unit tests..." -ForegroundColor Yellow
    $p = Start-Process -FilePath $exePath -ArgumentList "--unit-tests" -WorkingDirectory (Split-Path -Parent $exePath) -Wait -NoNewWindow -PassThru
    if ($p.ExitCode -ne 0) {
        throw "Pre-package unit tests failed with exit code $($p.ExitCode)!"
    }
    Write-Host "    All unit tests PASSED." -ForegroundColor Green
} else {
    Write-Host "--> [3/5] Skipping pre-package unit tests (-SkipTests)." -ForegroundColor Gray
}

# -----------------------------------------------------------------------------
# 4. CPack Packaging
# -----------------------------------------------------------------------------
Write-Host "--> [4/5] Running CPack packaging (Generator: $Generator)..." -ForegroundColor Yellow
$distFullPath = Join-Path $repoRoot $OutputDir
if (!(Test-Path $distFullPath)) {
    New-Item -ItemType Directory -Path $distFullPath -Force | Out-Null
}

& cpack --config Build/CPackConfig.cmake -C $Config -G $Generator -B $distFullPath
if ($LASTEXITCODE -ne 0) {
    throw "CPack packaging failed."
}

# Locate the newly generated archive
$packageFiles = Get-ChildItem -Path $distFullPath -Filter "TetrisGame*.$($Generator.ToLower())" | Sort-Object LastWriteTime -Descending
if ($packageFiles.Count -eq 0) {
    $packageFiles = Get-ChildItem -Path $distFullPath -File | Sort-Object LastWriteTime -Descending
}

if ($packageFiles.Count -eq 0) {
    throw "No package was found in $distFullPath after CPack completed."
}

$generatedPackage = $packageFiles[0]
$packageSizeMB = [math]::Round($generatedPackage.Length / 1MB, 2)
Write-Host "    Package generated: $($generatedPackage.Name) ($packageSizeMB MB)" -ForegroundColor Green

# -----------------------------------------------------------------------------
# 5. Sandbox Extraction & Verification
# -----------------------------------------------------------------------------
if (!$NoVerify -and ($generatedPackage.Extension -eq ".zip")) {
    Write-Host "--> [5/5] Performing Sandbox Verification on isolated package..." -ForegroundColor Yellow
    $tempDir = Join-Path $env:TEMP ("TetrisVerify_" + [Guid]::NewGuid().ToString("N").Substring(0, 8))
    New-Item -ItemType Directory -Path $tempDir -Force | Out-Null

    try {
        Write-Host "    Extracting to sandbox: $tempDir" -ForegroundColor Gray
        Expand-Archive -Path $generatedPackage.FullName -DestinationPath $tempDir -Force

        # Locate the executable in the sandbox
        $sandboxedExe = Get-ChildItem -Path $tempDir -Filter "TetrisGame.exe" -Recurse | Select-Object -First 1
        if (!$sandboxedExe) {
            throw "Sandbox verification failed: TetrisGame.exe not found in package!"
        }

        $sandboxRoot = $sandboxedExe.DirectoryName
        Write-Host "    Found sandboxed executable: $($sandboxedExe.FullName)" -ForegroundColor Gray

        # Verify key directories and files exist in sandbox
        $requiredItems = @("Shaders", "Assets\HDR", "README.txt")
        foreach ($item in $requiredItems) {
            $itemPath = Join-Path $sandboxRoot $item
            if (!(Test-Path $itemPath)) {
                throw "Sandbox verification failed: Required item missing in package: $item"
            }
        }

        # Run headless unit tests inside the sandbox
        Write-Host "    Running isolated unit tests..." -ForegroundColor Gray
        $p = Start-Process -FilePath $sandboxedExe.FullName -ArgumentList "--unit-tests" -WorkingDirectory $sandboxRoot -Wait -NoNewWindow -PassThru
        if ($p.ExitCode -ne 0) {
            throw "Sandbox test failed: Isolated unit tests exited with error code $($p.ExitCode)"
        }

        # Run headless rendering and screenshot inside the sandbox
        $verifyShot = Join-Path $sandboxRoot "verify_render.png"
        Write-Host "    Running isolated rendering verification (15 frames headless)..." -ForegroundColor Gray
        $p = Start-Process -FilePath $sandboxedExe.FullName -ArgumentList @("--screenshot", $verifyShot) -WorkingDirectory $sandboxRoot -Wait -NoNewWindow -PassThru
        if ($p.ExitCode -ne 0 -or !(Test-Path $verifyShot)) {
            throw "Sandbox test failed: Isolated headless rendering failed to create screenshot."
        }

        Write-Host "    Sandbox verification completed successfully!" -ForegroundColor Green
    }
    finally {
        if (Test-Path $tempDir) {
            Remove-Item -Path $tempDir -Recurse -Force -ErrorAction SilentlyContinue
        }
    }
} else {
    Write-Host "--> [5/5] Sandbox verification skipped." -ForegroundColor Gray
}

# -----------------------------------------------------------------------------
# Summary & Checksum
# -----------------------------------------------------------------------------
$hash = (Get-FileHash -Path $generatedPackage.FullName -Algorithm SHA256).Hash
Write-Host ""
Write-Host "========================================================================" -ForegroundColor Green
Write-Host "                PACKAGING COMPLETED SUCCESSFULLY!                       " -ForegroundColor Green
Write-Host "========================================================================" -ForegroundColor Green
Write-Host "File     : $($generatedPackage.FullName)" -ForegroundColor Cyan
Write-Host "Size     : $packageSizeMB MB" -ForegroundColor Cyan
Write-Host "SHA-256  : $hash" -ForegroundColor Gray
Write-Host "Ready to distribute to players!" -ForegroundColor Green
