#!/bin/bash

ffmpeg \
-f x11grab \
-video_size 2560x1600 \
-framerate 30 \
-i :1.0 \
-c:v h264_nvenc \
-g 30 \
-f rtsp \
-rtsp_transport tcp \
rtsp://192.168.8.10:8554/screen