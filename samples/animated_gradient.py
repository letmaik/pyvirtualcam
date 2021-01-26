# This script simply sends an animated gradient to the virtual camera.

import numpy as np
import pyvirtualcam

width_ = 1280
height_ = 720
fps_ = 20


# https://note.nkmk.me/en/python-numpy-generate-gradation-image/
def get_gradation_2d(start, stop, width, height, is_horizontal):
    if is_horizontal:
        return np.tile(np.linspace(start, stop, width), (height, 1))
    else:
        return np.tile(np.linspace(start, stop, height), (width, 1)).T


def get_gradation_3d(width, height, start_list, stop_list, is_horizontal_list):
    result = np.zeros((height, width, len(start_list)), np.float)

    for i, (start, stop, is_horizontal) in enumerate(zip(start_list, stop_list, is_horizontal_list)):
        result[:, :, i] = get_gradation_2d(start, stop, width, height, is_horizontal)

    return result


with pyvirtualcam.Camera(width_, height_, fps_, print_fps=True) as cam:
    print(f'Virtual cam started ({width_}x{height_} @ {fps_}fps)')
    j = 0
    while True:
        # Things we want to draw:
        # 1. A color gradient.
        gradient = get_gradation_3d(WIDTH, HEIGHT,
                                    start_list=(0, 0, 192),
                                    stop_list=((frame_counter * 2) % 255, 255, 64),
                                    is_horizontal_list=(True, False, False))
        # 2. A binary-encoded frame counter.
        bits = f'{j:012b}'
        bit_size = 10
        red = np.array([255, 0, 0], np.uint8)
        white = np.array([255, 255, 255], np.uint8)

        # Create a new frame.
        frame = np.zeros((height_, width_, 4), np.uint8)
        frame[:, :, :3] = gradient
        frame[:, :, 3] = 255
        for i_bit, bit in enumerate(bits):
            bit_color = red if bit == '1' else white
            frame[:bit_size, i_bit * bit_size:i_bit * bit_size + bit_size, :3] = bit_color

        # Send to the virtual cam.
        cam.send(frame)

        # Wait until it's time to draw the next frame.
        cam.sleep_until_next_frame()

        j += 1
