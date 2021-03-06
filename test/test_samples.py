import numpy as np
import pyvirtualcam

def test_sample_simple():
    with pyvirtualcam.Camera(width=1280, height=720, fps=20, print_fps=True) as cam:
        print(f'Using virtual camera: {cam.device}')
        frame = np.zeros((cam.height, cam.width, 3), np.uint8) # RGB
        for _ in range(100):
            frame[:] = cam.frames_sent % 255 # grayscale animation
            cam.send(frame)
            cam.sleep_until_next_frame()
