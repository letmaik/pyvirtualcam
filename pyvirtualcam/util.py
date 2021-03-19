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


def encode_fourcc(s: str) -> int:
    if len(s) != 4:
        raise ValueError('fourcc must be 4 characters')
    return ord(s[0]) | (ord(s[1]) << 8) | (ord(s[2]) << 16) | (ord(s[3]) << 24)

def decode_fourcc(i: int) -> str:
    return chr((i & 0xff)) + chr((i >> 8) & 0xff) + chr((i >> 16) & 0xff) + chr((i >> 24) & 0xff)
