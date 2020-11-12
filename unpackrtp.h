#include <stdio.h>  
#include <stdlib.h> 
#include <string.h>
#include "sps_pps.h"
#define  RTP_HEADLEN 12
typedef struct
{
    /** byte 0 */
    unsigned char csrc_len:4;        /** expect 0 */
    unsigned char extension:1;       /** expect 1, see RTP_OP below */
    unsigned char padding:1;         /** expect 0 */
    unsigned char version:2;         /** expect 2 */
    /** byte 1 */
    unsigned char payload:7;         /** stream type */
    unsigned char marker:1;          /** when send the first framer,set it */
    /** bytes 2, 3 */
    unsigned short seq_no;
    /** bytes 4-7 */
    unsigned  long timestamp;
    /** bytes 8-11 */
    unsigned long ssrc;              /** stream number is used here. */
} RTP_FIXED_HEADER;

typedef struct packet  
{  
	unsigned char *outbuffer;
	int outlen;  
	//int64_t pts;
	//int64_t dts;
	int payload;
	unsigned  char  nal;
	unsigned char flag;
	unsigned  long  seq;
	int64_t timestamp;
	unsigned  long  ssrc;
}packet;  

int UnpackRTPH264(unsigned char *bufIn,int len,struct packet *avpacket);
int UnpackRtpSTAP_A_NAL(unsigned char *buf, int size, unsigned char *framebuf);