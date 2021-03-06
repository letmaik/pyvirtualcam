import os
import sys
import pytest
import numpy as np
import pyvirtualcam

def test_consecutive():
    with pyvirtualcam.Camera(width=1280, height=720, fps=20) as cam:
        frame = np.zeros((cam.height, cam.width, 3), np.uint8) # RGB
        cam.send(frame)
    
    with pyvirtualcam.Camera(width=1280, height=720, fps=20) as cam:
        frame = np.zeros((cam.height, cam.width, 3), np.uint8) # RGB
        cam.send(frame)

def test_parallel():
    with pyvirtualcam.Camera(width=1280, height=720, fps=20) as cam:
        frame = np.zeros((cam.height, cam.width, 3), np.uint8) # RGB
        cam.send(frame)

        # only a single virtual camera is supported
        with pytest.raises(RuntimeError):
            with pyvirtualcam.Camera(width=1280, height=720, fps=20) as cam:
                frame = np.zeros((cam.height, cam.width, 3), np.uint8) # RGB
                cam.send(frame)

def test_invalid_frame_shape():
    with pyvirtualcam.Camera(width=1280, height=720, fps=20) as cam:
        with pytest.raises(ValueError):
            cam.send(np.zeros((640, 480, 3), np.uint8))
        with pytest.raises(ValueError):
            cam.send(np.zeros((1280, 720, 1), np.uint8))
        with pytest.raises(ValueError):
            cam.send(np.zeros((1280, 720), np.uint8))

def test_deprecated_rgba_frame_format():
    with pyvirtualcam.Camera(width=1280, height=720, fps=20) as cam:
        cam.send(np.zeros((1280, 720, 4), np.uint8))

@pytest.mark.skipif(
    os.environ.get('CI') and sys.platform == 'darwin',
    reason='disabled due to high fluctuations in CI, manually verified on MacBook Pro')
def test_sleep_until_next_frame():
    target_fps = 20
    with pyvirtualcam.Camera(width=1280, height=720, fps=target_fps) as cam:
        frame = np.zeros((cam.height, cam.width, 3), np.uint8) # RGBA
        for _ in range(100):
            cam.send(frame)
            cam.sleep_until_next_frame()
        
        actual_fps = cam.current_fps
        assert abs(target_fps - actual_fps) < 0.5
