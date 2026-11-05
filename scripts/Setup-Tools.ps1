param([Parameter(Mandatory=$true)][string]$ToolsRoot)
$ErrorActionPreference='Stop'
$cliVersion='1.5.1'
$archiveHash='fabe42e0eb04d00e776a66178299ff95a46c623dbc260f997e58fd514853dd40'
New-Item -ItemType Directory -Path $ToolsRoot -Force | Out-Null
$tools=(Resolve-Path -LiteralPath $ToolsRoot).Path
$archive=Join-Path $tools "arduino-cli_$($cliVersion)_Windows_64bit.zip"
if (-not (Test-Path -LiteralPath $archive)) {
    Invoke-WebRequest -Uri "https://github.com/arduino/arduino-cli/releases/download/v$cliVersion/arduino-cli_$($cliVersion)_Windows_64bit.zip" -OutFile $archive
}
if ((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash.ToLowerInvariant() -ne $archiveHash) { throw 'Arduino CLI archive checksum mismatch.' }
$cli=Join-Path $tools 'cli/arduino-cli.exe'
if (-not (Test-Path -LiteralPath $cli)) { Expand-Archive -LiteralPath $archive -DestinationPath (Join-Path $tools 'cli') }
$configuration=Join-Path $tools 'arduino-cli.yaml'
if (-not (Test-Path -LiteralPath $configuration)) {
    $config=@{board_manager=@{additional_urls=@('https://espressif.github.io/arduino-esp32/package_esp32_index.json')};directories=@{
        data=(Join-Path $tools 'data');downloads=(Join-Path $tools 'downloads');user=(Join-Path $tools 'user')}}
    [System.IO.File]::WriteAllText($configuration,($config | ConvertTo-Json -Depth 5),[System.Text.UTF8Encoding]::new($false))
}
& $cli --config-file $configuration version
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& $cli --config-file $configuration core update-index
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& $cli --config-file $configuration core install 'esp32:esp32@3.3.12' --skip-post-install
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
foreach ($library in @('ArduinoJson@6.21.5','Adafruit Unified Sensor@1.1.15','DHT sensor library@1.4.6')) {
    & $cli --config-file $configuration lib install $library
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}
Write-Output 'Isolated ESP32 tools ready. No global installation or hardware upload was performed.'
