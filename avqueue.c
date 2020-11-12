#include "avqueue.h"


void avpacket_queue_init(AVPacketQueue *q)
{
    memset(q, 0, sizeof(AVPacketQueue));
    pthread_mutex_init(&q->mutex, NULL);
    //pthread_cond_init(&q->cond, NULL);
}


static void avpacket_queue_flush(AVPacketQueue *q)
{
    AVPacketList *pkt, *pkt1;

    pthread_mutex_lock(&q->mutex);

    for (pkt = q->first_pkt; pkt != NULL; pkt = pkt1)
    {
        pkt1 = pkt->next;
        av_free_packet(&pkt->pkt);
        av_freep(&pkt);
    }

    q->last_pkt   = NULL;
    q->first_pkt  = NULL;
    q->nb_packets = 0;
    q->size       = 0;
    pthread_mutex_unlock(&q->mutex);
}


void avpacket_queue_release(AVPacketQueue *q)
{
    avpacket_queue_flush(q);
    pthread_mutex_destroy(&q->mutex);
    //pthread_cond_destroy(&q->cond);
}


unsigned int avpacket_queue_size(AVPacketQueue *q)
{
    unsigned int size;
    pthread_mutex_lock(&q->mutex);
    size = q->size;
    pthread_mutex_unlock(&q->mutex);

    return size;
}


int avpacket_queue_put(AVPacketQueue *q, AVPacket *pkt)
{
    AVPacketList *pkt1;

    // Drop Packet if queue size is > 10 MB
    if (avpacket_queue_size(q) >  1024 * 1024 * 10)
    {
        fprintf(stderr,"%s() - Error: input buffer overrun\n", __func__);
        return -1;
    }

    // duplicate the packet
    if (av_dup_packet(pkt) < 0)
    {
        return -1;
    }

    if ((pkt1 = (AVPacketList *)av_malloc(sizeof(AVPacketList))) == NULL)
        return -1;

    pkt1->pkt  = *pkt;
    pkt1->next = NULL;

    pthread_mutex_lock(&q->mutex);

    if (!q->last_pkt)
       q->first_pkt = pkt1;
    else
       q->last_pkt->next = pkt1;

    q->last_pkt = pkt1;
    q->nb_packets++;
    q->size += pkt1->pkt.size + sizeof(*pkt1);

    //pthread_cond_signal(&q->cond);
    pthread_mutex_unlock(&q->mutex);

    return 0;
}


int avpacket_queue_get(AVPacketQueue *q, AVPacket *pkt, int block)
{
    AVPacketList *pkt1;
    int ret;

    pthread_mutex_lock(&q->mutex);

    while(1)
    {
        pkt1 = q->first_pkt;
        if (pkt1)
        {
            q->first_pkt = pkt1->next;
            if (!q->first_pkt)
                q->last_pkt = NULL;

            q->nb_packets--;
            q->size -= pkt1->pkt.size + sizeof(*pkt1);
            *pkt     = pkt1->pkt;
            av_free(pkt1);
            ret = 1;
            break;
        }
        else if (!block)
        {
            ret = 0;
            break;
        }
        else
        {
            //pthread_cond_wait(&q->cond, &q->mutex);
            ret = -1;
            break;
        }
    }

    pthread_mutex_unlock(&q->mutex);

    return ret;
}
