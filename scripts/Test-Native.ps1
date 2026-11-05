param(
    [string]$VcVarsPath = 'C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Auxiliary/Build/vcvars64.bat',
    [string]$BuildDirectory = (Join-Path $PSScriptRoot '../build/native'),
    [string]$ArduinoJsonInclude = '',
    [string]$ToolsRoot = $env:MACHINEGUARD_EMBEDDED_TOOLS
)
$ErrorActionPreference = 'Stop'
$project = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
if (-not (Test-Path -LiteralPath $VcVarsPath)) { throw 'Provide -VcVarsPath for a Visual Studio C++ x64 tools installation.' }
New-Item -ItemType Directory -Path $BuildDirectory -Force | Out-Null
$build = (Resolve-Path -LiteralPath $BuildDirectory).Path
$source = Join-Path $project 'firmware/MachineGuardNode/src'
if (-not $ArduinoJsonInclude -and $ToolsRoot) { $ArduinoJsonInclude=Join-Path $ToolsRoot 'user/libraries/ArduinoJson/src' }
$argsList = @('/nologo','/std:c++17','/EHsc','/W4','/WX','/permissive-','/utf-8','/D_CRT_SECURE_NO_WARNINGS',
    "/I`"$source`"",
    '/Fe:native-tests.exe','/Fo:./')
$sourceFiles=@("`"$(Join-Path $project 'tests/native_tests.cpp')`"")
foreach ($file in Get-ChildItem -LiteralPath $source -File -Filter '*.cpp') { $sourceFiles += "`"$($file.FullName)`"" }
if ($ArduinoJsonInclude) { $argsList = @("/external:I`"$ArduinoJsonInclude`"",'/external:W0') + $argsList }
$argsList += $sourceFiles
$compile = 'call "' + $VcVarsPath + '" >nul && cl ' + ($argsList -join ' ')
Push-Location -LiteralPath $build
try { & $env:ComSpec /d /s /c $compile; $compileExit=$LASTEXITCODE }
finally { Pop-Location }
if ($compileExit -ne 0) { exit $compileExit }
& (Join-Path $build 'native-tests.exe')
exit $LASTEXITCODE
