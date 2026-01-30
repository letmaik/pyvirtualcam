import os
import signal
from typing import Union, Tuple
import sys
import subprocess
import json
import traceback
import platform
import time
from pathlib import Path

import pytest
import numpy as np
import imageio

# This guard exists only to deal with a macOS/GitHub Actions scenario.
# See dev-requirements.txt for more details.
try:
    import cv2
except ImportError:
    if os.environ.get('CI'):
        assert platform.system() == 'Darwin'
        print('Skipping due to https://github.com/opencv/opencv-python/issues/291#issuecomment-841816850')
    pytest.skip('OpenCV is not installed', allow_module_level=True)

import pyvirtualcam
from pyvirtualcam import PixelFormat

if platform.system() == 'Windows':
    import pyvirtualcam_win_dshow_capture as dshow

    def capture_rgb(device: Union[str, int], width: int, height: int) -> Tuple[np.ndarray, float]:
        assert isinstance(device, str), "Windows requires device to be a string"
        rgb, timestamp_ms = dshow.capture(device, width, height)
        return rgb, timestamp_ms

elif platform.system() in ['Linux', 'Darwin']:
    def capture_rgb(device: Union[str, int], width: int, height: int) -> Tuple[np.ndarray, float]:
        print(f'Opening device {device} for capture')
        vc = cv2.VideoCapture(device)
        assert vc.isOpened()
        print(f'Configuring resolution of device {device}')
        vc.set(cv2.CAP_PROP_FRAME_WIDTH, width)
        vc.set(cv2.CAP_PROP_FRAME_HEIGHT, height)
        print(f'Reading frame from device {device}')
        for _ in range(50):
            vc.read()
        vc.grab()
        timestamp_ms = get_timestamp_ms()
        ret, frame = vc.retrieve()
        assert ret
        rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
        return rgb, timestamp_ms

w = 1280
h = 720
fps = 20

