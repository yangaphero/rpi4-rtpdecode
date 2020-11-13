#include "appdata.h"
/*
//下面是测试yuv生产
#define WIDTH     640
#define HEIGHT    480
#define STRIDE    640
static int generate_test_card(void *buf, uint32_t* filledLen, int frame)
{
   int i, j;
   char *y = buf;
   char *u = y + STRIDE * HEIGHT;
   char *v = u + (STRIDE >> 1) * (HEIGHT >> 1);

   for (j = 0; j < HEIGHT / 2; j++) {
      char *py = y + 2 * j * STRIDE;
      char *pu = u + j * (STRIDE>> 1);
      char *pv = v + j * (STRIDE>> 1);
      for (i = 0; i < WIDTH / 2; i++) {
         int z = (((i + frame) >> 4) ^ ((j + frame) >> 4)) & 15;
         py[0] = py[1] = py[STRIDE] = py[STRIDE + 1] = 0x80 + z * 0x8;
         if(i < 4 || i > (WIDTH / 2 - 20)) py[0] = py[1] = py[STRIDE] = py[STRIDE + 1] = 0x0f; // mark horizontal border
         if(j  < 4 || j > (HEIGHT / 2 - 40)) py[0] |= py[1] |= py[STRIDE] |= py[STRIDE + 1] |= 0xf0; // mark vertical border
         pu[0] = 0x00 + z * 0x10;
         pv[0] = 0x80 + z * 0x30;
         py += 2;
         pu++;
         pv++;

      }
   }
   *filledLen = (STRIDE* HEIGHT * 3) >> 1;
   return 1;
}
*/

double difftimeval(const struct timeval *start, const struct timeval *end)
{
        double d;
        time_t s;
        suseconds_t u;
        s = start->tv_sec - end->tv_sec;
        u = start->tv_usec - end->tv_usec;
        d = s;
        d *= 1000000.0;//1 秒 = 10^6 微秒
        d += u;

        return d/1000; //返回毫秒
}



