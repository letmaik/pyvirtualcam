import time
import platform
import threading
import pytest
import numpy as np
import cv2
import pyvirtualcam
from pyvirtualcam import PixelFormat

if platform.system() == 'Windows':
    import pyvirtualcam_win_dshow_capture as dshow

    def capture_rgb(device: str, width: int, height: int) -> np.ndarray:
        return dshow.capture(device, width, height)

elif platform.system() == 'Linux':
    def capture_rgb(device: str, width: int, height: int) -> np.ndarray:
        bgr = np.empty((height, width, 3), np.uint8)
        exc = []
        def capture_cv2():
            try:
                print(f'Opening {device} for capture')
                vc = cv2.VideoCapture(device)
                assert vc.isOpened()
                print(f'Configuring resolution of {device}')
                vc.set(cv2.CAP_PROP_FRAME_WIDTH, width)
                vc.set(cv2.CAP_PROP_FRAME_HEIGHT, height)
                print(f'Reading frame from {device}')
                ret, frame = vc.read()
                bgr[:] = frame
                assert ret
            except Exception as e:
                exc.append(e)

        # Sending and capturing frames on the same thread deadlocks.
        thread = threading.Thread(target=capture_cv2)
        thread.start()
        thread.join()
        if exc:
            raise exc[0]

        rgb = cv2.cvtColor(bgr, cv2.COLOR_BGR2RGB)
        return rgb

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
    platform.system() == 'Darwin',
    reason='not implemented on macOS')
@pytest.mark.parametrize("fmt", list(frames.keys()))
def test_capture(fmt):
    with pyvirtualcam.Camera(w, h, fps, fmt=fmt) as cam:
        stop = False
        def send_frames():
            frame = frames[fmt]
            while not stop:
                cam.send(frame)
                cam.sleep_until_next_frame()
    
        thread = threading.Thread(target=send_frames)
        thread.start()
            
        try:
            captured_rgb = capture_rgb(cam.device, cam.width, cam.height)
            d = np.fabs(captured_rgb.astype(np.int16) - rgb).max()
            assert d <= max_deltas[fmt]
        finally:
            stop = True
            thread.join()
    
    # wait until capture has fully let go of camera
    time.sleep(4)
