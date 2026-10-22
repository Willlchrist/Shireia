param([int]$TimeoutSeconds = 45, [string]$BuildRoot = "$PSScriptRoot/../build")
$ErrorActionPreference = 'Stop'
$project = Split-Path $PSScriptRoot -Parent
if (-not [IO.Path]::IsPathRooted($BuildRoot)) { $BuildRoot = Join-Path $project $BuildRoot }
$build = [IO.Path]::GetFullPath($BuildRoot)
New-Item -ItemType Directory -Force -Path (Join-Path $build 'build') | Out-Null
if (-not ('ShireiaRuntimeErrorMode' -as [type])) {
    Add-Type -TypeDefinition 'using System.Runtime.InteropServices; public static class ShireiaRuntimeErrorMode { [DllImport("kernel32.dll")] public static extern uint SetErrorMode(uint mode); }'
}
$priorMode = [ShireiaRuntimeErrorMode]::SetErrorMode(0x8003)
$results = [Collections.Generic.List[object]]::new()
function Invoke-CleanRuntime([string]$Name, [string]$Arguments, [bool]$NormalWindow = $false) {
    $start = [Diagnostics.ProcessStartInfo]::new()
    $start.FileName = Join-Path $build $Name
    $start.Arguments = $Arguments
    $start.WorkingDirectory = $build
    $start.UseShellExecute = $false
    $start.CreateNoWindow = $true
    $start.RedirectStandardOutput = $true
    $start.RedirectStandardError = $true
    $start.EnvironmentVariables['PATH'] = "$env:SystemRoot\System32;$env:SystemRoot"
    foreach ($variable in @('QT_PLUGIN_PATH', 'QT_QPA_PLATFORM_PLUGIN_PATH', 'QML_IMPORT_PATH', 'QML2_IMPORT_PATH', 'QT_QPA_PLATFORM', 'QT_QUICK_BACKEND', 'QSG_RHI_BACKEND')) {
        $start.EnvironmentVariables.Remove($variable)
    }
    $start.EnvironmentVariables['QT_FORCE_STDERR_LOGGING'] = '1'
    $process = [Diagnostics.Process]::Start($start)
    $outputTask = $process.StandardOutput.ReadToEndAsync()
    $errorTask = $process.StandardError.ReadToEndAsync()
    try {
        if ($NormalWindow) {
            $timer = [Diagnostics.Stopwatch]::StartNew()
            do { Start-Sleep -Milliseconds 100; $process.Refresh() } while (-not $process.HasExited -and $process.MainWindowHandle -eq 0 -and $timer.Elapsed.TotalSeconds -lt 10)
            if ($process.HasExited -or $process.MainWindowHandle -eq 0) { throw 'The main window did not open.' }
            Start-Sleep -Milliseconds 2000
            if (-not $process.Responding) { throw 'The application window is not responding.' }
            if (-not $process.CloseMainWindow()) { throw 'Could not request normal window closure.' }
        }
        if (-not $process.WaitForExit($TimeoutSeconds * 1000)) { throw "Executable timed out: $Name" }
        $output = $outputTask.GetAwaiter().GetResult()
        $errors = $errorTask.GetAwaiter().GetResult()
        $logName = if ($NormalWindow) { 'normal-window' } else { [IO.Path]::GetFileNameWithoutExtension($Name) }
        [IO.File]::WriteAllText((Join-Path $build "runtime-$logName.log"), "$output`n$errors", [Text.UTF8Encoding]::new($false))
        if ($process.ExitCode -ne 0) { throw "$Name exited with code $($process.ExitCode). $errors" }
        $results.Add([pscustomobject]@{ executable = $Name; arguments = $Arguments; normalWindow = $NormalWindow; exitCode = $process.ExitCode; cleanPath = $true })
        Write-Output "PASS: $Name $Arguments (without development PATH or Qt import variables)."
    } finally {
        if (-not $process.HasExited) { $process.Kill(); $process.WaitForExit(5000) | Out-Null }
        $process.Dispose()
    }
}
try {
    if (Test-Path -LiteralPath "$build/ShireiaCore.exe") { Invoke-CleanRuntime 'ShireiaCore.exe' '--self-test' }
    Invoke-CleanRuntime 'Shireia Next.exe' '--smoke-test --capture'
    Invoke-CleanRuntime 'Shireia Next.exe' '' $true
    $results | ConvertTo-Json | Set-Content -Encoding UTF8 -LiteralPath "$build/executable-validation.json"
} finally { [ShireiaRuntimeErrorMode]::SetErrorMode($priorMode) | Out-Null }
