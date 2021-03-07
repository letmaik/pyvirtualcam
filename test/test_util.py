import time
import pyvirtualcam.util

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
