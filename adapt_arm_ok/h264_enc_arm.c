#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <linux/fb.h>
#include "zk_comm.h"
#include "h264_enc_arm.h"
#include "message_queue.h"

static int stu_fd;
static int flag[3];

int EncodeVideoStream(VEncStream *pvenc_stream)
{
	nal_info info;
	unsigned char n;
	unsigned char *py = pvenc_stream->py;
	memset(&info, 0, sizeof(info));
	info.num_nals = py[4];
	info.idr_flag = 0;//false;
#if 1
	if (info.num_nals > 16)
	{
		printf("\n\nencode nals number is more than 5, bug???,[%d].\n\n", info.num_nals);
		info.num_nals = 10;
        return -1;
	}
#endif
	{
#if 1
        int len = 0;
        int nalType = 0;
        int offset = 5;
		for (n = 0; n < info.num_nals; n++)
		{
            memcpy(&len,py+offset,4);
            if(len > 300000)
                return -1;
            offset += 4;
            nalType = py[offset];
            if((nalType != 1) && (nalType != 5) && (nalType != 6) && (nalType != 7) && (nalType != 8))
                return -1;
//            printf("enc,type:%d,len:%d\n",nalType,len);
			info.nals[n].len = len-1;
			info.nals[n].buf = malloc(len-1);
            memcpy(info.nals[n].buf,py+offset+1,len-1);
#if 0
            if(!IsTeacher()) {
                if(stu_fd == 0) {
                stu_fd = open("/hejl/webrtc/stu_s.h264",O_CREAT | O_WRONLY | O_TRUNC,0666);    
                }
                if(stu_fd > 0) {
                    write(stu_fd,info.nals[n].buf,info.nals[n].len);    
                }
            }
#endif
            offset += len;
			info.nals[n].nal_type = nalType;
			if (info.nals[n].nal_type == H264E_NALU_ISLICE)
			{
                //printf("H264E_NALU_ISLICE\n");
				info.idr_flag = 1;//true
			}
		}
  //      printf("enc end\n");
		pvenc_stream->CopyEncData(pvenc_stream->param, &info);//callback copy data
		//printf("encode nals [%d]\n", stStream.u32PackCount);
		for (n = 0; n < info.num_nals; n++)
		{
			free(info.nals[n].buf);
        }
#endif

	}

	return 0;
}

int VencSetBitRate(int venc_ch, int bitrate)
{
//    printf("VencSetBitRate:%d,%d\n",venc_ch,bitrate);
  //  if(venc_ch < 0)
    //    return -1;
    if(bitrate > 1536)
    {
        bitrate = 2048;    
    }
    else if(bitrate > 1408)
    {
        bitrate = 1408;    
    }
    else if(bitrate > 1280)
    {
        bitrate = 1280;    
    }
    else if(bitrate > 1152)
    {
        bitrate = 1152;    
    }
    else if(bitrate > 1024)
    {
        bitrate = 1024;    
    }
    else if(bitrate > 896)
    {
        bitrate = 896;    
    }
    else if(bitrate > 768)
    {
        bitrate = 768;    
    }
    else if(bitrate > 640)
    {
        bitrate = 640;    
    }
    else
    {
        bitrate = 512;    
    }
        
    SetBitrate(bitrate);
    return 0;
}

void VencRequestIDR(int venc_ch, int bInstant)
{
    printf("VencRequestIDR\n");
}

int InitVenc(int VencChn)
{
    printf("InitVenc:%d\n",VencChn);
	return 0;
}


void ReleaseVenc(int VeChn)
{
    printf("ReleaseVenc:%d\n",VeChn);
}


