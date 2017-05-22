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
#include "h264_dec_arm.h"
#include "resource_note.h"
#include "param_conf.h"
#include "message_queue.h"
#include "class_interface.h"
#include <libavutil/pixfmt.h>

//static int stu_fd;
//static int flag[3];

int DecodeVideoStream(VDecStream *pvdec_stream)
{
    int fd;
	VDEC_STREAM_S stStream;
	HI_S32 s32Ret;
	int VdChn;
    unsigned char *py,*pu,*pv;
    int width = 640;
    int height = 480;
#if 0
    flag[pvdec_stream->ch] ++;
    if(flag[pvdec_stream->ch] > 40) {
        flag[pvdec_stream->ch] = 0;
        //printf("0. vdec len[%d]:%d,fd:%d\n",pvdec_stream->ch,pvdec_stream->len_264);
    }
#endif
//    printf("vdec\n");
    if(!IsTeacher()) {
        if(getTeacherAcrossBind() < 0)
        {
            py = malloc(width*height+width*height/2);
            if(py)
            {
                py[0] = pvdec_stream->ch;
                pu = py + width*height;
                pv = pu + width*height/4;
  		        pvdec_stream->pframe = pvdec_stream->Createframe(width, height, py, pu, pv);
                free(py);
                return 0;
            }
            return -1;
        }
    }
#if 1
    fd = getRemoteFd(pvdec_stream->ch);
    if(fd <= 0) {
        return -1;    
    }
    int len = pvdec_stream->len_264;
    writeN(fd,&len,4);
//    printf("fd:%d,chn:%d,dec len:%d\n",fd,pvdec_stream->ch,len);
    writeN(fd,pvdec_stream->pbuf_264,pvdec_stream->len_264);
//    printf("vdec ch:%d,len:%d\n",pvdec_stream->ch,pvdec_stream->len_264);
#endif

#if 0
    if(flag[pvdec_stream->ch] == 0) {
        flag[pvdec_stream->ch] = 0;
        printf("1. vdec len[%d]:%d,fd:%d\n",pvdec_stream->ch,pvdec_stream->len_264,fd);
    }
#endif
#if 0
    if(IsTeacher()) {
       if(stu_fd == 0) {
            stu_fd = open("/hejl/webrtc/stu_dec.h264",O_CREAT | O_WRONLY | O_TRUNC,0666);   
        }     
        if(stu_fd > 0) {
            write(stu_fd,pvdec_stream->pbuf_264,pvdec_stream->len_264);    
        }
    }
#endif
    //printf("vdec,chn:%d,len:%d\n",pvdec_stream->ch,pvdec_stream->len_264);
    

    if(0)
	{
		VDEC_CHN_STAT_S stStat;
		HI_MPI_VDEC_Query(VdChn, &stStat);
		ZK_PRT("************************** stat info***************************\n");
		ZK_PRT("u32LeftStreamBytes: %u\n", stStat.u32LeftStreamBytes);
		ZK_PRT("u32LeftStreamFrames: %u\n", stStat.u32LeftStreamFrames);
		ZK_PRT("u32LeftPics: %u\n", stStat.u32LeftPics);
		ZK_PRT("bStartRecvStream: %d\n", stStat.bStartRecvStream);
		ZK_PRT("u32RecvStreamFrames: %u\n", stStat.u32RecvStreamFrames);
		ZK_PRT("u32DecodeStreamFrames: %u\n", stStat.u32DecodeStreamFrames);

		//stStat.u32DecodeStreamFrames.stVdecDecErr.
		printf("s32FormatErr %d\n",stStat.stVdecDecErr.s32FormatErr); /* 不支持的格式 */
		printf("s32PicSizeErrSe %d\n",stStat.stVdecDecErr.s32PicSizeErrSet); /* 图像的宽（或高）比通道的宽（或高）大 */
		printf("s32StreamUnsprt %d\n",stStat.stVdecDecErr.s32StreamUnsprt); /* 不支持的规格 */
		printf("s32PackErr %d\n",stStat.stVdecDecErr.s32PackErr); /* 码流错误 */
		printf("s32PrtclNumErrSet %d\n",stStat.stVdecDecErr.s32PrtclNumErrSet); /* 设置的协议参数个数不够 */
		printf("s32RefErrSet %d\n",stStat.stVdecDecErr.s32RefErrSet); /* 设置的参考帧个数不够 */
		printf("s32PicBufSizeErrSet %d\n",stStat.stVdecDecErr.s32PicBufSizeErrSet); /* 图像Buffer大小不够 */
		printf("s32VdecStreamNotRelease %d\n",stStat.stVdecDecErr.s32VdecStreamNotRelease); /*码流长时间没有释放*/
		ZK_PRT("***************************************************************\n");

	}
	return -1;
}

int InitVdec(int clientChn)
{
    return -1;    
}


void ClearCurInteraction()
{
}

void SwitchClass(int clas)
{
}


void ClientDisplay(int clientChn) 
{
}


void ReleaseVdec(int clientChn)
{
}



