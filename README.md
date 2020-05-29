# pyvirtualcam

NOTE: This package is a work-in-progress. No support is provided, use at own risk.

## Installation

This package is Windows-only for now and not yet published on PyPI.

Make sure you have Visual Studio installed, then run:
```sh
pip install .
```

The package uses [obs-virtual-cam](https://github.com/Fenrirthviti/obs-virtual-cam/releases) which has to be installed separately. Note that the obs-virtual-cam installer assumes an OBS Studio installation and will fail otherwise. You can also download and extract the obs-virtual-cam zip package directly without installing OBS Studio. After unzipping, simply run `regsvr32 /n /i:1 "obs-virtualcam\bin\32bit\obs-virtualsource.dll"` from an elevated command prompt to install the virtual camera device. Use `regsvr32 /u "obs-virtualcam\bin\32bit\obs-virtualsource.dll"` to uninstall it again.
