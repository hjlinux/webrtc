#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>
#include "zk_comm.h"
#include "audio_pcm.h"
#include "class_interface.h"
#include "message_queue.h"

#define STEREO_EN    1

static int InteractionFlag = 0;
static int audioAiFd=0;
static int audioAoFd=0;

typedef struct _AudioBuff_ {
    unsigned char buff[4096];
    int maxLen;
    int startPos;
    int endPos; 
    struct _AudioBuff_ *next;
}AUDIOBUFF;

static AUDIOBUFF AudioBuff1;
static AUDIOBUFF AudioBuff2;
static AUDIOBUFF AudioBuff3;
static AUDIOBUFF AudioBuff4;
static AUDIOBUFF AudioBuff5;
static AUDIOBUFF AudioBuff6;
static AUDIOBUFF *AudioInBuff;
static AUDIOBUFF *AudioOutBuff;

static int flag2;

//#define TEST

//static int fdx = 0;

int GetPcmFrame(unsigned char *pbuf)
{
#if 1
#ifdef TEST
    if((AudioInBuff->endPos - AudioInBuff->startPos) >= 960*(STEREO_EN+1))
    {
        memcpy(pbuf,AudioInBuff->buff+AudioInBuff->startPos,960*(STEREO_EN+1));
        AudioInBuff->startPos += 960*(STEREO_EN+1);
        return 0;    
    }
#endif

    fd_set read_fds;
    struct timeval TimeoutVal;
    int frameLen = 0, ret;
    if(audioAiFd <= 0) {
        audioAiFd = getAudioInFd();
    }
    //printf("GetPcmFrame,fd:%d\n",audioAiFd);
    if(audioAiFd <= 0) {
        usleep(20000);
        return -1;
    }
    int i = 0, s32Ret = 0,offset = 0, maxfd = audioAiFd + 1;
    AEC_FRAME_S stAecFrm;
    AUDIO_FRAME_S stFrm;
    FD_ZERO(&read_fds);
    FD_SET(audioAiFd, &read_fds);

    TimeoutVal.tv_sec = 1;
    TimeoutVal.tv_usec = 0;
    s32Ret = select(maxfd, &read_fds, NULL, NULL, &TimeoutVal);
    if (s32Ret < 0) {
        printf("%d: select err: %d!\n", __LINE__, errno);
        return -1;
    } else if (0 == s32Ret) {
        printf("%d: select time out!\n", __LINE__);
        return -1;
    } else {
        if (FD_ISSET(audioAiFd, &read_fds)) 
        {   
#if 0
            if(fdx == 0) {
                fdx = open("./62.test",O_CREAT|O_TRUNC|O_WRONLY,0666);    
            }
#endif
            ret = readN(audioAiFd,&frameLen,4);
            if(ret < 0) {
                return -1;    
            }
//            printf("get audio len:%d\n",frameLen);
            //ret = write(fdx,&frameLen,4);
            //if(frameLen != 500)
            //    exit(0);
            //if(frameLen > 1920) {
            //    return -1;    
            //}
#if 0
            if(flag2 > 40) {
                flag2 = 0;
                printf("get audio len:%d\n",frameLen);
            }
            flag2 ++;
#endif
#ifdef TEST
            int offset = AudioInBuff->endPos-AudioInBuff->startPos;
            memcpy(pbuf,AudioInBuff->buff+AudioInBuff->startPos,offset);

            while((AudioInBuff->maxLen - AudioInBuff->endPos) < frameLen) {
                AudioInBuff->startPos = 0;
                AudioInBuff->endPos = 0;
                AudioInBuff = AudioInBuff->next;
            }
            ret = readN(audioAiFd,AudioInBuff->buff+AudioInBuff->endPos,frameLen);
            if(ret < 0) {
                return -1;    
            }
            AudioInBuff->endPos += frameLen;
            memcpy(pbuf+offset,AudioInBuff->buff+AudioInBuff->startPos,960*(STEREO_EN+1)-offset);
            AudioInBuff->startPos += (960*(STEREO_EN+1)-offset);
#else
            ret = readN(audioAiFd,pbuf,frameLen);
            if(ret < 0) {
                return -1;    
            }
  //          printf("get audio data\n");
            //ret = write(fdx,pbuf,frameLen);
#endif
        }
        return 0;
    }
    return -1;
#else 
    printf("audio ai get ...\n");
    usleep(20000);
    return 0;
#endif
}

