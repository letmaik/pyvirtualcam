import os
import platform
import threading
import time
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

def get_test_frame(w, h, fmt: PixelFormat):
    def rgb_color(r, g, b):
        if fmt == PixelFormat.RGB:
            return np.array([r, g, b], np.uint8).reshape(1,1,3)
        elif fmt == PixelFormat.BGR:
            return np.array([b, g, r], np.uint8).reshape(1,1,3)
    
    if fmt in [PixelFormat.BGR, PixelFormat.RGB]:
        frame = np.empty((h, w, 3), np.uint8)
        frame[:h//2,:w//2] = rgb_color(220, 20, 60)
        frame[:h//2,w//2:] = rgb_color(240, 230, 140)
        frame[h//2:,:w//2] = rgb_color(50, 205, 50)
        frame[h//2:,w//2:] = rgb_color(238, 130, 238)
    elif fmt == PixelFormat.I420:
        frame = np.empty((h + h // 2, w), np.uint8)
        # Y plane
        frame[:h//2,:w//2] = 30
        frame[:h//2,w//2:] = 60
        frame[h//2:h,:w//2] = 100
        frame[h//2:h,w//2:] = 200
        # UV planes
        s = h // 4
        u = h
        v = h + s
        frame[u:u+s//2] = 100
        frame[u+s//2:v] = 10
        frame[v:v+s//2] = 30
        frame[v+s//2:] = 200
    elif fmt == PixelFormat.NV12:
        frame = get_test_frame(w, h, PixelFormat.I420)
        # UUVV -> UVUV
        u = frame[h:h + h // 4].copy()
        v = frame[h + h // 4:].copy()
        frame[h::2] = u
        frame[h+1::2] = v
    elif fmt in [PixelFormat.YUYV, PixelFormat.UYVY]:
        frame = np.empty((h, w, 2), np.uint8)
        if fmt == PixelFormat.YUYV:
            y = frame.reshape(-1)[::2].reshape(h, w)
            u = frame.reshape(-1)[1::4].reshape(h, w // 2)
            v = frame.reshape(-1)[3::4].reshape(h, w // 2)
        elif fmt == PixelFormat.UYVY:
            y = frame.reshape(-1)[1::2].reshape(h, w)
            u = frame.reshape(-1)[::4].reshape(h, w // 2)
            v = frame.reshape(-1)[2::4].reshape(h, w // 2)
        else:
            assert False
        y[:h//2,:w//2] = 30
        y[:h//2,w//2:] = 60
        y[h//2:h,:w//2] = 100
        y[h//2:h,w//2:] = 200
        u[:h//2] = 100
        u[h//2:] = 10
        v[:h//2] = 30
        v[h//2:] = 200
    else:
        assert False
    return frame

formats = [
    PixelFormat.RGB,
    PixelFormat.BGR,
    PixelFormat.I420,
    PixelFormat.NV12,
    PixelFormat.YUYV,
    PixelFormat.UYVY,
]

frames = { fmt: get_test_frame(w, h, fmt) for fmt in formats }

frames_rgb = {
    PixelFormat.RGB: frames[PixelFormat.RGB],
    PixelFormat.BGR: cv2.cvtColor(frames[PixelFormat.BGR], cv2.COLOR_BGR2RGB),
    PixelFormat.I420: cv2.cvtColor(frames[PixelFormat.I420], cv2.COLOR_YUV2RGB_I420),
    PixelFormat.NV12: cv2.cvtColor(frames[PixelFormat.NV12], cv2.COLOR_YUV2RGB_NV12),
    PixelFormat.YUYV: cv2.cvtColor(frames[PixelFormat.YUYV], cv2.COLOR_YUV2RGB_YUYV),
    PixelFormat.UYVY: cv2.cvtColor(frames[PixelFormat.UYVY], cv2.COLOR_YUV2RGB_UYVY),
}

@pytest.mark.skipif(
    platform.system() == 'Darwin',
    reason='not implemented on macOS')
@pytest.mark.parametrize("fmt", formats)
def test_capture(fmt):
    if fmt == PixelFormat.NV12 and platform.system() == 'Linux':
        pytest.skip('OpenCV VideoCapture does not support NV12')

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

            if os.environ.get('PYVIRTUALCAM_DUMP_FRAMES'):
                import imageio
                imageio.imwrite(f'test_{fmt}_in.png', frames_rgb[fmt])
                imageio.imwrite(f'test_{fmt}_out.png', captured_rgb)

            d = np.fabs(captured_rgb.astype(np.int16) - frames_rgb[fmt]).max()
            assert d <= 2
        finally:
            stop = True
            thread.join()
    
    # wait until capture has fully let go of camera
    time.sleep(4)
