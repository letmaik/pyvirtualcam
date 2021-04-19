from typing import Optional, Dict, Type
from abc import ABC, abstractmethod
import platform
import time
import warnings
from enum import Enum

import numpy as np

from pyvirtualcam.util import FPSCounter, encode_fourcc, decode_fourcc

class Backend(ABC):
    """
    Describes the interface of Backend classes.

    Note that Backend classes should not inherit from this class,
    it exists only for documentation and static typing purposes.
    """

    @abstractmethod
    def __init__(self, *, width: int, height: int, fps: float,
                 fourcc: int, device: Optional[str], **kw):
        """
        All arguments are keyword-only arguments to avoid ambiguities.

        :param width: Image width of the frames passed to :meth:`send`.
            If the virtual camera does not support the given width and
            height combination then an exception must be raised.
        :param height: Image height of the frames passed to :meth:`send`.
        :param fps: Approximate rate at which :meth:`send` will be called,
            actual rate may be more or less. The backend should
            not expect a specific rate but may use this rate as a hint
            or an upper bound. If the rate is not one of the native frame
            rates supported by the virtual camera device, then a suitable
            rate should be chosen. An exception should generally not be raised.
        :param fourcc: Pixel format of frames passed to :meth:`send`, encoded as
            `libyuv FourCC <https://chromium.googlesource.com/libyuv/libyuv/+/refs/heads/master/include/libyuv/video_common.h>`_
            code. One of the codes in the :class:`~pyvirtualcam.PixelFormat` enum.
            If the pixel format is unsupported then an exception must
            be raised.
        :param device: If given, name of the virtual camera device to use,
            otherwise any available device can be used.
            Depending on the operating system, this is typically a device
            file name or a camera name as it appears in apps.
            If a device name is given then the specified device must be
            used or an exception raised if the device is unavailable.
            If no device name is given (``None``) and no device is available
            then an exception must be raised.
        :param kw: Extra keyword arguments passed through from user code.
        """
    
    @abstractmethod
    def close(self):
        """ Releases any resources associated with the virtual camera device.
        
        This method is guaranteed to be called exactly once.
        After this method was called, no further methods are called.
        """
    
    @abstractmethod
    def send(self, frame: np.ndarray):
        """ Send the given frame to the camera device.

        :param frame: A 1D C-contiguous uint8 numpy array corresponding
            to the chosen pixel format and frame width and height.
        """
    
    @abstractmethod
    def device(self) -> str:
        """ The name of the virtual camera device in use.

        If ``device`` was not ``None`` in the constructor, then
        the returned value must match the constructor argument.
        """
    
    @abstractmethod
    def native_fourcc(self) -> Optional[int]:
        """ The `libyuv FourCC <https://chromium.googlesource.com/libyuv/libyuv/+/refs/heads/master/include/libyuv/video_common.h>`_
        code of the native pixel format used in the backend.
        Must be one of the codes in the :class:`~pyvirtualcam.PixelFormat` enum,
        or ``None`` if no matching code is defined or the value
        for some other reason is not available or sensible.
        
        This does not necessarily correspond to all the formats
        that the device supports when a program captures from it,
        which is typical on Windows.
        Rather it is an indication on whether any pixel format
        conversions have to be done in the backend itself before
        sending the frame to the camera device.
        """

BACKENDS: Dict[str, Type[Backend]] = {}

def register_backend(name: str, clazz):
    """
    Register a new backend.

    If a backend with the same name is already registered,
    it will be replaced.

    :param name: Name of the backend.
        Used as ``backend`` argument in :meth:`~pyvirtualcam.Camera`.
    :param clazz: Class type of the backend conforming to :class:`~pyvirtualcam.Backend`.
    """
    BACKENDS[name] = clazz

if platform.system() == 'Windows':
    from pyvirtualcam import _native_windows_obs, _native_windows_unity_capture
    register_backend('obs', _native_windows_obs.Camera)
    register_backend('unitycapture', _native_windows_unity_capture.Camera)
elif platform.system() == 'Darwin':
    from pyvirtualcam import _native_macos_obs
    register_backend('obs', _native_macos_obs.Camera)
elif platform.system() == 'Linux':
    from pyvirtualcam import _native_linux_v4l2loopback
    register_backend('v4l2loopback', _native_linux_v4l2loopback.Camera)

class PixelFormat(Enum):
    """ Pixel formats.

    The enum values are libyuv FourCC codes.
    They are only used internally by the virtual camera backends.
    """

    # See external/libyuv/include/libyuv/video_common.h for fourcc codes.
    RGB = 'raw '
    """ Shape: ``(h,w,3)`` """

    BGR = '24BG'
    """ Shape: ``(h,w,3)`` """

    RGBA = 'ABGR'
    """ Shape: ``(h,w,4)`` """

    GRAY = 'J400'
    """ Shape: ``(h,w)`` """

    I420 = 'I420'
    """ Shape: any of size ``w * h * 3/2`` """

    NV12 = 'NV12'
    """ Shape: any of size ``w * h * 3/2`` """

    YUYV = 'YUY2'
    """ Shape: any of size ``w * h * 2`` """

    UYVY = 'UYVY'
    """ Shape: any of size ``w * h * 2`` """

    def __str__(self):
        return self.name
    
    def __repr__(self):
        return f'{type(self).__name__}.{self.name}'

