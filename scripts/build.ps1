param(
    [string]$QtPrefix = 'C:/msys64/ucrt64',
    [string]$Toolchain = 'C:/msys64/ucrt64',
    [string]$BuildRoot = "$PSScriptRoot/../build"
)
$ErrorActionPreference = 'Stop'
$project = Split-Path $PSScriptRoot -Parent
$suyuSource = Join-Path $project 'third_party/suyu'
if (-not [IO.Path]::IsPathRooted($BuildRoot)) { $BuildRoot = Join-Path $project $BuildRoot }
$BuildRoot = [IO.Path]::GetFullPath($BuildRoot)
$suyuBuild = Join-Path $BuildRoot 'suyu-core'
$mainBuild = $BuildRoot
$cpmCache = Join-Path $BuildRoot 'cpm-source-cache'
$null = New-Item -ItemType Directory -Path $BuildRoot -Force
$null = New-Item -ItemType Directory -Path $cpmCache -Force
$QtPrefix = (Resolve-Path $QtPrefix).Path
$env:PATH = "$QtPrefix/bin;$Toolchain/bin;" + $env:PATH

if (-not (Test-Path -LiteralPath (Join-Path $suyuSource 'GIT-COMMIT'))) { throw 'Vendored Suyu source snapshot is missing.' }
if (-not (Test-Path -LiteralPath (Join-Path $suyuSource 'externals/dynarmic/src'))) { throw 'Pinned Dynarmic source is missing.' }

$suyuArgs = @(
    '-S', $suyuSource, '-B', $suyuBuild, '-G', 'Ninja',
    "-DCMAKE_C_COMPILER=$Toolchain/bin/gcc.exe",
    "-DCMAKE_CXX_COMPILER=$Toolchain/bin/g++.exe",
    '-DCMAKE_C_FLAGS=-DARCHITECTURE_x86_64=1',
    '-DCMAKE_CXX_FLAGS=-DARCHITECTURE_x86_64=1',
    '-DCMAKE_BUILD_TYPE=Release', '-DCMAKE_EXPORT_COMPILE_COMMANDS=ON',
    '-DARCHITECTURE=x86_64', '-DARCHITECTURE_x86_64=ON',
    "-DCPM_SOURCE_CACHE=$cpmCache",
    '-DVulkanHeaders_FORCE_BUNDLED=ON', '-DVulkanUtilityLibraries_FORCE_BUNDLED=ON',
    '-DENABLE_QT=OFF', '-DENABLE_QT_TRANSLATION=OFF', '-DYUZU_CMD=OFF',
    '-DYUZU_TESTS=OFF', '-DYUZU_CRASH_DUMPS=OFF',
    '-DENABLE_WEB_SERVICE=OFF', '-DENABLE_UPDATE_CHECKER=OFF', '-DUSE_DISCORD_PRESENCE=OFF',
    '-DENABLE_OPENGL=ON', '-DENABLE_LIBUSB=ON', '-DENABLE_SDL3=ON', '-DENABLE_CUBEB=ON',
    '-DYUZU_USE_BUNDLED_FFMPEG=OFF', '-DYUZU_USE_BUNDLED_SDL3=OFF', '-DYUZU_USE_BUNDLED_SIRIT=OFF',
    # Keep room-related core support enabled; skip the standalone room executable.
    '-DYUZU_ROOM=ON', '-DYUZU_ROOM_STANDALONE=OFF', '-DSUYU_NO_JIT=OFF',
    '-DSUYU_BUILD_LIBRETRO_CORE=OFF', '-DSUYU_BUILD_IOS_CORE_PROBE=OFF', '-DSUYU_EXPORT_BUILD_KIT=OFF',
    '-DYUZU_DOWNLOAD_TIME_ZONE_DATA=ON'
)
& cmake @suyuArgs
if ($LASTEXITCODE) { exit $LASTEXITCODE }
cmake --build $suyuBuild --target core frontend_common input_common --parallel
if ($LASTEXITCODE) { exit $LASTEXITCODE }

$shireiaArgs = @(
    '-S', $project, '-B', $mainBuild, '-G', 'Ninja',
    "-DCMAKE_C_COMPILER=$Toolchain/bin/gcc.exe",
    "-DCMAKE_CXX_COMPILER=$Toolchain/bin/g++.exe",
    "-DQt6_DIR=$QtPrefix/lib/cmake/Qt6",
    "-DCMAKE_PREFIX_PATH=$QtPrefix;$Toolchain",
    "-DSHIREIA_SUYU_SOURCE=$suyuSource", "-DSHIREIA_SUYU_BUILD=$suyuBuild",
    '-DSHIREIA_BUILD_CORE=ON', '-DCMAKE_BUILD_TYPE=Release',
    "-DSHIREIA_LIBARCHIVE_RUNTIME=$Toolchain/bin/libarchive-13.dll"
)
& cmake @shireiaArgs
if ($LASTEXITCODE) { exit $LASTEXITCODE }
cmake --build $mainBuild --parallel
if ($LASTEXITCODE) { exit $LASTEXITCODE }
& "$PSScriptRoot/deploy.ps1" -QtPrefix $QtPrefix -Toolchain $Toolchain -Destination $mainBuild
$env:QT_PLUGIN_PATH = "$QtPrefix/share/qt6/plugins"
$env:QML_IMPORT_PATH = "$QtPrefix/share/qt6/qml"
ctest --test-dir $mainBuild --output-on-failure
if ($LASTEXITCODE) { exit $LASTEXITCODE }
& "$PSScriptRoot/test-runtime.ps1" -BuildRoot $mainBuild