static int flag1;

int SendPcmFrame(unsigned char *pbuf)
{
#if 1
    int ret;
    if(audioAiFd <= 0)
        return 0;
    if(audioAoFd <= 0) {
        audioAoFd = getRemoteAudioFd();
        if(audioAoFd <= 0) {
            return 0;    
        }
    }
    //if(flag1 > 40) {
    //    flag1 = 0;
  //  printf("audio ao send...:%d\n",audioAoFd);    
    //}
    //flag1 ++;
    int len = 960*(STEREO_EN+1);
#ifdef TEST
    if((AudioOutBuff->endPos + len) < 2048*(STEREO_EN+1)) {
        memcpy(AudioOutBuff->buff+AudioOutBuff->endPos,pbuf,len);
        AudioOutBuff->endPos += len;
        return 0;
    }
    int offset = 2048*(STEREO_EN+1) - AudioOutBuff->endPos;
    memcpy(AudioOutBuff->buff+AudioOutBuff->endPos,pbuf,offset);
    int total = 2048*(STEREO_EN+1);
    ret = writeN(audioAoFd,&total,4);
    if(ret < 0) {
        return 0;    
    }
    ret = writeN(audioAoFd,AudioOutBuff->buff,2048*(STEREO_EN+1));
    if(ret < 0) {
        return 0;    
    }
    memcpy(AudioOutBuff->buff,pbuf+offset,len-offset);
    AudioOutBuff->endPos = len - offset;
#else
    ret = writeN(audioAoFd,&len,4);
    if(ret < 0) {
        return 0;    
    }
    ret = writeN(audioAoFd,pbuf,len);
    if(ret < 0) {
        return 0;    
    }
#endif
    return 0;
#else
    //printf("audio ao send...\n");    
#endif
}


RemoteFrame *remote_frame_list = NULL;

RemoteFrame* GetFrameByLocalSsrc(unsigned int ssrc)
{
    RemoteFrame *pos = remote_frame_list;
    if (pos == NULL)
    {
        return NULL;
    }

    while(pos != NULL)
    {
        if (pos->frame_info.local_ssrc == ssrc)
        {
            return pos;
        }
        pos = pos->next;
    }

    return NULL;
}

RemoteFrame* CreateFrame(FrameInfo *frame_info, short *frame_data)
{
    RemoteFrame *p_new;
    int len;

    p_new = (RemoteFrame*)malloc(sizeof(RemoteFrame));
    if (p_new == NULL)
    {
        return NULL;
    }

    memset(p_new, 0, sizeof(RemoteFrame));
    memcpy(&p_new->frame_info, frame_info, sizeof(FrameInfo));
    len = frame_info->samples_per_channel * frame_info->num_channels;
    memcpy(p_new->buf[0], frame_data, len*2);
    p_new->read_index = 0;
    p_new->write_index = 1;

    p_new->next = remote_frame_list;
    remote_frame_list = p_new;
    return p_new;
}

void AddFrame(RemoteFrame* frame, short *frame_data)
{
    int len;

    len = frame->frame_info.samples_per_channel * frame->frame_info.num_channels;
    memcpy(frame->buf[frame->write_index], frame_data, len*2);
    if (((frame->write_index + 1) % BUF_SIZE) != frame->read_index)
    {
        frame->write_index++;
        if (frame->write_index == BUF_SIZE)
        {
            frame->write_index = 0;
        }
    }
}

