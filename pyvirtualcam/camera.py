import platform
import time
import warnings
from abc import ABC, abstractmethod

import numpy as np

from pyvirtualcam.util import FPSCounter

if platform.system() == 'Windows':
    from pyvirtualcam import _native_windows
else:
    raise NotImplementedError('unsupported OS')


class CameraBase(ABC):
    @abstractmethod
    def __init__(self, width: int, height: int, fps: float, delay: int, print_fps: bool) -> None:
        self._width = width
        self._height = height
        self._fps = fps
        self._delay = delay
        self._print_fps = print_fps

        self._fps_counter = FPSCounter(fps)
        self._fps_warning_printed = False
        self._fps_last_printed = time.perf_counter()
        self._frames_sent = 0
        self._last_frame_t = None
        self._extra_time_per_frame = 0
    
    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_value, traceback) -> bool:
        self.close()
        return False

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

    @abstractmethod
    def close(self) -> None:
        pass

    @abstractmethod
    def send(self, frame: np.ndarray) -> None:
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


class _WindowsCamera(CameraBase):
    def __init__(self, width: int, height: int, fps: float, delay=10, print_fps=False) -> None:
        super().__init__(width, height, fps, delay, print_fps)
        _native_windows.start(width, height, fps, delay)

    def close(self) -> None:
        super().close()
        _native_windows.stop()

    def send(self, frame: np.ndarray) -> None:
        super().send(frame)
        _native_windows.send(frame)


if platform.system() == 'Windows':
    Camera = _WindowsCamera
