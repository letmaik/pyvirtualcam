# This scripts uses OpenCV to capture webcam output, applies a filter,
# and sends it to the virtual camera.

import cv2
import numpy as np
import pyvirtualcam

verbose = False

# Set up webcam capture
vc = cv2.VideoCapture(0)  # 0 = default camera

if not vc.isOpened():
    raise RuntimeError('Could not open video source')

pref_width = 1280
pref_height = 720
pref_fps_in = 30
vc.set(cv2.CAP_PROP_FRAME_WIDTH, pref_width)
vc.set(cv2.CAP_PROP_FRAME_HEIGHT, pref_height)
vc.set(cv2.CAP_PROP_FPS, pref_fps_in)

# Query final capture device values (may be different from preferred settings)
width = int(vc.get(cv2.CAP_PROP_FRAME_WIDTH))
height = int(vc.get(cv2.CAP_PROP_FRAME_HEIGHT))
fps_in = vc.get(cv2.CAP_PROP_FPS)
print(f'Webcam capture started ({width}x{height} @ {fps_in}fps)')

fps_out = 20

try:
    delay = 0  # low-latency, reduces internal queue size

    with pyvirtualcam.Camera(width, height, fps_out, delay, print_fps=True) as cam:
        print(f'Virtual cam started ({width}x{height} @ {fps_out}fps)')

        # Shake two channels horizontally each frame
        channels = [[0, 1], [0, 2], [1, 2]]

        while True:
            # Read frame from webcam
            ret, in_frame = vc.read()
            if not ret:
                raise RuntimeError('Error fetching frame')

            # Shake
            dx = 15 - cam.frames_sent % 5
            c1, c2 = channels[cam.frames_sent % 3]
            in_frame[:,:-dx,c1] = in_frame[:,dx:,c1]
            in_frame[:,dx:,c2] = in_frame[:,:-dx,c2]

            # Convert to RGBA
            out_frame = cv2.cvtColor(in_frame, cv2.COLOR_BGR2RGBA)

            # Send to virtual cam
            cam.send(out_frame)

            # Wait until it's time for the next frame
            cam.sleep_until_next_frame()
finally:
    vc.release()
