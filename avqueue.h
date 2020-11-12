#ifndef _AVQUEUE_
#define _AVQUEUE_

#include <pthread.h>
#include <semaphore.h>
#include "libavformat/avformat.h"

typedef struct
{
        AVPacketList *first_pkt, *last_pkt;
        int nb_packets;
        unsigned int size;
        pthread_mutex_t mutex;
        //pthread_cond_t cond;
} AVPacketQueue;


void avpacket_queue_init(AVPacketQueue *q);
void avpacket_queue_release(AVPacketQueue *q);
unsigned int avpacket_queue_size(AVPacketQueue *q);
int avpacket_queue_put(AVPacketQueue *q, AVPacket *pkt);
int avpacket_queue_get(AVPacketQueue *q, AVPacket *pkt, int block);

#endif
