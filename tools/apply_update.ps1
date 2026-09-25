$ErrorActionPreference = 'Stop'
$roomProjectRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$roomSourceDll = Join-Path $roomProjectRoot 'build\RoomCatList.dll'
$roomTargetDll = Join-Path $roomProjectRoot 'RoomCatList.dll'
$roomSourceFont = Join-Path $roomProjectRoot 'build\assets\native-ui.ttf'
$roomFontFolder = Join-Path $roomProjectRoot 'assets'
$roomTargetFont = Join-Path $roomFontFolder 'native-ui.ttf'
$roomSourceHud = Join-Path $roomProjectRoot 'build\swfs\house.swf'
$roomHudFolder = Join-Path $roomProjectRoot 'swfs'
$roomTargetHud = Join-Path $roomHudFolder 'house.swf'
if (@(Get-Process -Name Mewgenics -ErrorAction SilentlyContinue).Count -gt 0) {
    throw 'Please exit Mewgenics normally before applying this update. No game process was stopped.'
}
if (-not (Test-Path -LiteralPath $roomSourceDll)) {
    throw 'The compiled DLL is missing. Run build.cmd first.'
}
if (-not (Test-Path -LiteralPath $roomSourceHud)) {
    throw 'The staged native HUD resource is missing. Run tools/build_sidebar_swf.py first.'
}
$roomExpectedHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $roomSourceDll).Hash
if (-not (Test-Path -LiteralPath $roomSourceFont)) {
    throw 'The cached native UI font is missing. Run tools/build_ui_theme.py first.'
}
$roomExpectedFontHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $roomSourceFont).Hash
$roomExpectedHudHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $roomSourceHud).Hash
$roomHudBytes = [System.IO.File]::ReadAllBytes($roomSourceHud)
if ($roomHudBytes.Length -lt 8 -or [System.Text.Encoding]::ASCII.GetString($roomHudBytes,0,3) -ne 'FWS' -or [System.BitConverter]::ToUInt32($roomHudBytes,4) -ne $roomHudBytes.Length) {
    throw 'The staged native HUD must be an uncompressed FWS with an exact length.'
}
New-Item -ItemType Directory -Path $roomFontFolder -Force | Out-Null
Copy-Item -LiteralPath $roomSourceFont -Destination $roomTargetFont -ErrorAction Stop
if ((Get-FileHash -Algorithm SHA256 -LiteralPath $roomTargetFont).Hash -ne $roomExpectedFontHash) {
    throw 'Native font verification failed. The DLL was not replaced.'
}
New-Item -ItemType Directory -Path $roomHudFolder -Force | Out-Null
Copy-Item -LiteralPath $roomSourceHud -Destination $roomTargetHud -ErrorAction Stop
if ((Get-FileHash -Algorithm SHA256 -LiteralPath $roomTargetHud).Hash -ne $roomExpectedHudHash) {
    throw 'Native HUD verification failed. The DLL was not replaced.'
}
Copy-Item -LiteralPath $roomSourceDll -Destination $roomTargetDll -ErrorAction Stop
if ((Get-FileHash -Algorithm SHA256 -LiteralPath $roomTargetDll).Hash -ne $roomExpectedHash) {
    throw 'DLL verification failed. Please apply the update again before starting Mewgenics.'
}
Write-Output 'RoomCatList update VERIFIED: the installed DLL, native font and HUD match the compiled assets.'
Write-Output 'Start Mewgenics when you are ready.'
