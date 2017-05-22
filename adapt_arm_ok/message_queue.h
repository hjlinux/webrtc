#ifndef _MESSAGE_QUEUE_H_
#define _MESSAGE_QUEUE_H_

#ifdef __cplusplus
extern "C" {
#endif

int message_queue_init(void);
void StartInteraction();
void StopInteraction();
void StopInteraction2();
void StopInteraction3();
int getRecord(void);
int RecorderMsgHandler();
void SetBitrate(int bitrate);
int RECV2(int *msgType, void *msgBuf);

#ifdef __cplusplus
}
#endif

#endif
