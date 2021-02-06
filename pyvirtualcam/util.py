import time


class FPSCounter(object):
    def __init__(self, initial_fps=1):
        self.initing = True
        self.t_prev = None
        self.avg_delta = 1 / initial_fps

    def measure(self):
        now = time.perf_counter()
        if self.t_prev is None:
            self.t_prev = now
        else:
            delta = now - self.t_prev
            if self.initing:
                self.avg_delta = delta
                self.initing = False
            else:
                self.avg_delta += (delta - self.avg_delta) * 0.2
            self.t_prev = now

    @property
    def avg_fps(self):
        return 1 / self.avg_delta
