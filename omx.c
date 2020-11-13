/**
 *
 * ToxBlinkenwall
 * (C)Zoff <zoff@zoff.cc> in 2017 - 2019
 *
 * https://github.com/zoff99/ToxBlinkenwall
 *
 */
/**
 * @file omx.c     Raspberry Pi VideoCoreIV OpenMAX interface
 *
 * Copyright (C) 2016 - 2017 Creytiv.com
 * Copyright (C) 2016 - 2017 Jonathan Sieber
 * Copyright (C) 2018 - 2019 Zoff <zoff@zoff.cc>
 */


#include "omx.h"

/*
 * forward func declarations (acutal functions in toxblinkenwall.c)
 */

void usleep_usec(uint64_t usec)
{
    struct timespec ts;
    ts.tv_sec = usec / 1000000;
    ts.tv_nsec = (usec % 1000000) * 1000;
    nanosleep(&ts, NULL);
}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Avoids a VideoCore header warning about clock_gettime() */
#include <time.h>
#include <sys/time.h>

#include <bcm_host.h>

/**
 * @defgroup omx omx
 *
 * TODO:
 *  * Proper sync OMX events across threads, instead of busy waiting
 */

static const int VIDEO_RENDER_PORT = 90;

static OMX_ERRORTYPE EventHandler(OMX_HANDLETYPE hComponent, OMX_PTR pAppData,  OMX_EVENTTYPE eEvent, OMX_U32 nData1, OMX_U32 nData2, OMX_PTR pEventData)
{
    (void) hComponent;

    switch (eEvent)
    {
        case OMX_EventCmdComplete:
            printf("omx.EventHandler: Previous command completed\n d1=%x\td2=%x\teventData=%p\tappdata=%p\n", nData1, nData2, pEventData, pAppData);
            break;
        case OMX_EventError:
            printf("omx.EventHandler: Error event type " "data1=%x\tdata2=%x\n", nData1, nData2);
            break;
        default:
            printf( "omx.EventHandler: Unknown event type %d\t" "data1=%x data2=%x\n", eEvent, nData1, nData2);
            return -1;
            break;
    }
    return 0;
}


static OMX_ERRORTYPE EmptyBufferDone(OMX_HANDLETYPE hComponent, OMX_PTR pAppData, OMX_BUFFERHEADERTYPE *pBuffer)
{
    (void) hComponent;
    (void) pAppData;
    (void) pBuffer;
    /* TODO: Wrap every call that can generate an event,
     * and panic if an unexpected event arrives */
    return 0;
}


static OMX_ERRORTYPE FillBufferDone(OMX_HANDLETYPE hComponent,
                                    OMX_PTR pAppData, OMX_BUFFERHEADERTYPE *pBuffer)
{
    (void) hComponent;
    (void) pAppData;
    (void) pBuffer;
    printf("FillBufferDone\n");
    return 0;
}


static struct OMX_CALLBACKTYPE callbacks =
{
    EventHandler,
    EmptyBufferDone,
    &FillBufferDone
};


int omx_init(struct omx_state *st)
{
    OMX_ERRORTYPE err;
    bcm_host_init();
    st->buffers = NULL;
    err = OMX_Init();
    err |= OMX_GetHandle(&st->video_render, "OMX.broadcom.video_render", 0, &callbacks);

    if (!st->video_render || err != 0)
    {
        printf("omx: Failed to create OMX video_render component\n");
        return ENOENT;
    }
    else
    {
        printf("omx: created video_render component\n");
        return 0;
    }
}


/* Some busy loops to verify we're running in order */
static void block_until_state_changed(OMX_HANDLETYPE hComponent, OMX_STATETYPE wanted_eState)
{
    OMX_STATETYPE eState;
    unsigned int i = 0;
    uint32_t loop_counter = 0;

    while (i++ == 0 || eState != wanted_eState)
    {
        OMX_GetState(hComponent, &eState);

        if (eState != wanted_eState)
        {
            usleep_usec(10000);
        }

        loop_counter++;

        if (loop_counter > 50)
        {
            // we don't want to get stuck here
            break;
        }
    }
}


