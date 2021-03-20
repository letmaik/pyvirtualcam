import os
import platform
import time
import pytest
import pyvirtualcam.util

@pytest.mark.skipif(
    os.environ.get('CI') and platform.system() == 'Darwin',
    reason='disabled due to high fluctuations in CI, manually verified on MacBook Pro')
def test_fps_counter():
    target_fps = 20
    s_per_frame = 1 / target_fps
    counter = pyvirtualcam.util.FPSCounter()
    start = time.perf_counter()
    for frame_idx in range(60):
        end = start + s_per_frame
        while time.perf_counter() < end:
            pass
        counter.measure()
        start = end

    measured_avg_fps = counter.avg_fps
    diff = abs(measured_avg_fps - target_fps)
    assert diff < 0.2, measured_avg_fps
