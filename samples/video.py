# This script plays back a video file on the virtual camera.

import argparse
import pyvirtualcam
import cv2

parser = argparse.ArgumentParser()
parser.add_argument("video_path", help="path to input video file")
args = parser.parse_args()

video = cv2.VideoCapture(args.video_path)
if not video.isOpened():
    raise ValueError("error opening video")
length = int(video.get(cv2.CAP_PROP_FRAME_COUNT))
width = int(video.get(cv2.CAP_PROP_FRAME_WIDTH))
height = int(video.get(cv2.CAP_PROP_FRAME_HEIGHT))
fps = video.get(cv2.CAP_PROP_FPS)

with pyvirtualcam.Camera(width=width, height=height, fps=fps) as cam:
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
        
        # Convert to RGB.
        frame = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)

        # Send to virtual cam.
        cam.send(frame)

        # Wait until it's time for the next frame
        cam.sleep_until_next_frame()
        
        count += 1
