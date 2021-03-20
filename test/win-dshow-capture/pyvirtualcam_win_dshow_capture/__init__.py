from pyvirtualcam_win_dshow_capture._win_dshow_capture import DShowCapture

def capture(device: str, pref_width: int, pref_height: int):
    dshow = DShowCapture(device, pref_width, pref_height)
    img = dshow.capture()
    if dshow.is_nv12():
        return img
    else:
        img = img[::-1]
        img_rgb = img.copy()
        img_rgb[:,:,0] = img[:,:,2]
        img_rgb[:,:,1] = img[:,:,1]
        img_rgb[:,:,2] = img[:,:,0]
        return img_rgb
