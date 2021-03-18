# This script plays back a video file on the virtual camera.

import argparse
import pyvirtualcam
from pyvirtualcam import PixelFormat
import cv2

parser = argparse.ArgumentParser()
parser.add_argument("video_path", help="path to input video file")
parser.add_argument("--fps", action="store_true", help="output fps every second")
args = parser.parse_args()

video = cv2.VideoCapture(args.video_path)
if not video.isOpened():
    raise ValueError("error opening video")
length = int(video.get(cv2.CAP_PROP_FRAME_COUNT))
width = int(video.get(cv2.CAP_PROP_FRAME_WIDTH))
height = int(video.get(cv2.CAP_PROP_FRAME_HEIGHT))
fps = video.get(cv2.CAP_PROP_FPS)

with pyvirtualcam.Camera(width, height, fps, fmt=PixelFormat.BGR, print_fps=args.fps) as cam:
    print(f'Virtual cam started: {cam.device} ({cam.width}x{cam.height} @ {cam.fps}fps)')
    count = 0
    while True:
        # Restart video on last frame.
        if count == length:
            count = 0
            video.set(cv2.CAP_PROP_POS_FRAMES, 0)
        
        # Read video frame.
        ret, frame = video.read()
        if not ret:
            raise RuntimeError('Error fetching frame')
        
        # Send to virtual cam.
        cam.send(frame)

        # Wait until it's time for the next frame
        cam.sleep_until_next_frame()
        
        count += 1