void SaveRemoteFrame(FrameInfo *frame_info, short *frame_data)
{
    return;
    RemoteFrame *pos = NULL;
    pos = GetFrameByLocalSsrc(frame_info->local_ssrc);
    if (pos == NULL)
    {
        CreateFrame(frame_info, frame_data);
    }
    else
    {
        pos->frame_info.samples_per_channel = frame_info->samples_per_channel;
        pos->frame_info.num_channels = frame_info->num_channels;
        AddFrame(pos, frame_data);
    }
}

int GetRemoteFrame(FrameInfo *frame_info, short *frame_data)
{
    return -1;
    int i, temp, len;
    RemoteFrame *pos = remote_frame_list;
    int flag = -1;

    memset(frame_data, 0, 7680);

    while(pos != NULL)
    {
        if (pos->frame_info.local_ssrc == frame_info->local_ssrc
                || pos->read_index == pos->write_index)
        {
            pos = pos->next;
            //printf("--get next\n");
            continue;
        }
        frame_info->num_channels = pos->frame_info.num_channels;
        frame_info->samples_per_channel = pos->frame_info.samples_per_channel;
        frame_info->sample_rate_hz = pos->frame_info.sample_rate_hz;

/*
        printf("--get sample %d,num_ch %d, read_index %d, write_index %d,pos ssrc %u,%u\n",
                frame_info->samples_per_channel, frame_info->num_channels,pos->read_index,pos->write_index,
                pos->frame_info.local_ssrc, frame_info->local_ssrc);
        */

        len = pos->frame_info.num_channels * pos->frame_info.samples_per_channel;

        for (i = 0; i < len; i++)
        {
            temp = (int)(frame_data[i] + pos->buf[pos->read_index][i]);
            if (temp < -0x00008000)
            {
                temp = -0x8000;
            }
            else if (temp > 0x00007FFF) 
            {
                temp = 0x7FFF;
            }
            frame_data[i] = (short)temp;
        }
        pos->read_index++;
        if (pos->read_index == BUF_SIZE)
        {
            pos->read_index = 0;
        }
        pos = pos->next;
        flag = 1;
    }
    return flag;
}


void stopAi(void)
{
    printf("stopAi1\n");
    audioAiFd=0;
    audioAoFd=0;
    if(InteractionFlag == 1) {
        InteractionFlag = 0;
        StopInteraction();
    }
    printf("stopAi2\n");
}

void startAi(void)
{
    AudioBuff1.startPos = 0;
    AudioBuff1.endPos = 0;
    AudioBuff1.maxLen = 2048*(STEREO_EN+1);
    AudioBuff2.startPos = 0;
    AudioBuff2.endPos = 0;
    AudioBuff2.maxLen = 2048*(STEREO_EN+1);
    AudioBuff3.startPos = 0;
    AudioBuff3.endPos = 0;
    AudioBuff3.maxLen = 2048*(STEREO_EN+1);
    AudioBuff1.next = &AudioBuff2;
    AudioBuff2.next = &AudioBuff3;
    AudioBuff3.next = &AudioBuff1;
    AudioInBuff = &AudioBuff1;

    AudioBuff4.startPos = 0;
    AudioBuff4.endPos = 0;
    AudioBuff4.maxLen = 2048*(STEREO_EN+1);
    AudioBuff5.startPos = 0;
    AudioBuff5.endPos = 0;
    AudioBuff5.maxLen = 2048*(STEREO_EN+1);
    AudioBuff6.startPos = 0;
    AudioBuff6.endPos = 0;
    AudioBuff6.maxLen = 2048*(STEREO_EN+1);
    AudioBuff4.next = &AudioBuff5;
    AudioBuff5.next = &AudioBuff6;
    AudioBuff6.next = &AudioBuff4;
    AudioOutBuff = &AudioBuff4;
    printf("startAi0\n");
    if(InteractionFlag == 0) {
        InteractionFlag = 1;
        StartInteraction();
    }
    printf("startAi1\n");
}

