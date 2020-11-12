#include <stdint.h>
#include <time.h>
#include <sys/time.h>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <netinet/in.h>  
#include <sys/socket.h> 

#include <libavutil/opt.h>

#include "bcm_host.h"
#include "avqueue.h"
#include "vid_decode.h"
#include "appdata.h"
#include "unpackrtp.h"
#include "sps_pps.h"
#define PACKET_MAX_SIZE     1024*1024*1



int main(int argc, char **argv)
{

	int sockfd;		
	unsigned char buffer[2048];
	int recv_len;
	packet   rtp_packet;
    AVPacket av_packet;
    AVCodec *videoCodec = NULL;
    struct omx_state omx;
    unsigned short before_seq;
    int error_packet=0;

	int tmp_len=0;//组包临时长度累加
    appData *userData = (appData*) malloc(sizeof(appData));
    memset(userData, 0, sizeof(appData));

    // 建立socket，注意必须是SOCK_DGRAM
	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) 
    {
		printf("socket error\n");
	}

    //端口复用
    int flag=1;
    if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(int)) == -1)  
    {  
        fprintf(stderr, "[%s@%s,%d]:socket setsockopt error\n",__func__, __FILE__, __LINE__);
    } 

    //设置接收超时
    struct timeval tv_out;
    tv_out.tv_sec = 0;//等待2秒
    tv_out.tv_usec = 1000*300;
    if(setsockopt(sockfd,SOL_SOCKET,SO_RCVTIMEO,&tv_out, sizeof(tv_out)) == -1)
    {  
        fprintf(stderr, "[%s@%s,%d]:socket setsockopt error\n",__func__, __FILE__, __LINE__);
    } 

    //绑定本地端口
    struct sockaddr_in local;  
    local.sin_family=AF_INET;  
    local.sin_port=htons(42000);            ///监听端口  
    local.sin_addr.s_addr=INADDR_ANY;       ///本机  
    if(bind(sockfd,(struct sockaddr*)&local,sizeof(local))==-1) 
    {
		fprintf(stderr,"[%s@%s,%d]:udp port bind error\n",__func__, __FILE__, __LINE__);
    }


    //初始化解码器,不打开
    //avcodec_register_all();//过时不用了
    videoCodec = avcodec_find_decoder(AV_CODEC_ID_H264);
    //videoCodec = avcodec_find_decoder_by_name("h264_mmal");
    if (videoCodec == NULL)
    {
	    fprintf(stderr, "Unsupported video codec!\n");
    }else
    {
        printf("Video codec: %s\n", videoCodec->name);
        userData->decContext = avcodec_alloc_context3(videoCodec);
    }
    //初始化omx渲染器
    if (omx_init(&omx) != 0) 
    {
        printf("Could not initialize OpenMAX\n");
    }else
    {
        userData->omxState=&omx;
    }


    userData->start_flag=0;//当收到sps帧的时候，存入到队列，否则丢弃
    avpacket_queue_init(&userData->videoPacketFifo);
    
    while(1)
    {
        
        bzero(buffer, sizeof(buffer));
        //printf("recv_len=%d\n",recv_len);
        recv_len = recvfrom(sockfd, buffer, sizeof(buffer), 0, NULL,NULL);
       
        if (userData->start_flag==0 && recv_len>0) printf("recv size=%d ! but waiting for sps!\n",recv_len);
        if(recv_len>0)
        {
            rtp_packet.outbuffer=(unsigned char *)malloc(2048);//动态分配内存
            memset(rtp_packet.outbuffer,0,2048);
            if(recv_len>12)	UnpackRTPH264(buffer,recv_len,&rtp_packet);//收到的是分片，没有重新组包
            if(rtp_packet.outlen<=0)   continue;//跳过空包，在重组包时可能有问题

            //下面根据序号检查时候丢包
            if(before_seq!=0)
            {
                //printf("seq=%d before_seq=%d nal=%d\n",rtp_packet.seq, before_seq, rtp_packet.nal);
                if((rtp_packet.seq == 0) && (before_seq == 65535))
                {
                    error_packet=0;
                    before_seq=rtp_packet.seq;
                }else if(rtp_packet.seq - before_seq ==1)
                {
                    error_packet=0;
                    before_seq=rtp_packet.seq;
                }else
                {
                    printf("seq=%d before_seq=%d nal=%d\n",rtp_packet.seq, before_seq, rtp_packet.nal);
                    error_packet=1;
                    before_seq=rtp_packet.seq;
                    continue;
                }
            }
            else
            {
                before_seq=rtp_packet.seq;
            }
            //end 检查序号完毕

            //处理fu-a包，需要检查丢包情况
            if  (rtp_packet.nal == 28 ) //将网络单个包rtp_packet组合成av_packet视频帧
            {
                switch (rtp_packet.flag)
                {
                    case 0x80: // 开始包
                        av_new_packet(&av_packet, PACKET_MAX_SIZE);
                        memcpy(av_packet.data,rtp_packet.outbuffer,rtp_packet.outlen);
                        free(rtp_packet.outbuffer);
                        rtp_packet.outbuffer=NULL;
                        tmp_len=rtp_packet.outlen;
                        continue;
                    case 0x00: // 中间包
                        memcpy(av_packet.data+tmp_len,rtp_packet.outbuffer,rtp_packet.outlen);
                        free(rtp_packet.outbuffer);
                        rtp_packet.outbuffer=NULL;
                        tmp_len+=rtp_packet.outlen;
                        continue;
                    case 0x40: // 结束包
                        memcpy(av_packet.data+tmp_len,rtp_packet.outbuffer,rtp_packet.outlen);
                        free(rtp_packet.outbuffer);
                        rtp_packet.outbuffer=NULL;
                        tmp_len+=rtp_packet.outlen;
                        av_packet.size=tmp_len;
                        if(av_packet.size > PACKET_MAX_SIZE) printf("AVPACKET超出PACKET_MAX_SIZE\n");
                        //如果有错误序号的包，不存入队列
                        if (userData->start_flag && error_packet==0) avpacket_queue_put(&userData->videoPacketFifo, &av_packet);
                        break;
                }//switch
            }
            else if(rtp_packet.nal ==24 )
            {
                //printf("stap-a\n");
                av_new_packet(&av_packet, 2048);
                //注意这里直接使用recvfrom接收到的包，并且跳过前面12个字节的rtp头
                int stap_len=UnpackRtpSTAP_A_NAL(buffer+12, recv_len-12, av_packet.data);
                free(rtp_packet.outbuffer);
                rtp_packet.outbuffer=NULL;
                av_packet.size=stap_len;
                //for(int i=0;i<recv_len-12;i++) printf("%02x ",buffer[i+12]); printf("\n");

                struct get_bit_context spsbuffer;
                struct SPS sps_buffer;
                bzero(&spsbuffer, sizeof(spsbuffer));
                spsbuffer.buf = av_packet.data+5;//跳过0x00--0x01-0xx7
                spsbuffer.buf_size =av_packet.size-5;
                if(h264dec_seq_parameter_set(&spsbuffer, &sps_buffer)==0) 
                {
                    int width=h264_get_width(&sps_buffer);
                    int height=h264_get_height(&sps_buffer);
                    int framerate=h264_get_framerate(&sps_buffer);
                    if(userData->decContext->width != width) //不检测高度因为1088！!1080 || userData->decContext->height != height
                    {
                        printf("检测到分辨率发生变化 W[%d]->[%d] H[%d]->[%d]\n",userData->decContext->width, width,userData->decContext->height, height);
                        if(userData->playerState & STATE_HAVEVIDEO) userData->stop_flag = 1;//如果原来的分辨率不为0
                        userData->decContext->width=width;
                        userData->decContext->height=height;
                        printf("[sps]:width=%d,height=%d,framerate=%d\n",width,height,framerate);
                        userData->start_flag = 1;
                    }
                }
                if (userData->start_flag){
                    //printf("put stap-a size=%d\n",av_packet.size);
                    avpacket_queue_put(&userData->videoPacketFifo, &av_packet);
                    
                } 

            }

            else
            {//单个帧包
                if  (rtp_packet.nal  == 7 )
                {
                    struct get_bit_context spsbuffer;
                    struct SPS sps_buffer;
                    bzero(&spsbuffer, sizeof(spsbuffer));
                    bzero(&spsbuffer, sizeof(spsbuffer));
                    //for(int i=0;i<rtp_packet.outlen;i++) printf("%02x ",rtp_packet.outbuffer[i]); printf("\n");
                    spsbuffer.buf = rtp_packet.outbuffer+5;//跳过0x00--0x01-0xx7
                    spsbuffer.buf_size =rtp_packet.outlen-5;
                    if(h264dec_seq_parameter_set(&spsbuffer, &sps_buffer)==0) 
                    {
                        int width=h264_get_width(&sps_buffer);
                        int height=h264_get_height(&sps_buffer);
                        int framerate=h264_get_framerate(&sps_buffer);
                        if(userData->decContext->width != width) //不检测高度因为1088！!1080 || userData->decContext->height != height
                        {
                            printf("检测到分辨率发生变化 W[%d]->[%d] H[%d]->[%d]\n",userData->decContext->width, width,userData->decContext->height, height);
                            if(userData->playerState & STATE_HAVEVIDEO) userData->stop_flag = 1;//如果原来的分辨率不为0
                            userData->decContext->width=width;
                            userData->decContext->height=height;
                            printf("[sps]:width=%d,height=%d,framerate=%d\n",width,height,framerate);
                            userData->start_flag = 1;
                        }
                    }
                } 
                av_new_packet(&av_packet, rtp_packet.outlen);
                memcpy(av_packet.data,rtp_packet.outbuffer,rtp_packet.outlen);
                free(rtp_packet.outbuffer);
                rtp_packet.outbuffer=NULL;
                av_packet.size=rtp_packet.outlen;
                if (userData->start_flag) avpacket_queue_put(&userData->videoPacketFifo, &av_packet);
            }//组包结束，放入队列
        }
        if(recv_len < 0 ){
            printf("数据接收超时\n");
            userData->stop_flag=1;
        } 
        
        if(userData->stop_flag && userData->videoThreadId) //当接收到数据超时时候，关闭解码等
        {
            printf("0-准备关闭:\n");
            userData->playerState |= STATE_EXIT;
            printf("1-正在关闭解码线程\n");
            if (userData->playerState & STATE_HAVEVIDEO)   pthread_join(userData->videoThreadId, NULL);
            usleep(50*1000);
            
            printf("2-正在关闭渲染设备\n");
            omx_display_disable(userData->omxState);
            printf("3-正在关闭缓存队列\n");
            //avpacket_queue_release(&userData->videoPacketFifo);
        
            printf("4-正在关闭解码器\n");
            if(userData->playerState & STATE_HAVEVIDEO)   {
                avcodec_close(userData->decContext);
            }
            printf("5-正在重置内部参数\n");
            userData->start_flag = 0;
            userData->videoThreadId=NULL;
            userData->decContext->width=0;
            userData->decContext->height=0;
            userData->stop_flag = 0;
            before_seq=0;
            printf("6-已经全部关闭\n");
        }
  
        //把初始化omx放到这里----当开始的时候：打开解码--初始化omxrender---打开解码线程
        if(userData->videoThreadId==NULL && userData->start_flag)
        {
            printf("0-接收数据开始:\n");

            printf("1-正在初始化接收队列:\n");
            //avpacket_queue_init(&userData->videoPacketFifo);

            printf("2-正在打开解码器:\n");
            if(avcodec_open2(userData->decContext, videoCodec, NULL)<0)
            {
                printf("Could not open video codec\n");
            }else{
                printf("Video resolution: %dx%d\n", userData->decContext->width, userData->decContext->height);
                userData->playerState |= STATE_HAVEVIDEO;
                userData->playerState &= ~STATE_EXIT;
            }

            printf("3-正在打开渲染设备:\n");

            if (omx_display_enable(userData->omxState,	userData->decContext->width, userData->decContext->height, userData->decContext->width) != 0)
            {
                printf("Could not open OpenMAX\n");
            }

            printf("4-正在打开解码线程:\n");
            if (userData->playerState & STATE_HAVEVIDEO)  pthread_create(&userData->videoThreadId, NULL, &handleVideoThread, userData);
            
            printf("5-全部打开完毕:\n");
        }
    
    }//while end

omx_deinit(userData->omxState);  
bcm_host_deinit();
userData->omxState=NULL;


}






