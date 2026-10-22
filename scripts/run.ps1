param([string]$QtPrefix = 'C:/msys64/ucrt64', [string]$Toolchain = 'C:/msys64/ucrt64', [string]$BuildRoot = "$PSScriptRoot/../build", [switch]$Smoke, [switch]$Capture, [switch]$CoreSelfTest, [switch]$ExecutionSelfTest, [switch]$Vulkan, [switch]$GpuSmoke, [string]$ProgramSmoke)
$ErrorActionPreference = 'Stop'
$project = Split-Path $PSScriptRoot -Parent
if (-not [IO.Path]::IsPathRooted($BuildRoot)) { $BuildRoot = Join-Path $project $BuildRoot }
$BuildRoot = [IO.Path]::GetFullPath($BuildRoot)
$QtPrefix = (Resolve-Path $QtPrefix).Path
$env:PATH = "$QtPrefix/bin;$Toolchain/bin;" + $env:PATH
$env:QT_PLUGIN_PATH = "$QtPrefix/share/qt6/plugins"
$env:QML_IMPORT_PATH = "$QtPrefix/share/qt6/qml"
$env:QT_FORCE_STDERR_LOGGING = '1'
if ($CoreSelfTest) { & (Join-Path $BuildRoot 'ShireiaCore.exe') --self-test; exit $LASTEXITCODE }
if ($ExecutionSelfTest) {
    $coreArguments = @('--execution-self-test')
    if ($Vulkan) { $coreArguments += '--vulkan-test' }
    & (Join-Path $BuildRoot 'ShireiaCore.exe') @coreArguments
    exit $LASTEXITCODE
}
$arguments = @()
if ($Smoke) { $arguments += '--smoke-test' }
if ($GpuSmoke) { $arguments += '--gpu-smoke-test' }
if ($Capture) { $arguments += '--capture' }
if ($ProgramSmoke) { $arguments += @('--program-smoke-test', (Resolve-Path -LiteralPath $ProgramSmoke).Path) }
Push-Location $project
try { & (Join-Path $BuildRoot 'Shireia Next.exe') @arguments; exit $LASTEXITCODE } finally { Pop-Location }
