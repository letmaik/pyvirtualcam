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
    
        with pytest.raises(RuntimeError):
            with pyvirtualcam.Camera(width=1280, height=720, fps=20) as cam:
                frame = np.zeros((cam.height, cam.width, 4), np.uint8) # RGBA
                cam.send(frame)
