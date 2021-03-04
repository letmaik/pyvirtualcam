# pyvirtualcam

pyvirtualcam sends frames to a virtual camera from Python.

## Usage

```py
import pyvirtualcam
import numpy as np

with pyvirtualcam.Camera(width=1280, height=720, fps=20) as cam:
    frame = np.zeros((cam.height, cam.width, 4), np.uint8)  # RGBA
    frame[:, :, 3] = 255
    while True:
        frame[:, :, :3] = cam.frames_sent % 255  # grayscale animation
        cam.send(frame)
        cam.sleep_until_next_frame()
```

For more examples, check out the [`samples/`](samples) folder.

## Installation

This package works on Windows, macOS, and Linux. Install it from PyPI with:

```sh
pip install pyvirtualcam
```

pyvirtualcam relies on existing virtual cameras which have to be installed first. See the next section for details.

## Supported virtual cameras

### Windows & macOS: OBS

[OBS](https://obsproject.com/) includes a built-in virtual camera for Windows (since 26.0) and macOS (since 26.1).

To use the OBS virtual camera, simply install OBS.

Note that OBS provides a single camera instance only, so it is *not* possible to send frames from Python, capture the camera in OBS, mix it with other content, and output it again as virtual camera.

### Linux: v4l2loopback via pyfakewebcam

pyvirtualcam uses [pyfakewebcam](https://github.com/jremmons/pyfakewebcam) on Linux to send frames to [v4l2loopback](https://github.com/umlaeute/v4l2loopback) virtual cameras.

For optimal performance, install the optional `opencv-python` dependency (see also pyfakewebcam's [README](https://github.com/jremmons/pyfakewebcam)):

```sh
# via PyPI
pip install opencv-python
# via Linux package manager
# apt install python-opencv
```

To create a v4l2loopback virtual camera, run the following:

```sh
sudo apt install v4l2loopback-dkms
sudo modprobe v4l2loopback devices=1
```

For further information, see the [v4l2loopback documentation](https://github.com/umlaeute/v4l2loopback).

pyvirtualcam uses the first available virtual camera it finds.
If you have more than one you can manually select a virtual camera by setting the `PYVIRTUALCAM_DEVICE=/dev/video1` environment variable.
