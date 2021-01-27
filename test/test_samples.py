import numpy as np
import pyvirtualcam

def test_sample_simple():
    with pyvirtualcam.Camera(width=1280, height=720, fps=20, print_fps=True) as cam:
        frame = np.zeros((cam.height, cam.width, 4), np.uint8) # RGBA
        frame[:,:,:3] = cam.frames_sent % 255 # grayscale animation
        frame[:,:,3] = 255
        for _ in range(100):
            cam.send(frame)
            cam.sleep_until_next_frame()
