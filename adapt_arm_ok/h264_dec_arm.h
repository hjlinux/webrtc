#ifndef _H264_DEC_ARM_H_
#define _H264_DEC_ARM_H_


#ifdef __cplusplus
extern "C"
{
#endif

#include "resource_note.h"

typedef struct _VDecStream_
{
	int ch;
    int fd;
	unsigned char *pbuf_264;
	int len_264;
	unsigned long long ntp;
	void* (*Createframe)(int width, int height, unsigned char *py, 
	                    unsigned char *pu, unsigned char *pv);
	void *pframe;
}VDecStream;


int InitVdec(int clientChn);
void ReleaseVdec(int VdChn);
int DecodeVideoStream(VDecStream *pvdec_stream);
void ClientDisplay(int clientChn);
void SwitchClass(int clas);
void ClearCurInteraction();



#ifdef __cplusplus
}
#endif
#endif