void omx_deinit(struct omx_state *st)
{
    if (!st) return;

    OMX_SendCommand(st->video_render, OMX_CommandStateSet, OMX_StateIdle, NULL);
    block_until_state_changed(st->video_render, OMX_StateIdle);
    OMX_SendCommand(st->video_render, OMX_CommandStateSet, OMX_StateLoaded, NULL);
    usleep_usec(150000);
    OMX_SendCommand(st->video_render,OMX_CommandFlush, VIDEO_RENDER_PORT, NULL);
    OMX_FreeHandle(st->video_render);
    OMX_Deinit();
}


void omx_display_disable(struct omx_state *st)
{
    printf("omx_display_disable\n");
    OMX_ERRORTYPE err;
    OMX_CONFIG_DISPLAYREGIONTYPE config;

    if (!st)   return;

    memset(&config, 0, sizeof(OMX_CONFIG_DISPLAYREGIONTYPE));
    config.nSize = sizeof(OMX_CONFIG_DISPLAYREGIONTYPE);
    config.nVersion.nVersion = OMX_VERSION;
    config.nPortIndex = VIDEO_RENDER_PORT;
    config.fullscreen = OMX_FALSE;//OMX_TRUE;//OMX_FALSE;
    config.set = OMX_DISPLAY_SET_FULLSCREEN;
    err = OMX_SetParameter(st->video_render, OMX_IndexConfigDisplayRegion, &config);
    if (err != 0)
    {
        printf("omx_display_disable command failed\n");
    }
}


