# This script allows to visually evaluate latency by comparing
# periodic color changes with correponding output on the terminal.

import time
import numpy as np
import pyvirtualcam

colors = [
    ('RED', np.array([255, 0, 0], np.uint8)),
    ('GREEN', np.array([0, 255, 0], np.uint8)),
    ('BLUE', np.array([0, 0, 255], np.uint8))
]

with pyvirtualcam.Camera(width=1280, height=720, fps=30) as cam:
    print(f'Using virtual camera: {cam.device}')
    frame = np.zeros((cam.height, cam.width, 3), np.uint8)  # RGB
    t_last_change = 0
    color_idx = 0
    while True:
        t_now = time.time()
        if t_now - t_last_change >= 1:
            t_last_change = t_now
            color_idx = (color_idx + 1) % len(colors)
            name, color = colors[color_idx]
            print(name)
        frame[:,:] = color
        cam.send(frame)
        cam.sleep_until_next_frame()
