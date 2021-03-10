from typing import Optional
import platform
import time
import warnings

import numpy as np

from pyvirtualcam.util import FPSCounter

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

class Camera:
    def __init__(self, width: int, height: int, fps: float, delay=None,
                 print_fps=False, backend: Optional[str]=None, **kw) -> None:
        if backend:
            backends = [(backend, BACKENDS[backend])]
        else:
            backends = list(BACKENDS.items())
        self._backend = None # for __del__ in case backend raises exception
        errors = []
        for name, clazz in backends:
            try:
                self._backend = clazz(width=width, height=height, fps=fps, **kw)
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
        self._print_fps = print_fps

        self._fps_counter = FPSCounter(fps)
        self._fps_warning_printed = False
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
    def frames_sent(self) -> int:
        return self._frames_sent

    def close(self) -> None:
        self._backend.close()

    def send(self, frame: np.ndarray) -> None:
        if frame.ndim != 3 or frame.shape[0] != self._height or frame.shape[1] != self._width:
            raise ValueError(f"mismatching frame dimensions: {frame.shape} != ({self._height}, {self._width}, 3)")
        if frame.shape[2] == 4:
            warnings.warn('RGBA frames are deprecated, use RGB instead', DeprecationWarning)
            frame = frame[:,:,:3]
        elif frame.shape[2] != 3:
            raise ValueError(f"invalid number of color channels, must be RGB or (deprecated) RGBA")

        self._frames_sent += 1
        self._last_frame_t = time.perf_counter()
        self._fps_counter.measure()

        if self._print_fps and self._last_frame_t - self._fps_last_printed > 1:
            self._fps_last_printed = self._last_frame_t
            print(f'{self._fps_counter.avg_fps:.1f} fps')

        # The first few frames may lead to a lower fps, so we only print the
        # warning after the rate has stabilized a bit.
        if not self._fps_warning_printed and self._frames_sent > 100 and \
                self._fps_counter.avg_fps / self._fps < 0.5:
            self._fps_warning_printed = True
            warnings.warn(
                f'current fps ({self._fps_counter.avg_fps:.1f}) much lower '
                f'than camera fps ({self._fps:.1f}), '
                f'consider lowering the camera fps')
        
        self._backend.send(frame)
        
    @property
    def current_fps(self) -> float:
        return self._fps_counter.avg_fps

    def sleep_until_next_frame(self) -> None:
        next_frame_t = self._last_frame_t + 1 / self._fps
        current_t = time.perf_counter()
        if current_t < next_frame_t:
            factor = (self._fps - self._fps_counter.avg_fps) / self._fps_counter.avg_fps
            self._extra_time_per_frame += 0.01 * factor
            self._extra_time_per_frame = max(0, self._extra_time_per_frame)
            t_sleep = next_frame_t - current_t - self._extra_time_per_frame
            if t_sleep > 0:
                time.sleep(t_sleep)