static void block_until_port_changed(OMX_HANDLETYPE hComponent, OMX_U32 nPortIndex, OMX_BOOL bEnabled)
{
    OMX_ERRORTYPE r;
    OMX_PARAM_PORTDEFINITIONTYPE portdef;
    OMX_U32 i = 0;
    memset(&portdef, 0, sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
    portdef.nSize = sizeof(OMX_PARAM_PORTDEFINITIONTYPE);
    portdef.nVersion.nVersion = OMX_VERSION;
    portdef.nPortIndex = nPortIndex;
    uint32_t    loop_counter = 0;

    while (i++ == 0 || portdef.bEnabled != bEnabled)
    {
        r = OMX_GetParameter(hComponent, OMX_IndexParamPortDefinition, &portdef);

        if (r != OMX_ErrorNone)
        {
            printf("block_until_port_changed: OMX_GetParameter  failed with Result=%d\n", r);
        }

        if (portdef.bEnabled != bEnabled)
        {
            usleep_usec(10000);
        }

        loop_counter++;

        if (loop_counter > 50)
        {
            // we don't want to get stuck here
            break;
        }
    }
}



int omx_display_xy(int flag, struct omx_state *st, int width, int height, int stride)
{
    OMX_CONFIG_DISPLAYREGIONTYPE config;
    OMX_ERRORTYPE err = 0;
    printf("omx_update_size %d %d %d\n", width, height, stride);
    memset(&config, 0, sizeof(OMX_CONFIG_DISPLAYREGIONTYPE));
    config.nSize = sizeof(OMX_CONFIG_DISPLAYREGIONTYPE);
    config.nVersion.nVersion = OMX_VERSION;
    config.nPortIndex = VIDEO_RENDER_PORT;
  
    config.dest_rect.x_offset  = 0;
    config.dest_rect.y_offset  = 0;
    config.dest_rect.width     = (int)(1920 / 2);
    config.dest_rect.height    = (int)(1080 / 2);
    config.mode = OMX_DISPLAY_MODE_LETTERBOX;

    if (flag == 0)
    {
        config.transform = OMX_DISPLAY_ROT0;
    }
    else
    {
        config.transform = OMX_DISPLAY_ROT90;
    }

    config.fullscreen = OMX_FALSE;
    config.set = (OMX_DISPLAYSETTYPE)(OMX_DISPLAY_SET_TRANSFORM | OMX_DISPLAY_SET_DEST_RECT | OMX_DISPLAY_SET_FULLSCREEN | OMX_DISPLAY_SET_MODE);
    err |= OMX_SetParameter(st->video_render, OMX_IndexConfigDisplayRegion, &config);
}


int omx_display_enable(struct omx_state *st, int width, int height, int stride, int displaynum)
{
    unsigned int i;
    OMX_PARAM_PORTDEFINITIONTYPE portdef;
    OMX_CONFIG_DISPLAYREGIONTYPE config;
    OMX_ERRORTYPE err = 0;
    printf("omx_update_size %d %d %d\n", width, height, stride);

    
    //by aphero 2018-2-23
    OMX_STATETYPE eState;	
	OMX_GetState(st->video_render, &eState);
	if (eState == OMX_StateExecuting)
    {
        printf("omx is already OMX_StateExecuting \n");
        err |= OMX_SendCommand(st->video_render,OMX_CommandFlush, VIDEO_RENDER_PORT, NULL);
        err |= OMX_SendCommand(st->video_render,OMX_CommandStateSet, OMX_StateIdle, NULL);//可以不用设置
		err |= OMX_SendCommand(st->video_render,OMX_CommandPortDisable, VIDEO_RENDER_PORT, NULL);//必须关闭端口使能，否则无法设置端口
		if (err != 0)
        {
            printf("omx_display_enable:reopen failed\n");
        }
		//释放渲染器的缓存空间，这里必须释放，否则出现段错误
		if (st->buffers) {
            for (int i = 0; i < st->num_buffers; i++)
            {
                err |= OMX_FreeBuffer(st->video_render, VIDEO_RENDER_PORT, st->buffers[i]);
            } 
            if (err != 0)
            {
                printf("omx_display_enable:OMX_FreeBuffer failed\n");
            }
			free(st->buffers);
			st->buffers=NULL;
			st->num_buffers = 0;
			st->current_buffer = 0;
		}
        err |= OMX_SendCommand(st->video_render,OMX_CommandStateSet, OMX_StateLoaded, NULL);//可以不用设置
        if (err != 0)
        {
            printf("omx_display_enable:reopen failed\n");
        }
    }
    

    memset(&config, 0, sizeof(OMX_CONFIG_DISPLAYREGIONTYPE));
    config.nSize = sizeof(OMX_CONFIG_DISPLAYREGIONTYPE);
    config.nVersion.nVersion = OMX_VERSION;
    config.nPortIndex = VIDEO_RENDER_PORT;

    /*s
    config.dest_rect.x_offset  = 0;
    config.dest_rect.y_offset  = 0;
    config.dest_rect.width     = 1600;
    config.dest_rect.height    = 900;
    */
    config.fullscreen = 1;
    config.mode = OMX_DISPLAY_MODE_FILL; //OMX_DISPLAY_MODE_LETTERBOX;//可以在IL/OMX_Broadcom.h查找
    config.num = displaynum;
    config.layer = 128;
    config.set = OMX_DISPLAY_SET_FULLSCREEN | OMX_DISPLAY_SET_DEST_RECT | OMX_DISPLAY_SET_MODE |OMX_DISPLAY_SET_LAYER | OMX_DISPLAY_SET_NUM; //(OMX_DISPLAYSETTYPE)(OMX_DISPLAY_SET_FULLSCREEN | OMX_DISPLAY_SET_MODE);

 
    err |= OMX_SetParameter(st->video_render, OMX_IndexConfigDisplayRegion, &config);
    if (err != 0)
    {
        printf("omx_display_enable: couldn't configure display region\n");
    }

    memset(&portdef, 0, sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
    portdef.nSize = sizeof(OMX_PARAM_PORTDEFINITIONTYPE);
    portdef.nVersion.nVersion = OMX_VERSION;
    portdef.nPortIndex = VIDEO_RENDER_PORT;
    /* specify buffer requirements */
    err |= OMX_GetParameter(st->video_render, OMX_IndexParamPortDefinition, &portdef);
    if (err != 0)
    {
        printf("omx_display_enable: couldn't retrieve port def\n");
        err = ENOMEM;
        goto exit;
    }

    if(portdef.bEnabled != 1){//如果端口关闭就打开 by aphero
		err |=OMX_SendCommand(st->video_render,OMX_CommandPortEnable, VIDEO_RENDER_PORT, NULL);//前面关闭了，一定要打开,aphero
	}


    printf("omx port definition(before): h=%d w=%d s=%d sh=%d enable=%d\n",
        portdef.format.video.nFrameWidth,
        portdef.format.video.nFrameHeight,
        portdef.format.video.nStride,
        portdef.format.video.nSliceHeight,
        portdef.bEnabled);

    portdef.format.video.nFrameWidth = width;
    portdef.format.video.nFrameHeight = height;
    portdef.format.video.nStride = stride;
    portdef.format.video.nSliceHeight = height;
    portdef.format.video.xFramerate = 30 << 16;
    portdef.bEnabled = 1;
    //
    printf( "omx port definition(after): h=%d w=%d s=%d sh=%d enable=%d\n",
        portdef.format.video.nFrameWidth,
        portdef.format.video.nFrameHeight,
        portdef.format.video.nStride,
        portdef.format.video.nSliceHeight,
        portdef.bEnabled);
    //
    err |= OMX_SetParameter(st->video_render, OMX_IndexParamPortDefinition, &portdef);
    if (err)
    {
        printf("omx_display_enable: could not set port definition\n");
        goto exit;
    }

    block_until_port_changed(st->video_render, VIDEO_RENDER_PORT, 1);
    err |= OMX_GetParameter(st->video_render, OMX_IndexParamPortDefinition, &portdef);

    if (portdef.format.video.nFrameWidth != width || portdef.format.video.nFrameHeight != height || portdef.format.video.nStride != stride)
    {
        printf("could not set requested resolution\n");
        err = EINVAL;
        goto exit;
    }

    if (err != 0 || !portdef.bEnabled)
    {
        printf("omx_display_enable: failed to set up video port\n");
        err = ENOMEM;
        goto exit;
    }

    /* HACK: This state-change sometimes hangs for unknown reasons,
     *       so we just send the state command and wait 50 ms */
    /* block_until_state_changed(st->video_render, OMX_StateIdle); */
    OMX_SendCommand(st->video_render, OMX_CommandStateSet, OMX_StateIdle, NULL);
    usleep_usec(150000);

    if (!st->buffers)
    {
        printf("OMX_AllocateBuffer\n");
        st->buffers = malloc(portdef.nBufferCountActual * sizeof(void *));
        st->num_buffers = portdef.nBufferCountActual;
        st->current_buffer = 0;

        for (i = 0; i < portdef.nBufferCountActual; i++)
        {
            err = OMX_AllocateBuffer(st->video_render, &st->buffers[i], VIDEO_RENDER_PORT, st, portdef.nBufferSize);

            if (err)
            {
                printf("OMX_AllocateBuffer failed: %d\n", err);
                err = ENOMEM;
                goto exit;
            }
        }
    }

    printf("omx_update_size: send to execute state\n");
    OMX_SendCommand(st->video_render, OMX_CommandStateSet, OMX_StateExecuting, NULL);
    block_until_state_changed(st->video_render, OMX_StateExecuting);
exit:
    return err;
}

int omx_display_input_buffer(struct omx_state *st, void **pbuf, uint32_t *plen)
{
    int buf_idx;

    if (!st->buffers)
    {
        return EINVAL;
    }

    buf_idx = st->current_buffer;
    *pbuf = st->buffers[buf_idx]->pBuffer;
    *plen = st->buffers[buf_idx]->nAllocLen;
    st->buffers[buf_idx]->nFilledLen = *plen;
    st->buffers[buf_idx]->nOffset = 0;
    /*
    //只用buffers[0]的情况下：
    *pbuf = st->buffers[0]->pBuffer;
	*plen = st->buffers[0]->nAllocLen;
	st->buffers[0]->nFilledLen = *plen;
	st->buffers[0]->nOffset = 0;
    */
    return 0;
}

int omx_display_flush_buffer(struct omx_state *st, uint32_t data_len)
{
    
    int buf_idx = st->current_buffer;
    st->current_buffer = (st->current_buffer + 1) % st->num_buffers;
    st->buffers[buf_idx]->nFlags = OMX_BUFFERFLAG_STARTTIME;
    st->buffers[buf_idx]->nOffset = 0;
    st->buffers[buf_idx]->nFilledLen = data_len;
    st->buffers[buf_idx]->nTimeStamp = omx_ticks_from_s64(0);

    if (OMX_EmptyThisBuffer(st->video_render, st->buffers[buf_idx]) != OMX_ErrorNone)
    {
        printf("OMX_EmptyThisBuffer error\n");
    }
    //else
    //{
    //    printf("omx_display_flush_buffer:idx=%d ptr=%p pnum=%d\n", buf_idx, (void *)st->buffers[buf_idx], (int)((void *)st->buffers[buf_idx] - (void *)st->buffers));
    //}
/*
    //只用buffers[0]的情况下：
   if (OMX_EmptyThisBuffer(st->video_render, st->buffers[0]) != OMX_ErrorNone) {
		printf("OMX_EmptyThisBuffer error");
	}
 */

    return 0;
}
