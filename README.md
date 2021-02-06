# pyvirtualcam

NOTE: This package is a work-in-progress. No support is provided, use at own risk.

## Usage

```py
import pyvirtualcam
import numpy as np

with pyvirtualcam.Camera(width=1280, height=720, fps=30) as cam:
    while True:
        frame = np.zeros((cam.height, cam.width, 4), np.uint8) # RGBA
        frame[:,:,:3] = cam.frames_sent % 255 # grayscale animation
        frame[:,:,3] = 255
        cam.send(frame)
        cam.sleep_until_next_frame()
```

## Installation

This package is Windows-only for now. Binary wheels are provided on PyPI.
Check out [pyfakewebcam](https://github.com/jremmons/pyfakewebcam) for something that works on Linux.

```sh
pip install pyvirtualcam
```

The package uses [obs-virtual-cam](https://github.com/Fenrirthviti/obs-virtual-cam/releases) which has to be installed separately. Note that the obs-virtual-cam installer assumes an OBS Studio installation and will fail otherwise. You can also download the obs-virtual-cam zip package from https://github.com/CatxFish/obs-virtual-cam/releases without installing OBS Studio. After unzipping, from an admin command prompt, run:
```
regsvr32 /n /i:1 "obs-virtualcam\bin\32bit\obs-virtualsource.dll"
regsvr32 /n /i:1 "obs-virtualcam\bin\64bit\obs-virtualsource.dll"
```
To uninstall, run:
```
regsvr32 /u "obs-virtualcam\bin\32bit\obs-virtualsource.dll"
regsvr32 /u "obs-virtualcam\bin\64bit\obs-virtualsource.dll"
```

## Contributions

The most useful contributions would be to add support for macOS or Linux.

Similar to Windows, it may be possible in macOS to piggyback on https://github.com/johnboiles/obs-mac-virtualcam.

For Linux, it seems like https://github.com/umlaeute/v4l2loopback is the right dependency. Code from https://github.com/CatxFish/obs-v4l2sink may be useful as inspiration on how to send frames to the loopback device. Also, there is https://github.com/jremmons/pyfakewebcam which may be a good candidate.
