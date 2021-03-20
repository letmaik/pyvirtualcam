import time
import platform
import pytest
import numpy as np
import cv2
import pyvirtualcam
from pyvirtualcam import PixelFormat

if platform.system() == 'Windows':
    import pyvirtualcam_win_dshow_capture as dshow

    def capture_rgb(device: str, width: int, height: int) -> np.ndarray:
        return dshow.capture(device, width, height)

w = 1280
h = 720
fps = 20
rgb = np.zeros((h, w, 3), np.uint8)
# TODO test with gradient or similar
rgb[:,:,0] = 5
rgb[:,:,1] = 50
rgb[:,:,2] = 250

frames = {
    PixelFormat.RGB: rgb,
    PixelFormat.BGR: cv2.cvtColor(rgb, cv2.COLOR_RGB2BGR),
    PixelFormat.I420: cv2.cvtColor(rgb, cv2.COLOR_RGB2YUV_I420)
    # TODO test remaining formats (OpenCV is missing converters)
}

max_deltas = {
    PixelFormat.RGB: 1,
    PixelFormat.BGR: 1,
    PixelFormat.I420: 3,
}

@pytest.mark.skipif(
    platform.system() != 'Windows',
    reason='capture test only implemented on Windows so far')
@pytest.mark.parametrize("fmt", list(frames.keys()))
def test_capture(fmt):
    with pyvirtualcam.Camera(w, h, fps, fmt=fmt) as cam:
        frame = frames[fmt]
        for _ in range(10):
            cam.send(frame)
            cam.sleep_until_next_frame()
        
        captured_rgb = capture_rgb(cam.device, cam.width, cam.height)
        d = np.fabs(captured_rgb.astype(np.int16) - rgb).max()
        assert d <= max_deltas[fmt]

    # wait until capture has fully let go of camera
    time.sleep(4)
