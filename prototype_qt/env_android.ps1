# ==============================================================================
# GeoRescuX Qt Prototype — Project-Local Android Toolchain Session Environment
# ==============================================================================
# Sets process-scoped environment variables and session PATH for Android builds.
# Does NOT modify persistent system environment or registry.

[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"

Write-Host "=================================================================" -ForegroundColor Cyan
Write-Host " [GeoRescuX] Initializing Project-Local Android Toolchain Environment" -ForegroundColor Cyan
Write-Host "=================================================================" -ForegroundColor Cyan

# 1. Base Installation Paths
$JAVA_HOME_PATH    = "C:\Users\sanid\AppData\Local\Programs\jdk-21.0.12.1+1"
$SDK_ROOT_PATH     = "C:\Users\sanid\AppData\Local\Android\Sdk"
$NDK_ROOT_PATH     = "C:\Users\sanid\AppData\Local\Android\Sdk\ndk\27.2.12479018"
$QT_ARM64_PATH     = "C:\Qt\6.11.1\android_arm64_v8a"
$QT_HOST_BIN_PATH  = "C:\Qt\6.11.1\mingw_64\bin"
$CMAKE_BIN_PATH    = "C:\Qt\Tools\CMake_64\bin"
$NINJA_BIN_PATH    = "C:\Qt\Tools\Ninja"
$CMDLINE_TOOLS_BIN = "C:\Users\sanid\AppData\Local\Android\Sdk\cmdline-tools\latest\bin"
$PLATFORM_TOOLS    = "C:\Users\sanid\AppData\Local\Android\Sdk\platform-tools"

# 2. Executable Verification Table
$requiredExecutables = @(
    @{ Name = "JDK 21 Compiler (javac)"; Path = Join-Path $JAVA_HOME_PATH "bin\javac.exe" },
    @{ Name = "JDK 21 Runtime (java)";   Path = Join-Path $JAVA_HOME_PATH "bin\java.exe" },
    @{ Name = "Android NDK Properties";  Path = Join-Path $NDK_ROOT_PATH "source.properties" },
    @{ Name = "Qt CMake Wrapper (ARM64)";Path = Join-Path $QT_ARM64_PATH "bin\qt-cmake.bat" },
    @{ Name = "Qt Android Deployment";   Path = Join-Path $QT_HOST_BIN_PATH "androiddeployqt.exe" },
    @{ Name = "CMake Executable";        Path = Join-Path $CMAKE_BIN_PATH "cmake.exe" },
    @{ Name = "Ninja Executable";        Path = Join-Path $NINJA_BIN_PATH "ninja.exe" },
    @{ Name = "SDK Manager (cmdline)";   Path = Join-Path $CMDLINE_TOOLS_BIN "sdkmanager.bat" },
    @{ Name = "ADB (Platform Tools)";    Path = Join-Path $PLATFORM_TOOLS "adb.exe" }
)

foreach ($item in $requiredExecutables) {
    if (-not (Test-Path $item.Path)) {
        Write-Error "Required tool missing: $($item.Name) at '$($item.Path)'"
        return
    }
}

# 3. Set Process-Scoped Environment Variables (Current Process Only)
$env:JAVA_HOME        = $JAVA_HOME_PATH
$env:ANDROID_SDK_ROOT = $SDK_ROOT_PATH
$env:ANDROID_HOME     = $SDK_ROOT_PATH
$env:ANDROID_NDK_ROOT = $NDK_ROOT_PATH
$env:GRADLE_OPTS      = "-Dorg.gradle.java.home=$JAVA_HOME_PATH"

# 4. Construct Clean Session PATH (Prepend toolchain directories)
$toolchainPaths = @(
    (Join-Path $JAVA_HOME_PATH "bin"),
    $QT_ARM64_PATH,
    (Join-Path $QT_ARM64_PATH "bin"),
    $QT_HOST_BIN_PATH,
    $CMAKE_BIN_PATH,
    $NINJA_BIN_PATH,
    $CMDLINE_TOOLS_BIN,
    $PLATFORM_TOOLS
)

$env:Path = ($toolchainPaths -join ";") + ";" + $env:Path

# 5. Report Detected Tool Versions (No secrets or credentials printed)
Write-Host "`nEnvironment Variables (Process-Local):" -ForegroundColor Green
Write-Host "  JAVA_HOME        : $env:JAVA_HOME"
Write-Host "  ANDROID_SDK_ROOT : $env:ANDROID_SDK_ROOT"
Write-Host "  ANDROID_HOME     : $env:ANDROID_HOME"
Write-Host "  ANDROID_NDK_ROOT : $env:ANDROID_NDK_ROOT"

Write-Host "`nToolchain Verification:" -ForegroundColor Green

# Java version (java -version outputs to stderr by design)
$psi = New-Object System.Diagnostics.ProcessStartInfo
$psi.FileName = "$JAVA_HOME_PATH\bin\java.exe"
$psi.Arguments = "-version"
$psi.RedirectStandardError = $true
$psi.UseShellExecute = $false
$p = [System.Diagnostics.Process]::Start($psi)
$p.WaitForExit()
$javaVer = $p.StandardError.ReadLine().Trim()
$javacVer = (& "$JAVA_HOME_PATH\bin\javac.exe" -version 2>&1).Trim()
Write-Host "  Java Runtime     : $javaVer"
Write-Host "  Java Compiler    : $javacVer"

# NDK version
$ndkRev = (Get-Content (Join-Path $NDK_ROOT_PATH "source.properties") | Select-String "Pkg.Revision").Line.Trim()
$ndkRelease = (Get-Content (Join-Path $NDK_ROOT_PATH "source.properties") | Select-String "Pkg.ReleaseName").Line.Trim()
Write-Host "  Android NDK      : $ndkRev ($ndkRelease)"

# CMake & Ninja
$cmakeVer = (& "$CMAKE_BIN_PATH\cmake.exe" --version | Select-Object -First 1)
$ninjaVer = (& "$NINJA_BIN_PATH\ninja.exe" --version)
Write-Host "  CMake            : $cmakeVer"
Write-Host "  Ninja            : v$ninjaVer"

# Qt
Write-Host "  Qt ARM64 Target  : Qt 6.11.1 (ARM64-v8a) at $QT_ARM64_PATH"
Write-Host "  qt-cmake Script  : $(Join-Path $QT_ARM64_PATH 'bin\qt-cmake.bat')"

# SDK Manager & ADB
$adbVer = (& "$PLATFORM_TOOLS\adb.exe" version | Select-Object -First 1)
Write-Host "  ADB              : $adbVer"

Write-Host "`n[GeoRescuX] Environment initialized successfully. Session ready for Android build.`n" -ForegroundColor Cyan
