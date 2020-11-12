
/*
//--------解码输入参数的sps pps字符串--------
char *base_sps="Z2QAH6zZQFAFuwEQAAADABAAAAMDIPGDGWA=";
char *base_pps="aOvjyyLA";

 //if(base_sps && base_pps) userData->start_flag =1;//临时添加

if(base_sps && base_pps){
	unsigned char startcode[4]={0x00,0x00,0x00,0x01};
	
	char sps[200] ;
	int spsnum= base64_decode(base_sps, strlen(base_sps),sps);  
	unsigned char *spsoutbuffer= (unsigned char *)malloc(spsnum+4);//动态分配内存
	memcpy(spsoutbuffer,startcode,4);
	memcpy(spsoutbuffer+4,sps,spsnum);
	av_new_packet(&av_packet, spsnum+4);
    memcpy(av_packet.data,spsoutbuffer,spsnum+4);
    //avpacket_queue_put(&userData->videoPacketFifo, &av_packet);
    free(spsoutbuffer);

	char pps[50];
	int ppsnum=base64_decode(base_pps, strlen(base_pps),pps);
	unsigned char *ppsoutbuffer= (unsigned char *)malloc(ppsnum+4);//动态分配内存
	memcpy(ppsoutbuffer,startcode,4);
	memcpy(ppsoutbuffer+4,pps,ppsnum);
    av_new_packet(&av_packet, ppsnum+4);
    memcpy(av_packet.data,ppsoutbuffer,ppsnum+4);
    //avpacket_queue_put(&userData->videoPacketFifo, &av_packet);
    free(ppsoutbuffer);
}
*/

/*
bool H264Parser::FindStartCode(const uint8_t* data, size_t data_size, size_t* offset, size_t* start_code_size)
{
  size_t bytes_left = data_size;

  while (bytes_left >= 3) {
    // The start code is "\0\0\1", ones are more unusual than zeroes, so let's
    // search for it first.
    const uint8_t* tmp =  reinterpret_cast<const uint8_t*>(memchr(data + 2, 1, bytes_left - 2));
    if (!tmp) {
      data += bytes_left - 2;
      bytes_left = 2;
      break;
    }
    tmp -= 2;
    bytes_left -= tmp - data;
    data = tmp;

    if (IsStartCode(data)) {
      // Found three-byte start code, set pointer at its beginning.
      *offset = data_size - bytes_left;
      *start_code_size = 3;

      // If there is a zero byte before this start code,
      // then it's actually a four-byte start code, so backtrack one byte.
      if (*offset > 0 && *(data - 1) == 0x00) {
        --(*offset);
        ++(*start_code_size);
      }

      return true;
    }

    ++data;
    --bytes_left;
  }
  */