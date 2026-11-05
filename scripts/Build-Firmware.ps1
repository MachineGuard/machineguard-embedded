param(
    [string]$ToolsRoot = $env:MACHINEGUARD_EMBEDDED_TOOLS,
    [string]$OutputDirectory = (Join-Path $PSScriptRoot '../build/esp32'),
    [switch]$OfflineSimulation
)
$ErrorActionPreference='Stop'
if (-not $ToolsRoot) { throw 'Provide -ToolsRoot, or set MACHINEGUARD_EMBEDDED_TOOLS to the isolated tool directory.' }
$cli=Join-Path $ToolsRoot 'cli/arduino-cli.exe'
$configuration=Join-Path $ToolsRoot 'arduino-cli.yaml'
if (-not (Test-Path -LiteralPath $cli) -or -not (Test-Path -LiteralPath $configuration)) { throw 'Run Setup-Tools.ps1 first.' }
$project=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
if($OfflineSimulation -and (Test-Path -LiteralPath (Join-Path $project 'firmware/MachineGuardNode/secrets.h'))) { throw 'Offline simulation must not include secrets.h.' }
if($OfflineSimulation -and -not $PSBoundParameters.ContainsKey('OutputDirectory')) { $OutputDirectory=Join-Path $project 'build/simulation-offline' }
New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
$output=(Resolve-Path -LiteralPath $OutputDirectory).Path
$profileArgs=@()
if($OfflineSimulation) { $profileArgs=@('--build-property','compiler.cpp.extra_flags=-DMG_SIMULATION_PROFILE=1') }
& $cli --config-file $configuration compile --fqbn 'esp32:esp32:esp32doit-devkit-v1' --warnings all --build-path $output @profileArgs (Join-Path $project 'firmware/MachineGuardNode')
exit $LASTEXITCODE
