$ErrorActionPreference = 'Stop'

. $PSScriptRoot\download-file.ps1

$VERSION = "fe461e8f6e1cd1e6a0dfa9891147c8e393a20a2c"

$FILENAME = "UnityCaptureFilter64bit.dll"
$URL = "https://github.com/schellingb/UnityCapture/raw/$VERSION/Install/$FILENAME"

$path = DownloadFile $URL $FILENAME

$args = "/i:UnityCaptureDevices=2 /s $path"
Write-Host "regsvr32" $args
Start-Process -FilePath "regsvr32" -ArgumentList $args -Wait -Passthru -Verb "runAs"
