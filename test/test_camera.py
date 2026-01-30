from typing import Any, Dict, Tuple
import os
import platform
import pytest
import numpy as np
import pyvirtualcam
from pyvirtualcam import PixelFormat

@pytest.mark.parametrize("backend", list(pyvirtualcam.camera.BACKENDS))
def test_consecutive(backend: str):
    with pyvirtualcam.Camera(width=1280, height=720, fps=20, backend=backend) as cam:
        frame = np.zeros((cam.height, cam.width, 3), np.uint8) # RGB
        cam.send(frame)
    
    with pyvirtualcam.Camera(width=1280, height=720, fps=20, backend=backend) as cam:
        frame = np.zeros((cam.height, cam.width, 3), np.uint8) # RGB
        cam.send(frame)

@pytest.mark.skipif(
    platform.system() == 'Linux',
    reason='v4l2loopback (Linux) allows multiple cameras')
@pytest.mark.parametrize("backend", ['obs'])
def test_multiple_cameras_not_supported(backend: str):
    with pyvirtualcam.Camera(width=1280, height=720, fps=20, backend=backend) as cam:
        frame = np.zeros((cam.height, cam.width, 3), np.uint8) # RGB
        cam.send(frame)

        # only a single virtual camera is supported
        with pytest.raises(RuntimeError):
            with pyvirtualcam.Camera(width=1280, height=720, fps=20, backend=backend) as cam:
                frame = np.zeros((cam.height, cam.width, 3), np.uint8) # RGB
                cam.send(frame)

@pytest.mark.skipif(
    platform.system() == 'Darwin',
    reason='multiple cameras not supported on macOS (obs backend)')
@pytest.mark.parametrize("backend", 
    ['unitycapture'] if platform.system() == 'Windows' else ['v4l2loopback'])
def test_multiple_cameras_are_supported(backend: str):
    cam1 = pyvirtualcam.Camera(width=1280, height=720, fps=20, backend=backend)
    frame = np.zeros((cam1.height, cam1.width, 3), np.uint8) # RGB
    cam1.send(frame)

    cam2 = pyvirtualcam.Camera(width=640, height=480, fps=20, backend=backend)
    frame = np.zeros((cam2.height, cam2.width, 3), np.uint8) # RGB
    cam2.send(frame)

@pytest.mark.parametrize("backend", list(pyvirtualcam.camera.BACKENDS))
def test_select_camera_device(backend: str):
    if backend == 'obs':
        device = 'OBS Virtual Camera'
    elif backend == 'unitycapture':
        device = 'Unity Video Capture'
    elif backend == 'v4l2capture':
        device = '/dev/video0'
    else:
        raise NotImplementedError
    with pyvirtualcam.Camera(width=1280, height=720, fps=20, device=device) as cam:
        frame = np.zeros((cam.height, cam.width, 3), np.uint8) # RGB
        cam.send(frame)

@pytest.mark.parametrize("backend", list(pyvirtualcam.camera.BACKENDS))
def test_select_invalid_camera_device(backend: str):
    if backend == 'obs':
        device = 'Foo'
    elif backend == 'unitycapture':
        device = 'Unity Video Capture #20'
    elif backend == 'v4l2capture':
        device = '/dev/video123'
    else:
        raise NotImplementedError
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

EXPECTED_NATIVE_FMTS: Dict[Tuple[str,str],Any] = {
    ('Windows', 'obs'): lambda _: PixelFormat.NV12,
    ('Windows', 'unitycapture'): lambda _: PixelFormat.RGBA,
    ('Darwin', 'obs'): lambda _: PixelFormat.UYVY,
    ('Linux', 'v4l2loopback'): lambda fmt: PixelFormat.I420 
                                 if fmt in [PixelFormat.RGB, PixelFormat.BGR]
                                 else fmt,
}

@pytest.mark.parametrize("backend", list(pyvirtualcam.camera.BACKENDS))
def test_alternative_pixel_formats(backend: str):
    def check_native_fmt(cam):
        assert cam.native_fmt == EXPECTED_NATIVE_FMTS[(platform.system(), backend)](cam.fmt)
    
    if backend == 'unitycapture':
        with pyvirtualcam.Camera(width=1280, height=720, fps=20, fmt=PixelFormat.RGBA, backend=backend) as cam:
            check_native_fmt(cam)
            cam.send(np.zeros((cam.height, cam.width, 4), np.uint8))
    
    with pyvirtualcam.Camera(width=1280, height=720, fps=20, fmt=PixelFormat.BGR, backend=backend) as cam:
        check_native_fmt(cam)
        cam.send(np.zeros((cam.height, cam.width, 3), np.uint8))
    
    with pyvirtualcam.Camera(width=1280, height=720, fps=20, fmt=PixelFormat.GRAY, backend=backend) as cam:
        check_native_fmt(cam)
        cam.send(np.zeros((cam.height, cam.width), np.uint8))

    with pyvirtualcam.Camera(width=1280, height=720, fps=20, fmt=PixelFormat.I420, backend=backend) as cam:
        check_native_fmt(cam)
        cam.send(np.zeros(cam.height * cam.width + cam.height * (cam.width // 2), np.uint8))

    with pyvirtualcam.Camera(width=1280, height=720, fps=20, fmt=PixelFormat.NV12, backend=backend) as cam:
        check_native_fmt(cam)
        cam.send(np.zeros(cam.height * cam.width + cam.height * (cam.width // 2), np.uint8))
    
    with pyvirtualcam.Camera(width=1280, height=720, fps=20, fmt=PixelFormat.YUYV, backend=backend) as cam:
        check_native_fmt(cam)
        cam.send(np.zeros(cam.height * cam.width * 2, np.uint8))
    
    with pyvirtualcam.Camera(width=1280, height=720, fps=20, fmt=PixelFormat.UYVY, backend=backend) as cam:
        check_native_fmt(cam)
        cam.send(np.zeros(cam.height * cam.width * 2, np.uint8))

@pytest.mark.skipif(
    bool(os.environ.get('CI')) and platform.system() == 'Darwin',
    reason='disabled due to high fluctuations in CI, manually verified on MacBook Pro')
def test_sleep_until_next_frame():
    target_fps = 20
    with pyvirtualcam.Camera(width=1280, height=720, fps=target_fps) as cam:
        frame = np.zeros((cam.height, cam.width, 3), np.uint8) # RGB
        for _ in range(100):
            cam.send(frame)
            cam.sleep_until_next_frame()
        
        actual_fps = cam.current_fps
        assert abs(target_fps - actual_fps) < 1.5

@pytest.mark.parametrize("backend", list(pyvirtualcam.camera.BACKENDS))
def test_device_name(backend: str):
    with pyvirtualcam.Camera(width=1280, height=720, fps=20, backend=backend) as cam:
        if backend == 'obs':
            assert cam.device == 'OBS Virtual Camera'
        elif backend == 'unitycapture':
            assert cam.device == 'Unity Video Capture'
        elif backend == 'v4l2loopback':
            assert cam.device.startswith('/dev/video')
        else:
            raise NotImplementedError
