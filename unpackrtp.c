#include <stdio.h>  
#include <stdlib.h> 
#include <math.h>
#include "unpackrtp.h"
unsigned  char timestamp_no[4];//timestamp
unsigned  char seq_no[2]; //序号
int  UnpackRTPH264(unsigned char *bufIn,int len,struct packet *avpacket)
{
int outlen=0;
     if  (len  <  RTP_HEADLEN)         return  -1 ;
    
     //rtp头解析
	memcpy(timestamp_no,(unsigned  char * )bufIn+4,4);
	//avpacket->timestamp=(int64_t)((timestamp_no[0]<<24)+((timestamp_no[1]<<16)+(timestamp_no[2]<<8)))+timestamp_no[3];//有错误，出现负数
	avpacket->timestamp=(int64_t)(timestamp_no[0]*pow(2,24)+timestamp_no[1]*pow(2,16)+timestamp_no[2]*pow(2,8)+timestamp_no[3]);
	memcpy(seq_no,(unsigned  char * )bufIn+2,2);
	avpacket->seq=(seq_no[0]<<8)+seq_no[1];

    //h264playload解析
    unsigned  char *src  =  (unsigned  char * )bufIn  +  RTP_HEADLEN;
    unsigned  char  head1  =   * src; // 获取第一个字节
    unsigned  char  nal  =  head1  &   0x1f ; // 获取FU indicator的类型域，
    unsigned  char  head2  =   * (src + 1 ); // 获取第二个字节
    unsigned  char  flag  =  head2  &   0xe0 ; // 获取FU header的前三位，判断当前是分包的开始、中间或结束
    unsigned  char  nal_fua  =  (head1  &   0xe0 )  |  (head2  &   0x1f ); // FU_A nal
	
	avpacket->nal=nal;//输出nal

    if  (nal == 0x1c ){
        if  (flag == 0x80 ) // 开始
        {
            avpacket->outbuffer[0]=0x0;
            avpacket->outbuffer[1]=0x0;
            avpacket->outbuffer[2]=0x0;
            avpacket->outbuffer[3]=0x1;
            avpacket->outbuffer[4]=nal_fua;
            outlen  = len - RTP_HEADLEN -2+5;//-2跳过前2个字节，+5前面前导码和类型码
            memcpy(avpacket->outbuffer+5,src+2,outlen);
            avpacket->outlen=outlen;
            avpacket->flag=flag;//输出flag
            //printf("start:bufout[end]=%x %x %x %x,src[end]=%x\n",bufout[outlen-4],bufout[outlen-3],bufout[outlen-2],bufout[outlen-1],src[len-RTP_HEADLEN-1]);
        }
        else   if (flag == 0x40 ) // 结束
         {
            outlen  = len - RTP_HEADLEN -2 ;
            memcpy(avpacket->outbuffer,src+2,len-RTP_HEADLEN-2);
            avpacket->outlen=outlen;
            avpacket->flag=flag;//输出flag
            //printf("end:bufout[end]=%x %x %x %x,src[end]=%x\n",bufout[outlen-4],bufout[outlen-3],bufout[outlen-2],bufout[outlen-1],src[len-RTP_HEADLEN-1]);
       }
        else // 中间
         {
            outlen  = len - RTP_HEADLEN -2 ;
            memcpy(avpacket->outbuffer,src+2,len-RTP_HEADLEN-2);
            avpacket->outlen=outlen;
            avpacket->flag=flag;//输出flag
            //printf("center:bufout[end]=%x %x %x %x,src[end]=%x\n",bufout[outlen-4],bufout[outlen-3],bufout[outlen-2],bufout[outlen-1],src[len-RTP_HEADLEN-1]);
        }
    }
    else  if  (nal == 0x07 )
    {//sps
        //printf("sps");
        avpacket->outbuffer[0]=0x0;
        avpacket->outbuffer[1]=0x0;
        avpacket->outbuffer[2]=0x0;
        avpacket->outbuffer[3]=0x1;
        memcpy(avpacket->outbuffer+4,src,len-RTP_HEADLEN);
        outlen=len-RTP_HEADLEN+4;
        avpacket->outlen=outlen;
	}
    else {//当个包，1,7,8
        //printf("*****");
        avpacket->outbuffer[0]=0x0;
        avpacket->outbuffer[1]=0x0;
        avpacket->outbuffer[2]=0x0;
        avpacket->outbuffer[3]=0x1;
        memcpy(avpacket->outbuffer+4,src,len-RTP_HEADLEN);
        outlen=len-RTP_HEADLEN+4;
        avpacket->outlen=outlen;
        //printf("singe:bufout[3]=%x %x %x %x,src[0]=%x\n",bufout[3],bufout[4],bufout[5],bufout[6],src[0]);
    }
    return  0;
}


static char h264prefix[4] = {0x00, 0x00, 0x00, 0x01};
int UnpackRtpSTAP_A_NAL(unsigned char *buf, int size, unsigned char *framebuf)
{
    int len = sizeof(h264prefix);
    unsigned char *pos = framebuf;
    unsigned char *q = NULL;
    unsigned int nalu_size;
    int paysize = size;

    q = buf+1;
    int nidx = 0;
    int counter=0;
    while (nidx < (paysize-1)) {
        memcpy((void *)pos, (const void*)(&h264prefix), len);
        pos += len;
        /* get NALU size */
        nalu_size = ((unsigned char)q[nidx]<<8)|((unsigned char)q[nidx+1]);
        nidx += 2;
        if (nalu_size == 0) {
            nidx++;
            continue;
        }
        memcpy((void *)pos, (const void*)(q+nidx), nalu_size);
        nidx += nalu_size;
        pos  += nalu_size;
        counter++;
    }

    return (pos-framebuf);
}

