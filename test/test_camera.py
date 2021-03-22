import os
import platform
import pytest
import numpy as np
import pyvirtualcam
from pyvirtualcam import PixelFormat

def test_consecutive():
    with pyvirtualcam.Camera(width=1280, height=720, fps=20) as cam:
        frame = np.zeros((cam.height, cam.width, 3), np.uint8) # RGB
        cam.send(frame)
    
    with pyvirtualcam.Camera(width=1280, height=720, fps=20) as cam:
        frame = np.zeros((cam.height, cam.width, 3), np.uint8) # RGB
        cam.send(frame)

@pytest.mark.skipif(
    platform.system() == 'Linux',
    reason='v4l2loopback (Linux) allows multiple cameras')
def test_multiple_cameras_not_supported():
    with pyvirtualcam.Camera(width=1280, height=720, fps=20) as cam:
        frame = np.zeros((cam.height, cam.width, 3), np.uint8) # RGB
        cam.send(frame)

        # only a single virtual camera is supported
        with pytest.raises(RuntimeError):
            with pyvirtualcam.Camera(width=1280, height=720, fps=20) as cam:
                frame = np.zeros((cam.height, cam.width, 3), np.uint8) # RGB
                cam.send(frame)

@pytest.mark.skipif(
    platform.system() != 'Linux',
    reason='multiple cameras only supported with v4l2loopback (Linux)')
def test_multiple_cameras_are_supported():
    cam1 = pyvirtualcam.Camera(width=1280, height=720, fps=20)
    frame = np.zeros((cam1.height, cam1.width, 3), np.uint8) # RGB
    cam1.send(frame)

    cam2 = pyvirtualcam.Camera(width=640, height=480, fps=20)
    frame = np.zeros((cam2.height, cam2.width, 3), np.uint8) # RGB
    cam2.send(frame)

def test_select_camera_device():
    if platform.system() in ['Windows', 'Darwin']:
        device = 'OBS Virtual Camera'
    else:
        device = '/dev/video0'
    with pyvirtualcam.Camera(width=1280, height=720, fps=20, device=device) as cam:
        frame = np.zeros((cam.height, cam.width, 3), np.uint8) # RGB
        cam.send(frame)

def test_select_invalid_camera_device():
    if platform.system() in ['Windows', 'Darwin']:
        device = 'Foo'
    else:
        device = '/dev/video123'
    with pytest.raises(RuntimeError):
        with pyvirtualcam.Camera(width=1280, height=720, fps=20, device=device) as cam:
            frame = np.zeros((cam.height, cam.width, 3), np.uint8) # RGB
            cam.send(frame)

def test_invalid_frame_shape():
    with pyvirtualcam.Camera(width=1280, height=720, fps=20) as cam:
        with pytest.raises(ValueError):
            cam.send(np.zeros((cam.width, cam.height, 3), np.uint8))
        with pytest.raises(ValueError):
            cam.send(np.zeros((480, 640, 3), np.uint8))
        with pytest.raises(ValueError):
            cam.send(np.zeros((cam.height, cam.width, 1), np.uint8))
        with pytest.raises(ValueError):
            cam.send(np.zeros((cam.height, cam.width), np.uint8))

def test_invalid_frame_dtype():
    with pyvirtualcam.Camera(width=1280, height=720, fps=20) as cam:
        with pytest.raises(TypeError):
            cam.send(np.zeros((cam.height, cam.width, 3), np.uint16))

EXPECTED_NATIVE_FMTS = {
    'Windows': lambda _: PixelFormat.NV12,
    'Darwin': lambda _: PixelFormat.UYVY,
    'Linux': lambda fmt: PixelFormat.I420 if fmt in [PixelFormat.RGB, PixelFormat.BGR]
                         else fmt,
}

def test_alternative_pixel_formats():
    def check_native_fmt(cam):
        assert cam.native_fmt == EXPECTED_NATIVE_FMTS[platform.system()](cam.fmt)
    
    with pyvirtualcam.Camera(width=1280, height=720, fps=20, fmt=PixelFormat.BGR) as cam:
        check_native_fmt(cam)
        cam.send(np.zeros((cam.height, cam.width, 3), np.uint8))
    
    with pyvirtualcam.Camera(width=1280, height=720, fps=20, fmt=PixelFormat.GRAY) as cam:
        check_native_fmt(cam)
        cam.send(np.zeros((cam.height, cam.width), np.uint8))

    with pyvirtualcam.Camera(width=1280, height=720, fps=20, fmt=PixelFormat.I420) as cam:
        check_native_fmt(cam)
        cam.send(np.zeros(cam.height * cam.width + cam.height * (cam.width // 2), np.uint8))

    with pyvirtualcam.Camera(width=1280, height=720, fps=20, fmt=PixelFormat.NV12) as cam:
        check_native_fmt(cam)
        cam.send(np.zeros(cam.height * cam.width + cam.height * (cam.width // 2), np.uint8))
    
    with pyvirtualcam.Camera(width=1280, height=720, fps=20, fmt=PixelFormat.YUYV) as cam:
        check_native_fmt(cam)
        cam.send(np.zeros(cam.height * cam.width * 2, np.uint8))
    
    with pyvirtualcam.Camera(width=1280, height=720, fps=20, fmt=PixelFormat.UYVY) as cam:
        check_native_fmt(cam)
        cam.send(np.zeros(cam.height * cam.width * 2, np.uint8))

@pytest.mark.skipif(
    os.environ.get('CI') and platform.system() == 'Darwin',
    reason='disabled due to high fluctuations in CI, manually verified on MacBook Pro')
def test_sleep_until_next_frame():
    target_fps = 20
    with pyvirtualcam.Camera(width=1280, height=720, fps=target_fps) as cam:
        frame = np.zeros((cam.height, cam.width, 3), np.uint8) # RGB
        for _ in range(100):
            cam.send(frame)
            cam.sleep_until_next_frame()
        
        actual_fps = cam.current_fps
        assert abs(target_fps - actual_fps) < 0.5

def test_device_name():
    with pyvirtualcam.Camera(width=1280, height=720, fps=20) as cam:
        if platform.system() in ['Darwin', 'Windows']:
            assert cam.device == 'OBS Virtual Camera'
        elif platform.system() == 'Linux':
            assert cam.device.startswith('/dev/video')
