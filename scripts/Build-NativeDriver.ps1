param(
    [string]$ToolsRoot=$env:MACHINEGUARD_EMBEDDED_TOOLS,
    [string]$VcVarsPath='C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Auxiliary/Build/vcvars64.bat',
    [string]$OutputDirectory=(Join-Path $PSScriptRoot '../build/driver')
)
$ErrorActionPreference='Stop'
if(-not $ToolsRoot) { throw 'Provide -ToolsRoot for the pinned ArduinoJson headers.' }
$root=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$source=Join-Path $root 'firmware/MachineGuardNode/src'
$json=Join-Path $ToolsRoot 'user/libraries/ArduinoJson/src'
New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
$output=(Resolve-Path -LiteralPath $OutputDirectory).Path
$argsList=@('/nologo','/std:c++17','/EHsc','/W4','/WX','/permissive-','/utf-8','/D_CRT_SECURE_NO_WARNINGS',
    "/I`"$source`"","/external:I`"$json`"",'/external:W0','/Fe:native-edge-driver.exe','/Fo:./')
$argsList += "`"$(Join-Path $root 'tools/native_edge_driver.cpp')`""
foreach($file in Get-ChildItem -LiteralPath $source -Filter '*.cpp' -File) { $argsList += "`"$($file.FullName)`"" }
$command='call "'+$VcVarsPath+'" >nul && cl '+($argsList -join ' ')+' /link winhttp.lib'
Push-Location -LiteralPath $output
try { & $env:ComSpec /d /s /c $command; $result=$LASTEXITCODE } finally { Pop-Location }
exit $result
