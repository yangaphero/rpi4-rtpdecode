树莓派4接收rtp(udp)h264流解码显示
使用sudo ./cktool -s 15 -f 30 -a 192.168.2.112 -p 42000
或ffmpeg  -re -i dream-1080p.mkv -an -vcodec h264 -preset ultrafast  -f rtp rtp://192.168.2.112:42000 >test.sdp