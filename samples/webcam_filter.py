# This scripts uses OpenCV to capture webcam output, applies a filter,
# and sends it to the virtual camera.

import time
import cv2
import numpy as np
import pyvirtualcam

verbose = False

# Set up webcam capture
vc = cv2.VideoCapture(0) # 0 = default camera

if not vc.isOpened():
    raise RuntimeError('could not open video source')

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
print(f'webcam capture started ({width}x{height} @ {fps_in}fps)')

fps_out = 20.
spf_out = 1/fps_out

try:
    delay = 0 # low-latency, reduces internal queue size
    pyvirtualcam.start(width, height, fps_out, delay)
    print(f'virtual cam output started ({width}x{height} @ {fps_out}fps)')
    try:
        i = 0
        t_last = 0
        while True:
            # Try to maintain target fps
            t_now = time.time()
            t_elapsed = t_now - t_last
            if t_elapsed < spf_out:
                if verbose:
                    print(f'sleeping {spf_out - t_elapsed}s')
                time.sleep(spf_out - t_elapsed)
            else:
                if verbose:
                    print(f'not sleeping (elapsed={t_elapsed}s)')
            t_last = t_now

            # Read frame from webcam
            rval, in_frame = vc.read()
            if not rval:
                raise RuntimeError('error fetching frame')

            # Apply cartoon filter
            # (https://pysource.com/2018/10/11/how-to-create-a-cartoon-effect-opencv-with-python/)
            # 1) Edges
            gray = cv2.cvtColor(in_frame, cv2.COLOR_BGR2GRAY)
            gray = cv2.medianBlur(gray, 5)
            edges = cv2.adaptiveThreshold(gray, 255, cv2.ADAPTIVE_THRESH_MEAN_C, cv2.THRESH_BINARY, 9, 9)
            # 2) Color
            color = cv2.bilateralFilter(in_frame, 9, 300, 300)
            # 3) Cartoon
            cartoon = cv2.bitwise_and(color, color, mask=edges)
            assert cartoon.shape == (height, width, 3)

            # convert to RGBA
            out_frame = cv2.cvtColor(cartoon, cv2.COLOR_BGR2RGB)
            out_frame_rgba = np.zeros((height, width, 4), np.uint8)
            out_frame_rgba[:,:,:3] = out_frame
            out_frame_rgba[:,:,3] = 255

            # Send to virtual cam
            pyvirtualcam.send(i, out_frame_rgba)
            i += 1
    finally:
        pyvirtualcam.stop()
        print('virtual cam output stopped')
finally:
    vc.release()
    print('webcam capture stopped')
