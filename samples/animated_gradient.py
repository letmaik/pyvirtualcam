# This script simply sends an animated gradient to the virtual camera.

import time
import numpy as np
import pyvirtualcam

width = 1280
height = 720
fps = 30

# https://note.nkmk.me/en/python-numpy-generate-gradation-image/
def get_gradation_2d(start, stop, width, height, is_horizontal):
    if is_horizontal:
        return np.tile(np.linspace(start, stop, width), (height, 1))
    else:
        return np.tile(np.linspace(start, stop, height), (width, 1)).T

def get_gradation_3d(width, height, start_list, stop_list, is_horizontal_list):
    result = np.zeros((height, width, len(start_list)), dtype=np.float)

    for i, (start, stop, is_horizontal) in enumerate(zip(start_list, stop_list, is_horizontal_list)):
        result[:, :, i] = get_gradation_2d(start, stop, width, height, is_horizontal)

    return result

pyvirtualcam.start(width, height, fps, 10)
print(f'virtual cam output started ({width}x{height} @ {fps}fps)')
try:
    i = 0
    while True:
        # Things we want to draw:
        # 1. A color gradient.
        gradient = get_gradation_3d(width, height, (0, 0, 192), ((i*2) % 255, 255, 64), (True, False, False))
        # 2. A binary-encoded frame counter.
        bits = '{0:012b}'.format(i)
        bit_size = 10
        red = np.array([255,0,0], np.uint8)
        white = np.array([255,255,255], np.uint8)

        # Create a new frame.
        frame = np.zeros((height, width, 4), np.uint8)
        frame[:,:,:3] = gradient
        frame[:,:,3] = 255        
        for i_bit, bit in enumerate(bits):
            bit_color = red if bit == '1' else white
            frame[:bit_size, i_bit*bit_size:i_bit*bit_size+bit_size, :3] = bit_color

        # Send to the virtual cam.
        pyvirtualcam.send(i, frame)

        # Wait until it's time to prepare the next frame.
        time.sleep(1/fps)

        i += 1
finally:
    pyvirtualcam.stop()
    print('virtual cam output stopped')
