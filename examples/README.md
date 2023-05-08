# Examples

## Simple

Source: [simple.py](simple.py)

Description: Shows basics of setting up a virtual camera and sending frames.

```sh
python ./examples/simple.py
```

<img width="500" src="screencasts/simple.gif">

## Video

Source: [video.py](video.py)

Description: Reads video frames from a file and sends them to the virtual camera.

```sh
# Download example mp4 from:
# https://file-examples.com/index.php/sample-video-files/sample-mp4-files/
python ./examples/video.py file_example_MP4_1280_10MG.mp4
```

<img width="500" src="screencasts/video.gif">

## Latency

Source: [latency.py](latency.py)

Description: Allows to evaluate latency visually by comparing color changes with corresponding output on the terminal.

```sh
python ./examples/latency.py
```

<img width="800" src="screencasts/latency.gif">

## Webcam Filter

Source: [webcam_filter.py](webcam_filter.py)

Description: Reads frames from a webcam, applies a filter, and sends them to the virtual camera. 

*screencast tbd*

## RGBA GIF

Source: [rgba_gif.py](rgba_gif.py)

Description: Reads RGBA frames from a GIF animation and sends them to a virtual camera while preserving transparency. Useful for post-processing in other software like OBS Studio.

*screencast tbd*
