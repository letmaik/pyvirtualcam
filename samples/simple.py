# This script is the sample from the README.

import numpy as np
import pyvirtualcam

with pyvirtualcam.Camera(width=1280, height=720, fps=20, print_fps=True) as cam:
    frame = np.zeros((cam.height, cam.width, 4), np.uint8)  # RGBA
    frame[:, :, 3] = 255
    while True:
        frame[:, :, :3] = cam.frames_sent % 255  # grayscale animation
        cam.send(frame)
        cam.sleep_until_next_frame()
