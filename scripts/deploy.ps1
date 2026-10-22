param(
    [string]$QtPrefix = 'C:/msys64/ucrt64',
    [string]$Toolchain = 'C:/msys64/ucrt64',
    [string]$Destination = "$PSScriptRoot/../build"
)
$ErrorActionPreference = 'Stop'
$project = Split-Path $PSScriptRoot -Parent
if (-not [IO.Path]::IsPathRooted($Destination)) { $Destination = Join-Path $project $Destination }
$destination = [IO.Path]::GetFullPath($Destination)
$null = New-Item -ItemType Directory -Path $destination -Force
$QtPrefix = (Resolve-Path $QtPrefix).Path
$env:PATH = "$QtPrefix/bin;$Toolchain/bin;" + $env:PATH
$binaries = @(Join-Path $destination 'Shireia Next.exe')
if (Test-Path -LiteralPath "$destination/ShireiaCore.exe") { $binaries += "$destination/ShireiaCore.exe" }
$deployTool = Join-Path $QtPrefix 'bin/windeployqt.exe'
$previousPreference = $ErrorActionPreference
try {
    $ErrorActionPreference = 'Continue'
    & $deployTool --release --no-translations --verbose 0 --qmldir "$project/src/frontend" --dir $destination @binaries *> "$destination/deploy-qt.log"
    $deployResult = $LASTEXITCODE
} finally { $ErrorActionPreference = $previousPreference }
if ($deployResult -ne 0) { throw "Qt deployment failed. See $destination/deploy-qt.log" }

# windeployqt deploys Qt/QML but not every dependency of SDL2 or the core.
# Resolve PE imports recursively using only the matching Qt/UCRT64 prefixes.
$pending = [Collections.Generic.Queue[string]]::new()
# Runtime modules only: never traverse private NAND, saves or user test content.
$runtimeFiles = @(Get-ChildItem -LiteralPath $destination -File)
foreach ($moduleDir in @('qml', 'platforms', 'imageformats', 'iconengines', 'networkinformation', 'tls', 'generic', 'styles', 'scenegraph', 'qmltooling', 'platformthemes', 'egldeviceintegrations', 'sqldrivers')) {
    $modulePath = Join-Path $destination $moduleDir
    if (Test-Path -LiteralPath $modulePath) { $runtimeFiles += Get-ChildItem -LiteralPath $modulePath -Recurse -File }
}
$runtimeFiles | Where-Object { $_.Extension -in '.exe', '.dll' } | ForEach-Object { $pending.Enqueue($_.FullName) }
$visited = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
$sources = [Collections.Generic.List[object]]::new()
$dependencies = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
$objdump = Join-Path $Toolchain 'bin/objdump.exe'
while ($pending.Count -gt 0) {
    $binary = $pending.Dequeue()
    if (-not $visited.Add($binary)) { continue }
    $imports = & $objdump -p $binary
    if ($LASTEXITCODE -ne 0) { throw "Cannot inspect PE imports: $binary" }
    foreach ($line in $imports) {
        if ($line -notmatch 'DLL Name:\s*(\S+)') { continue }
        $name = $Matches[1]
        if ($name -match '^(api-ms-|ext-ms-)' -or (Test-Path -LiteralPath "$env:SystemRoot/System32/$name")) { continue }
        $local = Join-Path $destination $name
        if (Test-Path -LiteralPath $local) { $dependencies.Add($name) | Out-Null; $pending.Enqueue($local); continue }
        $source = @("$QtPrefix/bin/$name", "$Toolchain/bin/$name") | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
        if (-not $source) { throw "Missing runtime dependency $name required by $binary" }
        Copy-Item -LiteralPath $source -Destination $local
        $sources.Add([pscustomobject]@{ name = $name; source = $source })
        $dependencies.Add($name) | Out-Null
        $pending.Enqueue($local)
    }
}
[IO.File]::WriteAllText((Join-Path $destination 'qt.conf'), "[Paths]`nPrefix=.`nPlugins=.`nQmlImports=qml`n", [Text.UTF8Encoding]::new($false))
$inventory = @($dependencies | Sort-Object | ForEach-Object { [pscustomobject]@{ name = $_; path = Join-Path $destination $_ } })
ConvertTo-Json -InputObject $inventory | Set-Content -Encoding UTF8 -LiteralPath "$destination/runtime-dependencies.json"
Write-Output "Runtime ready: $($visited.Count) PE files checked; $($sources.Count) additional DLLs copied."
