$ErrorActionPreference = 'Stop'

. $PSScriptRoot\download-file.ps1

$VERSION = "26.1.1"

$ZIP_FILENAME = "OBS-Studio-$VERSION-Full-x64.zip"
$ZIP_URL = "https://cdn-fastly.obsproject.com/downloads/$ZIP_FILENAME"

$path = DownloadFile $ZIP_URL $ZIP_FILENAME

$install_dir = "obs-studio"
Write-Host "Extracting..."
Expand-Archive -LiteralPath $path -DestinationPath $install_dir -Force

$args = "/n /i:1 /s $pwd\$install_dir\data\obs-plugins\win-dshow\obs-virtualcam-module32.dll"
Write-Host "regsvr32" $args
Start-Process -FilePath "regsvr32" -ArgumentList $args -Wait -Passthru -Verb "runAs"

$args = "/n /i:1 /s $pwd\$install_dir\data\obs-plugins\win-dshow\obs-virtualcam-module64.dll"
Write-Host "regsvr32" $args
Start-Process -FilePath "regsvr32" -ArgumentList $args -Wait -Passthru -Verb "runAs"

