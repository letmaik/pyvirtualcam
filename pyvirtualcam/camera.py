from typing import Optional
import platform
import time
import warnings
from enum import Enum

import numpy as np

from pyvirtualcam.util import FPSCounter, encode_fourcc, decode_fourcc

BACKENDS = {}

if platform.system() == 'Windows':
    from pyvirtualcam import _native_windows_obs
    BACKENDS['obs'] = _native_windows_obs.Camera
elif platform.system() == 'Darwin':
    from pyvirtualcam import _native_macos_obs
    BACKENDS['obs'] = _native_macos_obs.Camera
elif platform.system() == 'Linux':
    from pyvirtualcam import _native_linux_v4l2loopback
    BACKENDS['v4l2loopback'] = _native_linux_v4l2loopback.Camera

class PixelFormat(Enum):
    # See external/libyuv/include/libyuv/video_common.h for fourcc codes.
    RGB = 'raw '
    BGR = '24BG'
    GRAY = 'J400'

    I420 = 'I420'
    NV12 = 'NV12'

    YUYV = 'YUY2'
    UYVY = 'UYVY'

    def __str__(self):
        return self.name

FrameShapes = {
    PixelFormat.RGB: lambda w, h: (h, w, 3),
    PixelFormat.BGR: lambda w, h: (h, w, 3),
    PixelFormat.GRAY: lambda w, h: (h, w),
    PixelFormat.I420: lambda w, h: (w + w // 2) * h,
    PixelFormat.NV12: lambda w, h: (w + w // 2) * h,
    PixelFormat.YUYV: lambda w, h: h * w * 2,
    PixelFormat.UYVY: lambda w, h: h * w * 2,
}

class Camera:
    def __init__(self, width: int, height: int, fps: float, *,
                 fmt: PixelFormat=PixelFormat.RGB,
                 device: Optional[str]=None,
                 backend: Optional[str]=None,
                 print_fps=False,
                 delay=None,
                 **kw) -> None:
        if backend:
            backends = [(backend, BACKENDS[backend])]
        else:
            backends = list(BACKENDS.items())
        self._backend = None
        errors = []
        for name, clazz in backends:
            try:
                self._backend = clazz(
                    width=width, height=height, fps=fps,
                    fourcc=encode_fourcc(fmt.value),
                    device=device,
                    **kw)
            except Exception as e:
                errors.append(f"'{name}' backend: {e}")
            else:
                self._backend_name = name
                break
        if self._backend is None:
            raise RuntimeError('\n'.join(errors))

        self._width = width
        self._height = height
        self._fps = fps
        self._fmt = fmt
        self._print_fps = print_fps

        frame_shape = FrameShapes[fmt](width, height)
        if isinstance(frame_shape, int):
            def check_frame_shape(frame: np.ndarray):
                if frame.size != frame_shape:
                    raise ValueError(f"unexpected frame size: {frame.size} != {frame_shape}")
        else:
            def check_frame_shape(frame: np.ndarray):
                if frame.shape != frame_shape:
                    raise ValueError(f"unexpected frame shape: {frame.shape} != {frame_shape}")

        self._check_frame_shape = check_frame_shape

        self._fps_counter = FPSCounter(fps)
        self._fps_last_printed = time.perf_counter()
        self._frames_sent = 0
        self._last_frame_t = None
        self._extra_time_per_frame = 0

        if delay is not None:
            warnings.warn("'delay' argument is deprecated and has no effect", DeprecationWarning)

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_value, traceback) -> bool:
        self.close()
        return False
    
    def __del__(self):
        if self._backend is not None:
            self.close()

    @property
    def backend(self) -> str:
        return self._backend_name

    @property
    def device(self) -> str:
        return self._backend.device()

    @property
    def width(self) -> int:
        return self._width

    @property
    def height(self) -> int:
        return self._height

    @property
    def fps(self) -> float:
        return self._fps
    
    @property
    def fmt(self) -> PixelFormat:
        return self._fmt
    
    @property
    def native_fmt(self) -> PixelFormat:
        return PixelFormat(decode_fourcc(self._backend.native_fourcc()))

    @property
    def frames_sent(self) -> int:
        return self._frames_sent

    def close(self) -> None:
        self._backend.close()

    def send(self, frame: np.ndarray) -> None:
        if frame.dtype != np.uint8:
            raise TypeError(f'unexpected frame dtype: {frame.dtype} != uint8')
        
        self._check_frame_shape(frame)

        self._frames_sent += 1
        self._last_frame_t = time.perf_counter()
        self._fps_counter.measure()

        if self._print_fps and self._last_frame_t - self._fps_last_printed > 1:
            self._fps_last_printed = self._last_frame_t
            s = f'{self._fps_counter.avg_fps:.1f} fps'
            
            # If sleep_until_next_frame() is used, show percentage of frame time
            # spent in computation (vs sleeping).
            if self._extra_time_per_frame > 0:
                busy_ratio = min(1, self._extra_time_per_frame * self._fps)
                s += f' | {100*busy_ratio:.0f} %'
            
            print(s)
        
        self._backend.send(frame)
        
    @property
    def current_fps(self) -> float:
        return self._fps_counter.avg_fps

    def sleep_until_next_frame(self) -> None:
        next_frame_t = self._last_frame_t + 1 / self._fps
        current_t = time.perf_counter()
        if current_t < next_frame_t:
            factor = self._fps / self._fps_counter.avg_fps - 1
            self._extra_time_per_frame += 0.01 * factor
            self._extra_time_per_frame = max(0, self._extra_time_per_frame)
            t_sleep = next_frame_t - current_t - self._extra_time_per_frame
            if t_sleep > 0:
                time.sleep(t_sleep)
