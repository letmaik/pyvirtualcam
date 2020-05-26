import time
import numpy as np
import pyvirtualcam

width = 640
height = 480
channels = 4
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
for i in range(1000):
    array = get_gradation_3d(width, height, (0, 0, 192), ((i*2) % 255, 255, 64), (True, False, False))
    bits = '{0:012b}'.format(i)
    frame = np.zeros((height, width, channels), np.uint8)
    frame[:,:,:3] = array
    frame[:,:,3] = 255
    bit_size = 5
    red = np.array([255,0,0], np.uint8)
    white = np.array([255,255,255], np.uint8)
    for i_bit, bit in enumerate(bits):
        frame[0:bit_size, i_bit*bit_size:i_bit*bit_size+bit_size, :3] = red if bit == '1' else white
    pyvirtualcam.send(i, frame.reshape(height, width * channels))
    time.sleep(1/fps)
pyvirtualcam.stop()
