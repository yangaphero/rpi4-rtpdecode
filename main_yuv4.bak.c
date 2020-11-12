#include <stdint.h>
#include <time.h>
#include <sys/time.h>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>

#include <libavutil/opt.h>

#include "bcm_host.h"
#include "omx_audio.h"
#include "omx_integration.h"

#include "avqueue.h"
#include "cpuload.h"
#include "aud_decode.h"
#include "vid_decode.h"
#include "control.h"
#include "appdata.h"


#define	AV_CODEC_FLAG2_FAST   		(1 << 0)
#define	AV_CODEC_FLAG_LOW_DELAY		(1 << 19)
#define	AV_CODEC_FLAG_INTERLACED_DCT	(1 << 18)
#define	AV_CODEC_FLAG_INTERLACED_ME	(1 << 29)
#define	AV_CODEC_FLAG_GRAY		(1 << 13)

#ifdef ALIGN_UP
    #undef ALIGN_UP
#endif

#define ALIGN_UP(x,y)  ((x + (y)-1) & ~((y)-1))


int parse_args(int argc, char** argv, appData* userData);

int main(int argc, char **argv)
{
    AVFormatContext *pFormatCtx = NULL;
    int i, videoStream, audioStream;
    AVCodec *videoCodec = NULL;
    AVCodec *audioCodec = NULL;
    AVPacket packet;

    AVDictionary *optionsDict = NULL;

    appData *userData = (appData*) malloc(sizeof(appData));
    memset(userData, 0, sizeof(appData));

    if (parse_args(argc, argv, userData) != 0)
    {
        //fprintf(stderr, "%s() - Error: parse_args() failed with retval %d\n", __func__, ret);
        return 1;
    }

//DEMUXER INIT
    avformat_network_init();

    // Register all formats and codecs
    av_register_all();

    // Open video file
    if (avformat_open_input(&pFormatCtx, userData->fileName, NULL, NULL) != 0)
    {
        printf("Error: can't open: %s\n", userData->fileName);
	return 1;
    }

    // Retrieve stream information
    if (avformat_find_stream_info(pFormatCtx, NULL) < 0)
    {
        printf("Error: can't find stream information\n");
	return 1;
    }

    // Dump information about file onto standard error
    av_dump_format(pFormatCtx, 0, userData->fileName, 0);
//DEMUXER INIT

// Find the first video stream
    videoStream=-1;
    for (i=0; i < pFormatCtx->nb_streams; i++)
	if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
        {
	    videoStream=i;
            printf("Video stream index: %d\n",videoStream);
	    break;
	}

    if (videoStream == -1)
    {
        //printf("Can't find video stream\n");
    }
    else
    {
        userData->videoStream = pFormatCtx->streams[videoStream];
        userData->videoStream->codec->idct_algo = FF_IDCT_SIMPLEARMV6;  // FF_IDCT_SIMPLEARMV6 || FF_IDCT_SIMPLENEON
/*
FF_IDCT_INT            //12.1
FF_IDCT_SIMPLE         //11.9
FF_IDCT_ARM            //11.8
FF_IDCT_SIMPLEARM      //12.1
FF_IDCT_SIMPLEARMV5TE  //11.1
FF_IDCT_SIMPLEARMV6    //10.4
FF_IDCT_SIMPLENEON     //9.0
*/
        //userData->videoStream->codec->flags |= (AV_CODEC_FLAG_INTERLACED_DCT | AV_CODEC_FLAG_INTERLACED_ME | AV_CODEC_FLAG_GRAY);
        //userData->videoStream->codec->flags |= (AV_CODEC_FLAG_LOW_DELAY); |// AV_CODEC_FLAG_INTERLACED_DCT); //| AV_CODEC_FLAG_INTERLACED_ME);  // AV_CODEC_FLAG_GRAY 

        // Find the decoder for the video stream
        //videoCodec = avcodec_find_decoder(userData->videoStream->codec->codec_id);
	videoCodec = avcodec_find_decoder_by_name("h264_mmal");

        if (videoCodec == NULL)
        {
	    fprintf(stderr, "Unsupported video codec!\n");
        }
        else
        {
            printf("Video codec: %s\n", videoCodec->name);

            // Open video codec
            if(avcodec_open2(userData->videoStream->codec, videoCodec, &optionsDict)<0)
            {
                printf("Could not open video codec\n");
            }
            else
            {
                printf("Video resolution: %dx%d\n", userData->videoStream->codec->width, userData->videoStream->codec->height);
                userData->playerState |= STATE_HAVEVIDEO;
            }
        }
    }
// Find the first video stream

// Find the first audio stream
    audioStream=-1;
    for (i=0; i < pFormatCtx->nb_streams; i++)
	if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO)
        {
	    audioStream=i;
            printf("Audio stream index: %d\n",audioStream);
	    break;
	}

    if (audioStream==-1)
    {
        //printf("Can't find audio stream\n");
    }
    else
    {
        userData->audioStream = pFormatCtx->streams[audioStream];
        //userData->audioStream->codec->flags2 |= AV_CODEC_FLAG2_FAST;  //nema vplyv

        userData->audioStream->codec->idct_algo = FF_IDCT_SIMPLEARMV6;  // FF_IDCT_SIMPLEARMV6 || FF_IDCT_SIMPLENEON

        audioCodec = avcodec_find_decoder(userData->audioStream->codec->codec_id);

        if (audioCodec==NULL)
        {
	    fprintf(stderr, "Unsupported audio codec!\n");
        }
        else
        {
            printf("Audio codec: %s\n", audioCodec->name);

            // Open codec
            if (avcodec_open2(userData->audioStream->codec, audioCodec, NULL) < 0)
            {
                printf("Could not open audio codec\n");
            }
            else
            {
                userData->swr = avresample_alloc_context();
                av_opt_set_int(userData->swr, "in_channel_layout",  av_get_default_channel_layout(userData->audioStream->codec->channels), 0);
                av_opt_set_int(userData->swr, "out_channel_layout", AV_CH_LAYOUT_STEREO, 0);
                av_opt_set_int(userData->swr, "in_sample_rate",     userData->audioStream->codec->sample_rate, 0);
                av_opt_set_int(userData->swr, "out_sample_rate",    userData->audioStream->codec->sample_rate, 0);
                av_opt_set_int(userData->swr, "in_sample_fmt",      userData->audioStream->codec->sample_fmt, 0);
                av_opt_set_int(userData->swr, "out_sample_fmt",     AV_SAMPLE_FMT_S16, 0);
                avresample_open(userData->swr);

                userData->playerState |= STATE_HAVEAUDIO;
            }
        }
    }
