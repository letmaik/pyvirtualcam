# This script simply sends an animated gradient to the virtual camera.

import numpy as np
import pyvirtualcam


# https://note.nkmk.me/en/python-numpy-generate-gradation-image/
def get_gradation_2d(start, stop, width, height, is_horizontal):
    if is_horizontal:
        return np.tile(np.linspace(start, stop, width), (height, 1))
    else:
        return np.tile(np.linspace(start, stop, height), (width, 1)).T


def get_gradation_3d(out, start_list, stop_list, is_horizontal_list):
    height = out.shape[0]
    width = out.shape[1]
    for i, (start, stop, is_horizontal) in enumerate(zip(start_list, stop_list, is_horizontal_list)):
        out[:, :, i] = get_gradation_2d(start, stop, width, height, is_horizontal)

speed = 2
red = np.array([255, 0, 0], np.uint8)
white = np.array([255, 255, 255], np.uint8)

with pyvirtualcam.Camera(width=1280, height=720, fps=20, print_fps=True) as cam:
    print(f'Virtual cam started ({cam.width}x{cam.height} @ {cam.fps}fps)')
    reverse = False
    last_stop = 0
    frame = np.zeros((cam.height, cam.width, 4), np.uint8)
    frame[:, :, 3] = 255
    while True:
        # Draw a color gradient.
        stop = (cam.frames_sent * speed) % 255
        if stop < last_stop:
            reverse = not reverse
        last_stop = stop
        if reverse:
            stop = 255 - stop
        get_gradation_3d(out=frame,
                         start_list=(0, 0, 192),
                         stop_list=(stop, 255 - stop / 2, stop),
                         is_horizontal_list=(True, False, False))

        # Draw a binary-encoded frame counter.
        bits = f'{cam.frames_sent:012b}'
        bit_size = 10
        for i_bit, bit in enumerate(bits):
            bit_color = red if bit == '1' else white
            frame[:bit_size, i_bit * bit_size:i_bit * bit_size + bit_size, :3] = bit_color

        # Send to the virtual cam.
        cam.send(frame)

        # Wait until it's time to draw the next frame.
        cam.sleep_until_next_frame()
