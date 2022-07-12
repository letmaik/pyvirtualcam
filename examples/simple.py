# This script is the sample from the README.

import numpy as np
import pyvirtualcam

with pyvirtualcam.Camera(width=3840, height=2160, fps=30, print_fps=True,backend="akvcam") as cam:
    print(f'Using virtual camera: {cam.device}')
    frame = np.zeros((cam.height, cam.width, 3), np.uint8)  # RGB
    while True:
        frame[:] = cam.frames_sent % 255  # grayscale animation
        cam.send(frame)
        cam.sleep_until_next_frame()
