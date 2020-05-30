import time

class FPSCounter(object):
    def __init__(self):
        self.t_prev = time.perf_counter()
        self.avg_delta = None
    
    def measure(self):
        now = time.perf_counter()
        delta = now - self.t_prev
        if self.avg_delta is None:
            self.avg_delta = delta
        else:
            self.avg_delta += (delta - self.avg_delta) * 0.2
        self.t_prev = now
    
    @property
    def avg_fps(self):
        return 1 / self.avg_delta