FrameShapes = {
    PixelFormat.RGB: lambda w, h: (h, w, 3),
    PixelFormat.BGR: lambda w, h: (h, w, 3),
    PixelFormat.RGBA: lambda w, h: (h, w, 4),
    PixelFormat.GRAY: lambda w, h: (h, w),
    PixelFormat.I420: lambda w, h: w * h * 3 // 2,
    PixelFormat.NV12: lambda w, h: w * h * 3 // 2,
    PixelFormat.YUYV: lambda w, h: w * h * 2,
    PixelFormat.UYVY: lambda w, h: w * h * 2,
}

class Camera:
    """
    :param width: Frame width in pixels.
    :param height: Frame height in pixels.
    :param fps: Target frame rate in frames per second.
    :param fmt: Input pixel format.
    :param device: The virtual camera device to use.
        If ``None``, the first available device is used.

        Built-in backends:

        - ``v4l2loopback`` (Linux): ``/dev/video<n>``
        - ``obs`` (macOS/Windows): ``OBS Virtual Camera``
        - ``unitycapture`` (Windows): ``Unity Video Capture``, or the name you gave to the device
    :param backend: The virtual camera backend to use.
        If ``None``, all available backends are tried.

        Built-in backends:

        - ``v4l2loopback`` (Linux)
        - ``obs`` (macOS/Windows)
        - ``unitycapture`` (Windows)
    :param print_fps: Print frame rate every second.
    :param kw: Extra keyword arguments forwarded to the backend.
        Should only be given if a backend is specified.
        Note that the built-in backends do not have extra arguments.
    """
    def __init__(self, width: int, height: int, fps: float, *,
                 fmt: PixelFormat=PixelFormat.RGB,
                 device: Optional[str]=None,
                 backend: Optional[str]=None,
                 print_fps: bool=False,
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

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_value, traceback) -> bool:
        self.close()
        return False
    
    def __del__(self):
        self.close()

    @property
    def backend(self) -> str:
        """ The virtual camera backend in use.
        """
        return self._backend_name

    @property
    def device(self) -> str:
        """ The virtual camera device in use.
        """
        return self._backend.device()

    @property
    def width(self) -> int:
        """ Frame width in pixels.
        """
        return self._width

    @property
    def height(self) -> int:
        """ Frame height in pixels.
        """
        return self._height

    @property
    def fps(self) -> float:
        """ Target frame rate in frames per second.
        """
        return self._fps
    
    @property
    def fmt(self) -> PixelFormat:
        """ The input pixel format as accepted by :meth:`send`.
        """
        return self._fmt
    
    @property
    def native_fmt(self) -> Optional[PixelFormat]:
        """ The native pixel format of the virtual camera,
        or ``None`` if not known or otherwise unavailable.

        Note that this is the native format in use in the virtual
        camera backend and may not necessarily be the native format
        of the virtual camera device itself.
        For example, on Windows, a camera device typically
        supports multiple formats.
        """
        fourcc = self._backend.native_fourcc()
        return PixelFormat(decode_fourcc(fourcc)) if fourcc else None

    @property
    def frames_sent(self) -> int:
        """ Number of frames sent.
        """
        return self._frames_sent

    def close(self) -> None:
        """ Manually free resources.

        This method is automatically called when using ``with`` or
        when this instance goes out of scope.
        """
        if self._backend is not None:
            self._backend.close()
            self._backend = None

    def send(self, frame: np.ndarray) -> None:
        """Send a frame to the virtual camera device.

        :param frame: Frame to send. The shape of the array must match
            the chosen :class:`~pyvirtualcam.PixelFormat`.
        """
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
        
        frame = np.array(frame.reshape(-1), copy=False, order='C')
        self._backend.send(frame)
        
    @property
    def current_fps(self) -> float:
        """ Current measured frames per second. """
        return self._fps_counter.avg_fps

    def sleep_until_next_frame(self) -> None:
        """ Adaptively sleep until the next frame is due.

        This method continuously adapts the sleep duration
        in order to reach the target frame rate.
        As a side effect, it estimates the time spent in computation
        which is printed as a percentage if ``print_fps=True``
        is given as argument in the constructor.
        """
        next_frame_t = self._last_frame_t + 1 / self._fps
        current_t = time.perf_counter()
        if current_t < next_frame_t:
            factor = self._fps / self._fps_counter.avg_fps - 1
            self._extra_time_per_frame += 0.01 * factor
            self._extra_time_per_frame = max(0, self._extra_time_per_frame)
            t_sleep = next_frame_t - current_t - self._extra_time_per_frame
            if t_sleep > 0:
                time.sleep(t_sleep)
