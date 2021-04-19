# This script sends a transparent GIF animation to an RGBA-compatible virtual camera.
# See the README on virtual cameras that support RGBA.
# OBS can be used to capture the RGBA frames for further processing.

import argparse
import os
import numpy as np
import imageio
import pyvirtualcam
from pyvirtualcam import PixelFormat

this_dir = os.path.dirname(__file__)
sample_gif_path = os.path.join(this_dir, "nyancat.gif")

parser = argparse.ArgumentParser()
parser.add_argument("--path", default=sample_gif_path, help="path to gif file")
parser.add_argument("--fps", action="store_true", help="output fps every second")
parser.add_argument("--device", help="virtual camera device, e.g. 'Unity Virtual Capture' (optional)")
args = parser.parse_args()

gif = imageio.get_reader(args.path)
gif_length = gif.get_length()
gif_height, gif_width = gif.get_data(0).shape[:2]
gif_fps = 1000 / gif.get_meta_data()['duration']

cam_width = 1280
cam_height = 720
cam_fmt = PixelFormat.RGBA

gif_x = (cam_width - gif_width) // 2
gif_y = (cam_height - gif_height) // 2

with pyvirtualcam.Camera(cam_width, cam_height, gif_fps, fmt=cam_fmt,
                         device=args.device, print_fps=args.fps) as cam:
    print(f'Virtual cam started: {cam.device} ({cam.width}x{cam.height} @ {cam.fps}fps)')

    print()
    print('Capturing in OBS with transparency:')
    print('1. Add "Video Capture Device" as new source')
    print(f'2. Select "{cam.device}" as device')
    print('3. Set "Resolution/FPS Type" to "Custom"')
    print(f'4. Set "Resolution" to match virtual camera: {cam.width}x{cam.height}')
    print('5. Set "Video Format" to "ARGB"')

    count = 0
    frame = np.zeros((cam.height, cam.width, 4), np.uint8)
    while True:
        # Restart gif on last frame.
        if count == gif_length:
            count = 0
        
        # Read gif frame.
        gif_frame = gif.get_data(count)
        
        # Prepare camera frame.
        frame[:] = 0
        frame[gif_y:gif_y+gif_height, gif_x:gif_x+gif_width] = gif_frame

        # Send to virtual cam.
        cam.send(frame)

        # Wait until it's time for the next frame
        cam.sleep_until_next_frame()
        
        count += 1
