ffmpeg \
  -loglevel debug \
  -f lavfi \
  -f x11grab \
  -framerate 30 \
  -i :1.0 \
  -vf "scale=1920:1080" \
  -c:v libx264 \
  -preset ultrafast \
  -rtsp_transport tcp \
  -f rtsp \
  rtsp://192.168.8.10:8554/camera3