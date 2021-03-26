# pyvirtualcam

pyvirtualcam sends frames to a virtual camera from Python.

## Usage

```py
import pyvirtualcam
import numpy as np

with pyvirtualcam.Camera(width=1280, height=720, fps=20) as cam:
    print(f'Using virtual camera: {cam.device}')
    frame = np.zeros((cam.height, cam.width, 3), np.uint8)  # RGB
    while True:
        frame[:] = cam.frames_sent % 255  # grayscale animation
        cam.send(frame)
        cam.sleep_until_next_frame()
```

For more examples, including using different pixel formats like BGR, or selecting a specific camera device on Linux, check out the [`samples/`](samples) folder.

See also the [API Documentation](https://letmaik.github.io/pyvirtualcam).

## Installation

This package works on Windows, macOS, and Linux. Install it from PyPI with:

```sh
pip install pyvirtualcam
```

pyvirtualcam relies on existing virtual cameras which have to be installed first. See the next section for details.

## Supported virtual cameras

### Windows: OBS

[OBS](https://obsproject.com/) includes a built-in virtual camera for Windows (since 26.0).

To use the OBS virtual camera, simply [install OBS](https://obsproject.com/).

Note that OBS provides a single camera instance only, so it is *not* possible to send frames from Python, capture the camera in OBS, mix it with other content, and output it again as virtual camera.

### macOS: OBS

[OBS](https://obsproject.com/) includes a built-in virtual camera for macOS (since 26.1).

To use the OBS virtual camera, follow these one-time setup steps:
- [Install OBS](https://obsproject.com/).
- Start OBS.
- Click "Start Virtual Camera" (bottom right), then "Stop Virtual Camera".
- Close OBS.

Note that OBS provides a single camera instance only, so it is *not* possible to send frames from Python, capture the camera in OBS, mix it with other content, and output it again as virtual camera.

### Linux: v4l2loopback

pyvirtualcam uses [v4l2loopback](https://github.com/umlaeute/v4l2loopback) virtual cameras on Linux.

To create a v4l2loopback virtual camera on Ubuntu, run the following:

```sh
sudo apt install v4l2loopback-dkms
sudo modprobe v4l2loopback devices=1
```

For further information, see the [v4l2loopback documentation](https://github.com/umlaeute/v4l2loopback).

If the `device` keyword argument is not given, then pyvirtualcam uses the first available v4l2loopback virtual camera it finds.
The camera device name can be accessed with `cam.device`.

## Build from source

### Linux/macOS

```sh
git clone https://github.com/letmaik/pyvirtualcam --recursive
cd pyvirtualcam
pip install .
```

### Windows

These instructions are experimental and support is not provided for them.
Typically, there should be no need to build manually since wheels are hosted on PyPI.

You need to have Visual Studio installed to build pyvirtualcam.

In a PowerShell window:
```sh
$env:PYTHON_VERSION = '3.7'
$env:PYTHON_ARCH = '64'
$env:NUMPY_VERSION = '1.14'
git clone https://github.com/letmaik/pyvirtualcam --recursive
cd pyvirtualcam
powershell .github/scripts/build-windows.ps1
```
The above will download all build dependencies (including a Python installation)
and is fully configured through the three environment variables.
