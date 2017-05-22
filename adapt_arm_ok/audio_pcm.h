#ifndef _AUDIO_PCM_H_
#define _AUDIO_PCM_H_

#ifdef __cplusplus
extern "C"
{
#endif


typedef struct _frame_info_
{
    unsigned int local_ssrc;
    unsigned int remote_ssrc;
    int samples_per_channel;
    int sample_rate_hz;
    int num_channels;
}FrameInfo;

#define BUF_SIZE   3
typedef struct _remote_frame_
{
    FrameInfo frame_info;
    short buf[BUF_SIZE][3840];
    volatile int read_index;
    volatile int write_index;
    struct _remote_frame_ *next;
}RemoteFrame;


void startAi(void);
void stopAi(void);

int GetPcmFrame(unsigned char *pbuf);
int SendPcmFrame(unsigned char *pbuf);
void SaveRemoteFrame(FrameInfo *frame_info, short *frame_data);
int GetRemoteFrame(FrameInfo *frame_info, short *frame_data);



#ifdef __cplusplus
}
#endif

#endif
