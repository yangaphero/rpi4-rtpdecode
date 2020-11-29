树莓派4接收rtp(udp)h264流解码显示



使用sudo ./cktool -s 15 -f 30 -a 192.168.2.112 -p 42000
或
ffmpeg  -re -i dream-1080p.mkv -an -vcodec h264  -x264opts "bframes=0:keyint=30" -b:v 4000k -f rtp rtp://192.168.50.189:42000 >test.sdp

-x264opts keyint=30   -tune zerolatency  
-x264opts "bframes=0:keyint=30"
-profile BaseLine、Extented、Main、High、Hight10

GOP设置：也可以通过-x264opts "keyint=50" 或 -g 50 
码率控制：-b:v 8000k