// Find the first audio stream

    if (!(userData->playerState & STATE_HAVEAUDIO || userData->playerState & STATE_HAVEVIDEO))
    {
        fprintf(stderr, "%s() - Error: no audio nor video stream found\n", __FUNCTION__);

        // Close the video file
        avformat_close_input(&pFormatCtx);

        return 1;
    }

    bcm_host_init();

    // resamplujem na stereo, 16-bit
    int nchannels = 2;
    int bitdepth = 16;
    int buffer_size = (BUFFER_SIZE_SAMPLES * bitdepth * OUT_CHANNELS(nchannels))>>3;

    int ret = omxInit(&userData->omxState,
              userData->playerState & STATE_HAVEVIDEO ? userData->videoStream->codec->width : 32, // width
              userData->playerState & STATE_HAVEVIDEO ? userData->videoStream->codec->height: 16, // height
              12, (userData->playerState & STATE_FILTERTYPE_MASK)>>STATE_FILTERTYPE_SHIFT, // numBuff, useFilter [image_fx]
              0, 0, 1024, 768, 0, // render canvas: x_off, y_off, width, height, disp_num [video_render]
              userData->playerState & STATE_HAVEAUDIO ? userData->audioStream->codec->sample_rate : 8000,
              nchannels, bitdepth, 12, buffer_size);  // ,,,num_buffers, [audio_render]

    if (ret)
    {
        fprintf(stderr, "%s() - Error: omxInit() failed with error %d\n", __FUNCTION__, ret);

        // Close the codec
        if (userData->playerState & STATE_HAVEVIDEO)
            avcodec_close(userData->videoStream->codec);

        if (userData->playerState & STATE_HAVEAUDIO)
            avcodec_close(userData->audioStream->codec);

        // Close the video file
        avformat_close_input(&pFormatCtx);

        return 1;
    }

    startCpuLoadDetectionThread();

    avpacket_queue_init(&userData->audioPacketFifo);
    avpacket_queue_init(&userData->videoPacketFifo);

    if (userData->playerState & STATE_HAVEAUDIO)
        pthread_create(&userData->audioThreadId, NULL, &handleAudioThread, userData);

    if (userData->playerState & STATE_HAVEVIDEO)
        pthread_create(&userData->videoThreadId, NULL, &handleVideoThread, userData);

    keyboardInit();

    while (av_read_frame(pFormatCtx, &packet) >= 0)
    {
	if (packet.stream_index == videoStream)
        {
            if (avpacket_queue_put(&userData->videoPacketFifo, &packet) != 0)
                goto terminatePlayer;
	}
        else if (packet.stream_index == audioStream)
        {
            if(avpacket_queue_put(&userData->audioPacketFifo, &packet) != 0)
                goto terminatePlayer;
	}
        else  // some other packet type
        {
            // Free the packet that was allocated by av_read_frame
	    av_free_packet(&packet);
        }

        while (avpacket_queue_size(&userData->videoPacketFifo) > INPUT_QUEUE_SIZE || avpacket_queue_size(&userData->audioPacketFifo) > INPUT_QUEUE_SIZE/2)  // wait if input queue full
        {
            if(checkKeyPress(userData))
                goto terminatePlayer;

             usleep(1000*100);
        }

        if(checkKeyPress(userData))
            goto terminatePlayer;
    }

    printf("Got EOS: V/A queue %u/%u, CPU %5.2f%%\n", avpacket_queue_size(&userData->videoPacketFifo),
            avpacket_queue_size(&userData->audioPacketFifo), getCpuLoad());

    // if EOS wait for threads to finish
    while (avpacket_queue_size(&userData->videoPacketFifo) > 0 || avpacket_queue_size(&userData->audioPacketFifo) > 0)
    {
        usleep(1000*100); // 100ms

        if(checkKeyPress(userData))
            goto terminatePlayer;
    }

    userData->playerState |= STATE_EXIT;

    printf("V/A queue %u/%u, CPU %5.2f%%\n", avpacket_queue_size(&userData->videoPacketFifo),
            avpacket_queue_size(&userData->audioPacketFifo), getCpuLoad());

    printf("exitSignal has been sent to decoding & rendering threads...\n");fflush(stdout);

    terminatePlayer:

    // switch keyboard to normal operation
    keyboardDeinit();

    stopCpuLoadDetectionThread();

    if (userData->playerState & STATE_HAVEAUDIO)
    {
        pthread_join(userData->audioThreadId, NULL);
        usleep(500*1000);
    }

    if (userData->playerState & STATE_HAVEVIDEO)
        pthread_join(userData->videoThreadId, NULL);

    // Deinit OMX components
    omxDeinit(userData->omxState);

    // Release fifo
    avpacket_queue_release(&userData->videoPacketFifo);
    avpacket_queue_release(&userData->audioPacketFifo);

    // Close the codec
    if (userData->playerState & STATE_HAVEAUDIO)
        avcodec_close(userData->audioStream->codec);

    if (userData->playerState & STATE_HAVEVIDEO)
        avcodec_close(userData->videoStream->codec);

    // Close the video file
    avformat_close_input(&pFormatCtx);

    bcm_host_deinit();

    fprintf(stderr, "%s() - Info: player finished\n", __FUNCTION__);

    return 0;
}



