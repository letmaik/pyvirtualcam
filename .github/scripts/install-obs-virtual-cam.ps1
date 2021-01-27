$ErrorActionPreference = 'Stop'

$OBS_VIRTUAL_CAM_URL = 'https://github.com/Fenrirthviti/obs-virtual-cam/releases/download/2.0.5/OBS-Virtualcam-2.0.5-Windows.zip'

. $PSScriptRoot\download-file.ps1

function DownloadObsVirtualCam () {
    $filename = "OBS-Virtualcam-Windows.zip"
    $url = $OBS_VIRTUAL_CAM_URL

    return DownloadFile $url $filename
}


function InstallObsVirtualCam () {
    Write-Host "Installing obs-virtual-cam"
    $install_dir = "obs-virtual-cam"
    $filepath = DownloadObsVirtualCam
    Write-Host "Installing" $filepath "to" $install_dir
    Expand-Archive -LiteralPath $filepath -DestinationPath $install_dir -Force
    
    $args = "/n /i:1 /s $pwd\$install_dir\bin\32bit\obs-virtualsource.dll"
    Write-Host "regsvr32" $args
    Start-Process -FilePath "regsvr32" -ArgumentList $args -Wait -Passthru -Verb "runAs"
}

InstallObsVirtualCam

