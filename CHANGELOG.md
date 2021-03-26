# Changelog
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.7.0] - 2021-03-26
### Added
- [API documentation](https://letmaik.github.io/pyvirtualcam).
- `pyvirtualcam.register_backend()` for registering custom backends.
- `latency.py` sample for visually evaluating latency.
- `--filter` flag in `webcam_filter.py` sample for choosing a filter (`shake` or `none`).

## [0.6.0] - 2021-03-22
### Added
- Support for device selection on Linux: `Camera(..., device="/dev/video0")`. On Windows/macOS there is only a single valid device `"OBS Virtual Camera"` when using the built-in backends.
- Support for common pixel formats: RGB (default), BGR (useful for OpenCV), GRAY, I420, NV12, YUYV, UYVY.
- New properties `Camera.fmt` (input format) and `Camera.native_fmt` (native format).

### Removed
- RGBA support has been removed. Use RGB instead.

## [0.5.0] - 2021-03-13
### Added
- Linux: multiple camera support (#37).
- If `print_fps=True` and `Camera.sleep_until_next_frame()` is used, the percentage of time spent in computation is printed in addition to fps.

### Changed
- macOS/Windows: raise error if OBS virtual camera is not installed.

### Fixed
- macOS: camera was not correctly cleaned up after use (#39).
- macOS: an error is now raised if the camera is already in use outside of the current process (#39).

## [0.4.0] - 2021-03-07
### Added
- macOS support via [OBS Virtual Camera (built-in)](https://github.com/obsproject/obs-studio/releases/tag/26.0.0) (#16).
- Linux support via [v4l2loopback](https://github.com/umlaeute/v4l2loopback) (#29).
- RGB frame support (in addition to RGBA).
- `Camera.device` property containing the name of the virtual camera device.
- New sample: send video frames to virtual camera (#9).

### Changed
- Windows support relies on [OBS Virtual Camera (built-in)](https://github.com/obsproject/obs-studio/releases/tag/26.1.0) now. Support for [OBS-VirtualCam](https://github.com/CatxFish/obs-virtual-cam) has been removed (#25).

### Deprecated
- RGBA frame format. Use RGB instead.
- `delay` argument of `Camera` is deprecated and has no effect.

### Fixed
- Sending a frame with incorrect width or height now raises an exception instead of crashing (#17).

## [0.3.2] - 2021-01-27
### Changed
- PEP8.

### Fixed
- Prevent adaptive sleep from overshooting (#1).
- Return type annotation of `Camera.fps` was incorrect.

## [0.3.1] - 2021-01-27
### Fixed
- Fixed resource cleanup issue preventing `Camera` to be used more than once (#5).

## [0.3.0] - 2021-01-23
### Added
- Python 3.9 support.

## [0.2.0] - 2020-05-30
### Added
- FPS counter.
- Adaptive sleep using `Camera.sleep_until_next_frame()`.
- Minimal example.

### Changed
- API re-design.

### Removed
- Drop Python 3.5 support.

### Fixed
- Add missing numpy dependency.

## [0.1.0] - 2020-05-29
### Added
- First release.
- Windows only, Python 3.5 - 3.8.
- Support for [OBS-VirtualCam](https://github.com/CatxFish/obs-virtual-cam).


[0.7.0]: https://github.com/letmaik/pyvirtualcam/compare/v0.6.0...v0.7.0
[0.6.0]: https://github.com/letmaik/pyvirtualcam/compare/v0.5.0...v0.6.0
[0.5.0]: https://github.com/letmaik/pyvirtualcam/compare/v0.4.0...v0.5.0
[0.4.0]: https://github.com/letmaik/pyvirtualcam/compare/v0.3.2...v0.4.0
[0.3.2]: https://github.com/letmaik/pyvirtualcam/compare/v0.3.1...v0.3.2
[0.3.1]: https://github.com/letmaik/pyvirtualcam/compare/v0.3.0...v0.3.1
[0.3.0]: https://github.com/letmaik/pyvirtualcam/compare/v0.2.0...v0.3.0
[0.2.0]: https://github.com/letmaik/pyvirtualcam/compare/v0.1.0...v0.2.0
[0.1.0]: https://github.com/letmaik/pyvirtualcam/releases/tag/v0.1.0