int parse_args(int argc, char** argv, appData* userData)
{
    int ret;

    while (1)
    {
        static struct option long_options[] =
        {
            {"help",                             no_argument,       0, 'h'},
            {"deinterlace",                      required_argument, 0, 'd'},
            {0, 0, 0, 0}
        };

        /* getopt_long stores the option index here. */
        int option_index = 0;

        if (argc == 1)  // no argument given
        {
            ret = 'h';
        }
        else
        {
            ret = getopt_long (argc, argv, "", long_options, &option_index);

            /* Detect the end of the options. */
            if (ret == -1)
                break;
        }


        switch (ret)
        {
            case 'h':
                print_help:
                fprintf (stderr, "%s [options] in_file_or_stream\n", argv[0]);
                fprintf(stderr, "\t--help\n");
                fprintf(stderr, "\t--deinterlace \e[4mtype\e[0m \n");
                fprintf(stderr, "\t\t0 - none\n");
                fprintf(stderr, "\t\t1 - line double\n");
                fprintf(stderr, "\t\t2 - advanced\n");
                fprintf(stderr, "\t\t3 - fast\n");

                return 1;

            case 'd':
                ret = atoi(optarg);
                if (ret < 0 || ret > 15)
                {
                    fprintf(stderr, "%s() - Error: --%s supplied with invalid value(%d)\n", __func__, long_options[option_index].name, ret);
                    return 1;
                }

                userData->playerState = (ret<<STATE_FILTERTYPE_SHIFT) & STATE_FILTERTYPE_MASK;

                break;

            case '?':
                /* getopt_long already printed an error message. */
                return 1;
                break;

            default:
                fprintf (stderr, "%s() - Error: getopt returned unexpected character code %d('%c')\n", __func__, ret, ret);
                return 1;
                //abort ();
        }
    }

    /* Print any remaining command line arguments (not options). */
/*
    if (optind < argc)
    {
        fprintf (stderr, "non-option ARGV-elements: ");
        while (optind < argc)
            fprintf (stderr, "%s ", argv[optind++]);
        putchar ('\n');
        return 1;
    }
*/
    if (optind < argc)  // first remaining command line argument is fileName
    {
        userData->fileName = argv[optind];
    }
    else
    {
        goto print_help;
    }

    return 0;
}

