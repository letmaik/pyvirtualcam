import pyvirtualcam
import numpy as np
from numpy import asarray
import cv2

path = 'cycle-1.mp4'
video = cv2.VideoCapture(path)
length = int(video.get(cv2.CAP_PROP_FRAME_COUNT))
count = 0
with pyvirtualcam.Camera(width=1280, height=720, fps=20) as cam:
    while True:
        if count == length:
            count = 0
        video.set(1,count)
        s,frame = video.read()
        frame = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
        array_frame = asarray(frame)
        frame = np.zeros((cam.height, cam.width, 4), np.uint8)
        frame[:,:,:3] = array_frame
        cam.send(frame)
        cam.sleep_until_next_frame()
        count = count + 1
