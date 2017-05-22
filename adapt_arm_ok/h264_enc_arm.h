#ifndef _H264_ENC_ARM_H_
#define _H264_ENC_ARM_H_


#ifdef __cplusplus
extern "C"
{
#endif


typedef struct _nal_data_
{
	unsigned char *buf;
	int len;
	unsigned char nal_type;
}nal_data;


typedef struct _nal_info_
{
	unsigned char num_nals;
	unsigned char idr_flag;
    nal_data nals[10];//array, num_nals < 5
}nal_info;


typedef struct _VEncStream_
{
	int ch;
	unsigned char *py;
	unsigned char *pu;
	unsigned char *pv;
	int width;
	int height;
	unsigned int ntp_time_ms;
	void(*CopyEncData)(void *param, nal_info *pinfo);
	void *param;
}VEncStream;


int InitVenc(int VencChn);
void ReleaseVenc(int VeChn);
int EncodeVideoStream(VEncStream *pvenc_stream);
void VencRequestIDR(int venc_ch, int bInstant);
int VencSetBitRate(int venc_ch, int bitrate);


#ifdef __cplusplus
}
#endif
#endif
