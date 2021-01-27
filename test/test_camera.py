import pytest
import numpy as np
import pyvirtualcam

def test_consecutive():
    with pyvirtualcam.Camera(width=1280, height=720, fps=20) as cam:
        frame = np.zeros((cam.height, cam.width, 4), np.uint8) # RGBA
        cam.send(frame)
    
    with pyvirtualcam.Camera(width=1280, height=720, fps=20) as cam:
        frame = np.zeros((cam.height, cam.width, 4), np.uint8) # RGBA
        cam.send(frame)

def test_parallel():
    with pyvirtualcam.Camera(width=1280, height=720, fps=20) as cam:
        frame = np.zeros((cam.height, cam.width, 4), np.uint8) # RGBA
        cam.send(frame)

        # only a single virtual camera is supported
        with pytest.raises(RuntimeError):
            with pyvirtualcam.Camera(width=1280, height=720, fps=20) as cam:
                frame = np.zeros((cam.height, cam.width, 4), np.uint8) # RGBA
                cam.send(frame)

def test_sleep_until_next_frame():
    target_fps = 20
    with pyvirtualcam.Camera(width=1280, height=720, fps=target_fps) as cam:
        frame = np.zeros((cam.height, cam.width, 4), np.uint8) # RGBA
        for _ in range(100):
            cam.send(frame)
            cam.sleep_until_next_frame()
        
        actual_fps = cam.current_fps
        assert abs(target_fps - actual_fps) < 0.1