def get_test_frame(w, h, fmt: PixelFormat):
    def rgb_color(r, g, b):
        if fmt == PixelFormat.RGB:
            return np.array([r, g, b], np.uint8).reshape(1,1,3)
        elif fmt == PixelFormat.RGBA:
            return np.array([r, g, b, 255], np.uint8).reshape(1,1,4)
        elif fmt == PixelFormat.BGR:
            return np.array([b, g, r], np.uint8).reshape(1,1,3)
    
    if fmt in [PixelFormat.RGB, PixelFormat.BGR, PixelFormat.RGBA]:
        c = 4 if fmt == PixelFormat.RGBA else 3
        frame = np.empty((h, w, c), np.uint8)
        frame[:h//2,:w//2] = rgb_color(220, 20, 60)
        frame[:h//2,w//2:] = rgb_color(240, 230, 140)
        frame[h//2:,:w//2] = rgb_color(50, 205, 50)
        frame[h//2:,w//2:] = rgb_color(238, 130, 238)
    elif fmt == PixelFormat.GRAY:
        frame = np.empty((h, w), np.uint8)
        frame[:h//2,:w//2] = 30
        frame[:h//2,w//2:] = 60
        frame[h//2:,:w//2] = 150
        frame[h//2:,w//2:] = 230
    elif fmt == PixelFormat.I420:
        frame = np.empty((h + h // 2, w), np.uint8)
        # Y plane
        frame[:h//2,:w//2] = 30
        frame[:h//2,w//2:] = 60
        frame[h//2:h,:w//2] = 100
        frame[h//2:h,w//2:] = 200
        # UV planes
        s = h // 4
        u = h
        v = h + s
        frame[u:u+s//2] = 100
        frame[u+s//2:v] = 10
        frame[v:v+s//2] = 30
        frame[v+s//2:] = 200
    elif fmt == PixelFormat.NV12:
        frame = get_test_frame(w, h, PixelFormat.I420)
        # UUVV -> UVUV
        u = frame[h:h + h // 4].copy()
        v = frame[h + h // 4:].copy()
        uv = frame[h:].reshape(-1)
        uv[::2] = u.reshape(-1)
        uv[1::2] = v.reshape(-1)
    elif fmt in [PixelFormat.YUYV, PixelFormat.UYVY]:
        frame = np.empty((h, w, 2), np.uint8)
        if fmt == PixelFormat.YUYV:
            y = frame.reshape(-1)[::2].reshape(h, w)
            u = frame.reshape(-1)[1::4].reshape(h, w // 2)
            v = frame.reshape(-1)[3::4].reshape(h, w // 2)
        elif fmt == PixelFormat.UYVY:
            y = frame.reshape(-1)[1::2].reshape(h, w)
            u = frame.reshape(-1)[::4].reshape(h, w // 2)
            v = frame.reshape(-1)[2::4].reshape(h, w // 2)
        else:
            assert False
        y[:h//2,:w//2] = 30
        y[:h//2,w//2:] = 60
        y[h//2:h,:w//2] = 100
        y[h//2:h,w//2:] = 200
        u[:h//2] = 100
        u[h//2:] = 10
        v[:h//2] = 30
        v[h//2:] = 200
    else:
        assert False
    return frame

def get_timestamp_ms():
    return round(time.time() * 1000)

def write_timestamp_ms(frame: np.ndarray):
    timestamp_ms = get_timestamp_ms()
    timestamp_b = timestamp_ms.to_bytes(8, sys.byteorder)
    bits = np.unpackbits(np.frombuffer(timestamp_b, np.uint8))
    if frame.ndim == 3:
        bits = bits[..., np.newaxis]
    frame[0, :len(bits)] = bits * 255
    return timestamp_ms

def read_timestamp_ms(frame: np.ndarray) -> int:
    raw = np.atleast_3d(frame)[0,:64,0]
    bits = (raw > 50).astype(np.uint8)
    timestamp_b = np.packbits(bits)
    timestamp_ms = int.from_bytes(timestamp_b, sys.byteorder)
    return timestamp_ms

formats = [
    PixelFormat.RGB,
    PixelFormat.RGBA,
    PixelFormat.BGR,
    PixelFormat.GRAY,
    PixelFormat.I420,
    PixelFormat.NV12,
    PixelFormat.YUYV,
    PixelFormat.UYVY,
]

frames = { fmt: get_test_frame(w, h, fmt) for fmt in formats }

frames_rgb = {
    PixelFormat.RGB: frames[PixelFormat.RGB],
    PixelFormat.RGBA: cv2.cvtColor(frames[PixelFormat.RGBA], cv2.COLOR_RGBA2RGB),
    PixelFormat.BGR: cv2.cvtColor(frames[PixelFormat.BGR], cv2.COLOR_BGR2RGB),
    PixelFormat.GRAY: cv2.cvtColor(frames[PixelFormat.GRAY], cv2.COLOR_GRAY2RGB),
    PixelFormat.I420: cv2.cvtColor(frames[PixelFormat.I420], cv2.COLOR_YUV2RGB_I420),
    PixelFormat.NV12: cv2.cvtColor(frames[PixelFormat.NV12], cv2.COLOR_YUV2RGB_NV12),
    PixelFormat.YUYV: cv2.cvtColor(frames[PixelFormat.YUYV], cv2.COLOR_YUV2RGB_YUYV),
    PixelFormat.UYVY: cv2.cvtColor(frames[PixelFormat.UYVY], cv2.COLOR_YUV2RGB_UYVY),
}

@pytest.mark.parametrize("backend", list(pyvirtualcam.camera.BACKENDS))
@pytest.mark.parametrize("fmt", formats)
@pytest.mark.parametrize("mode", ['latency', 'diff'])
def test_capture(backend: str, fmt: PixelFormat, mode: str, tmp_path: Path):
    if fmt == PixelFormat.NV12 and platform.system() == 'Linux':
        pytest.skip('OpenCV VideoCapture does not support NV12')

    if fmt == PixelFormat.RGBA and backend != 'unitycapture':
        pytest.skip('RGBA only supported by unitycapture backend')

    measure_latency = mode == 'latency'
    if measure_latency:
        if fmt in [PixelFormat.UYVY, PixelFormat.YUYV]:
            pytest.skip('Latency test currently not supported for packed YUV formats')
        if fmt in [PixelFormat.I420, PixelFormat.NV12] and platform.system() == 'Darwin':
            # https://github.com/letmaik/pyvirtualcam/issues/82
            pytest.skip('Latency test currently not supported for I420 & NV12 on macOS')

    # informational only
    imageio.imwrite(f'test_{fmt}_in.png', frames_rgb[fmt])

    # Sending frames via pyvirtualcam and capturing them in parallel via OpenCV / DShow
    # is done in separate processes to avoid locking and cleanup issues.
    info_path = tmp_path / 'info.json'
    p = subprocess.Popen([
        sys.executable, __file__,
        '--action', 'send',
        '--mode', mode,
        '--backend', backend,
        '--fmt', str(fmt),
        '--info-path', str(info_path)])
    try:
        # wait for subprocess to start up and start sending frames
        time.sleep(5)
        alive = p.poll() is None
        assert alive
        
        subprocess.run([
            sys.executable, __file__,
            '--action', 'capture',
            '--mode', mode,
            '--backend', backend,
            '--fmt', str(fmt),
            '--info-path', str(info_path)],
            check=True)
    finally:
        p.terminate()
        exitcode = p.wait()
        if platform.system() == 'Windows':
            assert exitcode == 1
        else:
            assert exitcode == -signal.SIGTERM

    captured_rgb = imageio.imread(get_capture_filename(mode, backend, fmt, 'png'))

    if measure_latency:
        frame_timestamp_ms = read_timestamp_ms(captured_rgb)
        with open(get_capture_filename(mode, backend, fmt, 'json')) as f:
            capture_timestamp_ms = json.load(f)["capture_timestamp_ms"]
        latency_ms = capture_timestamp_ms - frame_timestamp_ms
        with open(get_capture_filename(mode, backend, fmt, 'json'), 'w') as f:
            json.dump({
                "latency_ms": latency_ms
            }, f, indent=2)
        assert 0 <= latency_ms <= 1000
        print(f'Latency: {latency_ms} ms')
    else:
        d = np.fabs(captured_rgb.astype(np.int16) - frames_rgb[fmt]).max()
        if platform.system() == 'Darwin':
            # https://github.com/letmaik/pyvirtualcam/issues/82
            assert d <= 35
        else:
            assert d <= 2

def test_frame_timestamp_ms():
    frame = np.zeros((1, 64), np.uint8)
    t1 = write_timestamp_ms(frame)
    t2 = read_timestamp_ms(frame)
    assert t1 == t2

def get_capture_filename(mode: str, backend: str, fmt: PixelFormat, ext: str):
    pyver = f'{sys.version_info.major}{sys.version_info.minor}'
    return f'test_{fmt}_out_{mode}_{platform.system()}_{backend}_py{pyver}.{ext}'

def send_frames(backend: str, fmt: PixelFormat, info_path: Path, mode: str):
    frame = frames[fmt]
    if mode == 'latency':
        frame[:] = 0
    try:
        with pyvirtualcam.Camera(w, h, fps, backend=backend, fmt=fmt) as cam:
            print(f'sending frames to {cam.device}...')
            with open(info_path, 'w') as f:
                json.dump(cam.device, f)
            while True:
                if mode == 'latency':
                    write_timestamp_ms(frame)
                cam.send(frame)
                cam.sleep_until_next_frame()
    except:
        traceback.print_exc()
        sys.exit(2)

def capture_frame(backend: str, fmt: PixelFormat, info_path: Path, mode: str):
    if platform.system() == 'Darwin':
        device: Union[str, int] = 0
    else:
        with open(info_path) as f:
            device = json.load(f)
    captured_rgb, timestamp_ms = capture_rgb(device, w, h)
    imageio.imwrite(get_capture_filename(mode, backend, fmt, 'png'), captured_rgb)
    if mode == 'latency':
        with open(get_capture_filename(mode, backend, fmt, 'json'), 'w') as f:
            json.dump({
                "capture_timestamp_ms": timestamp_ms
            }, f)

if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument('--action', choices=['send', 'capture'], required=True)
    parser.add_argument('--mode', choices=['diff', 'latency'], required=True)
    parser.add_argument('--backend', choices=list(pyvirtualcam.camera.BACKENDS), required=True)
    parser.add_argument('--fmt', type=lambda fmt: PixelFormat[fmt], required=True)
    parser.add_argument('--info-path', type=Path, required=True)
    args = parser.parse_args()
    if args.action == 'send':
        send_frames(args.backend, args.fmt, args.info_path, args.mode)
    else:
        capture_frame(args.backend, args.fmt, args.info_path, args.mode)
    