void* handleVideoThread(void *params)
{
    appData *userData = (appData*)params;
    AVCodecContext *pCodecCtx = userData->decContext;
    AVFrame *pFrame;
    AVPacket pkt;
    int frameFinished;
    unsigned int  frame_counter=0;
    static int first_packet = 1;
    int ret;
    void* pbuf; //omx render的视频显示缓存地址
    uint32_t buf_len;//omx render的视频显示缓存长度
    struct timeval start,end;

    fprintf(stderr, "%s() - Info: video decoding thread started...\n", __FUNCTION__);
    gettimeofday(&start, NULL);//获取开始时间
    if ((pFrame = av_frame_alloc()) == NULL)
    {
        fprintf(stderr, "%s() - Error: failed to allocate video frame\n", __FUNCTION__);
        return (void*)1;
    }

    
    while (1)
    {
        if (avpacket_queue_get(&userData->videoPacketFifo, &pkt, 1) == 1)
        {
  
            //printf("get pkt.size=%d\n",pkt.size);
            //printf("queue.size=%d\n",avpacket_queue_size(&userData->videoPacketFifo));
            gettimeofday(&end, NULL);  //获取结束时间
            ret = avcodec_send_packet(pCodecCtx, &pkt);
            if (ret < 0) {
                //printf("avcodec: avcodec_send_packet error,"" packet=%zu bytes, ret=%d (%s)\n", pkt.size, ret, av_err2str(ret));
                av_free_packet(&pkt);
                continue;
            }
            ret = avcodec_receive_frame(pCodecCtx, pFrame);
            if (ret == AVERROR(EAGAIN)) {
                printf("EAGAIN packet=%zu bytes\n",pkt.size);
                continue;
            }
            else if (ret < 0) {
                printf("avcodec_receive_frame error ret=%d\n", ret);
                av_free_packet(&pkt);
                continue;
            }
            frameFinished = 1;
            //下面是测试帧速率：
            frame_counter++;
            if(frame_counter % 25==0)  printf("frames=[%d] time=[%.0f] fps=[%.02f] queue=[%d]\n", frame_counter,difftimeval(&end, &start),frame_counter*1000/difftimeval(&end, &start),avpacket_queue_size(&userData->videoPacketFifo));
            
            if (first_packet)
            {
                
                fprintf(stderr, "%s() - Info: video parameters dump:\n", __FUNCTION__);
                fprintf(stderr, "\tY  component address %p pitch %d\n", (void*)pFrame->data[0], pFrame->linesize[0]);
                fprintf(stderr, "\tU component address %p pitch %d\n", (void*)pFrame->data[1], pFrame->linesize[1]);
                fprintf(stderr, "\tV component address %p pitch %d\n", (void*)pFrame->data[2], pFrame->linesize[2]);
                fprintf(stderr, "\tAligned video size: %dx%d\n", pFrame->linesize[0], ALIGN_UP(userData->decContext->height,16));
            }

            //下面 开始进行显示，
            //第1步-首先从omx render中取出yuv视频的缓冲地址pbuf和大小buf_len
            if(omx_display_input_buffer(userData->omxState, &pbuf, &buf_len)!=0){
                printf("omx_display_input_buffer failed\n"); 
            }

            //第2步-复制pFrame->data复制到pbuf地址空间，这里注意pFrame->pFrame->data[0] data[1] data[2]对应Y U V分量
            int renderFrameStride   = ALIGN_UP(userData->decContext->width, 32);
            uint8_t* frameDataPtr   = pFrame->data[0];
            int libavFrameStride    = pFrame->linesize[0];
            int frameWidth          = userData->decContext->width;
            int frameHeight         = ALIGN_UP(userData->decContext->height, 16);
            int row;
      
            for(row=0; row<frameHeight; row++)  // insert Y component into omx buffer
            {
                memcpy(pbuf, frameDataPtr, frameWidth);
                pbuf += renderFrameStride;
                frameDataPtr  += libavFrameStride;
            }

            frameDataPtr = pFrame->data[1];
            libavFrameStride = pFrame->linesize[1];
            for(row=0; row<frameHeight/2; row++)  // insert U component into omx buffer
            {
                memcpy(pbuf, frameDataPtr, frameWidth/2);
                pbuf += renderFrameStride/2;
                frameDataPtr  += libavFrameStride;
            }

            frameDataPtr = pFrame->data[2];
            libavFrameStride = pFrame->linesize[2];
            for(row=0; row<frameHeight/2; row++)  // insert V component into omx buffer
            {
                memcpy(pbuf, frameDataPtr, frameWidth/2);
                pbuf += renderFrameStride/2;
                frameDataPtr  += libavFrameStride;
            }
            //第3步-开始消耗（flush）掉视频缓存，也就是显示
            if (omx_display_flush_buffer(userData->omxState, buf_len)!=0){
                printf("omx_display_flush_buffer failed\n"); 
            }
            if(first_packet)   first_packet = 0; 
            // Free video packet
            av_free_packet(&pkt);
        }
        else
        {
            usleep(1000*20);
        }
        if (userData->playerState & STATE_EXIT)  // videoThread user_exit 判断是否退出线程
        {
            
            ret =avcodec_send_packet(pCodecCtx, NULL);//冲刷解码器内部缓存
            if (ret < 0) {
                printf("avcodec: avcodec_send_packet error\n");
            }
            while(ret>=0){
                printf("flush frame\n");
                ret = avcodec_receive_frame(pCodecCtx, pFrame);
                if (ret == AVERROR_EOF){
                    avcodec_flush_buffers(pCodecCtx);
                    printf("avcodec_flush_buffers!\n");
                }
            }
            fprintf(stderr, "%s() - Info: STATE_EXIT flag has been set\n", __FUNCTION__);
            break;//退出解码循环
        }

    }
    
  

    // Free the YUV frame
    av_free(pFrame);

    userData->playerState &= ~STATE_HAVEVIDEO;
    fprintf(stderr, "%s() - Info: video decoding thread finished\n", __FUNCTION__);

    return 0;
}
