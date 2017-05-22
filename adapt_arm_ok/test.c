/******************************************************************************
  A simple program of Hisilicon HI3521A video input and output implementation.
  Copyright (C), 2014-2015, Hisilicon Tech. Co., Ltd.
 ******************************************************************************
Modification:  2015-1 Created
 ******************************************************************************/

#ifdef __cplusplus
#if __cplusplus
extern "C"{
#endif
#endif /* End of #ifdef __cplusplus */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <pthread.h>

#include "zk_comm.h"
#include "resource_note.h"
#include "param_conf.h"
#include "message_queue.h"
#include "display_arm.h"
#include "transmission.h"

#define HDMI_SUPPORT
static VO_DEV HdmiVoDev = ZK_VO_DEV_DHD0;
//static VO_DEV VgaVoDev = ZK_VO_DEV_DHD1;
static int source_fmt[8];




extern HI_S32 StartVirVo(PIP_CONF_S *pipConf);
/* ------------------------------------   shenghuai add   -------------------------------------------------- */
extern HI_S32 ZK_COMM_VI_GetVFrameFromYUV(FILE *pYUVFile, HI_U32 u32Width, HI_U32 u32Height,HI_U32 u32Stride, VIDEO_FRAME_INFO_S *pstVFrameInfo);

typedef struct zkVIDEO_LOSS_S
{
    HI_BOOL bStart;
    pthread_t Pid;
    //VIDEO_PARAM_S *pVideoParam;
}VIDEO_LOSS_S;

static VIDEO_LOSS_S gs_stVideoLoss;
static HI_S32 s_astViLastIntCnt[6] = {0};
static HI_VOID *ZK_VI_VLossDetProc(HI_VOID *parg);

HI_S32 ZK_VI_StartVLossDet(void)
{
    HI_S32 s32Ret;

    gs_stVideoLoss.bStart = HI_TRUE;

    s32Ret = pthread_create(&gs_stVideoLoss.Pid, 0, ZK_VI_VLossDetProc, &gs_stVideoLoss);
    if (HI_SUCCESS != s32Ret)
    {
        printf("pthread_create failed with %#x!\n", s32Ret);
        return HI_FAILURE;
    }

    return HI_SUCCESS;
}

HI_VOID ZK_VI_StopVLossDet()
{
    if (gs_stVideoLoss.bStart)
    {
        gs_stVideoLoss.bStart = HI_FALSE;
        pthread_join(gs_stVideoLoss.Pid, 0);
    }
}

#define VI_SOURCE_DET   0xAAAA
#define VI_SOURCE_LOSS  0x5555



struct CEA_861_Formats
{
    char name[16];
    int  format;
    int  std;
    int  frame;
    int  interlaced;
};

#define GV7601_IOC_MAGIC            'G'
#define GV7601_GET_FMT	            _IOWR(GV7601_IOC_MAGIC, 1, struct CEA_861_Formats) 
#define GV7601_GET_FMT_IRQ	        _IOWR(GV7601_IOC_MAGIC, 5, struct CEA_861_Formats)


/*adv7441 define*/
typedef enum _tagADV7441_VIDEO_INPUT_MODE
{

    VIDEO_HDMI = 0,
    VIDEO_VGA_VESA,
    VIDEO_VGA_CEA,
    VIDEO_YPrPb,
    VIDEO_CVBS,
    //NO_INPUT
} ADV7441_VIDEO_INPUT_MODE;


struct timing_blank
{
    int Hfreq;
    int Vfreq;
    int pixelclk;      /*kHz*/

    int HsyncAct ;    /* Horizontal effetive width */
    int VsyncVact ;   /* Vertical effetive width of one frame or odd-field frame picture */

    int HsyncHfb ;    /* Horizontal front blanking width */
    int Hsync    ;
    int HsyncHbb ;    /* Horizontal back blanking width */

    int VsyncVfb ;    /* Vertical front blanking height of one frame or odd-field frame picture */
    int Vsync;
    int VsyncVbb ;    /* Vertical back blanking height of one frame or odd-field frame picture */

    int VsyncVbfb ;   /* Even-field vertical front blanking height when input mode is interlace (invalid when progressive input mode) */
    int VsyncVbact ;  /* Even-field vertical effetive width when input mode is interlace (invalid when progressive input mode) */
    int VsyncVbbb ;   /* Even-field vertical back blanking height when input mode is interlace (invalid when progressive input mode) */

    int mode;         /*0:Progressive;1: Interlaced*/
    int busdrv;
};


typedef struct _tagADV7441_VIDEO_PARAM_S
{
    ADV7441_VIDEO_INPUT_MODE inMode;
    PIC_SIZE_E capSize;

    HI_BOOL userDefined;
} ADV7441_VIDEO_PARAM_S;

#define ADV7441_IOC_MAGIC            'A'
#define ADV7441_GET_VIDEO_TIMING	_IOWR(ADV7441_IOC_MAGIC, 1, ADV7441_VIDEO_PARAM_S) 
#define ADV7441_SET_VIDEO_TIMING	_IOW (ADV7441_IOC_MAGIC, 2, ADV7441_VIDEO_PARAM_S) 
#define ADV7441_SWITCH_VGA_INTYPE	_IOW (ADV7441_IOC_MAGIC, 3, VGA_INPUT_TYPE) 

#define GV7601_GET_FMT_ALL          _IOWR(GV7601_IOC_MAGIC, 2, struct CEA_861_Formats)

static HI_VOID *ZK_VI_VLossDetProc(HI_VOID *parg)
{ 
    VI_CHN ViChn;
    HI_S32 s32Ret, i;
    VI_CHN_STAT_S stStat;
    VIDEO_LOSS_S *ctl = (VIDEO_LOSS_S*)parg;
    int change[8] = {0};
    int fd_sdi = -1;
    int fd_vga = -1;
    char chn_def[8] = {0,4,8,12,16,20,28,32};

    fd_sdi = open ("/dev/spi",O_RDWR);
    if (fd_sdi < 0)
    {
        printf("not found spi driver\n");		
    }
    fd_vga = open ("/dev/adv7441",O_RDWR);
    if (fd_vga < 0)
    {
        printf("not found spi driver\n");		
    }


    //HI_MPI_VI_SetFrameDepth(20, 4);  /*VGA */
    while (ctl->bStart)
    {		
        for(i = 0; i < 4; i++)
        {
            {
                if((!IsTeacher() && (i != GetVideoDev())) || ((i != GetVideoDev()) && IsTeacher() && (i != GetVgaDev())))
                    continue;
                /*0 4 8 12 16 20 24 32*/
                ViChn = i << 2;

                ViChn = chn_def[i];

                s32Ret = HI_MPI_VI_Query(ViChn, &stStat);
                if (HI_SUCCESS != s32Ret)
                {
                    printf("HI_MPI_VI_Query failed with %#x!\n", s32Ret);
                    return NULL;
                }

                if (stStat.u32IntCnt == s_astViLastIntCnt[i])
                {						
                    HI_MPI_VI_EnableUserPic(ViChn);

                    if (change[i] == VI_SOURCE_DET)
                    {													
                        printf("Video [%d] Out \n",i);							
                    }

                    change[i] = VI_SOURCE_LOSS;						
                }
                else
                {						
                    HI_MPI_VI_DisableUserPic(ViChn);						
                    /*½ÓÈë ok*/												
                    if (change[i] == VI_SOURCE_LOSS)
                    {

                        //                        printf("Video [%d] In \n",i);	
                        if (i < 6)
                        {	
#if 0
                            if (fd_sdi > 0)
                            {
                                res = ioctl(fd_sdi,GV7601_GET_FMT,&i); 								
                                if (res >= 0)
                                {
                                    change[i] = VI_SOURCE_DET;

                                    /*
                                       update param ..
                                     */

                                }
                            }		
#endif
                        }
                        else
                        {
#if 0
                            if (fd_vga > 0)
                            {
                                ADV7441_VIDEO_PARAM_S stParam;//*pstParam;

                                if (ioctl(fd_vga,ADV7441_GET_VIDEO_TIMING,&stParam))
                                {

                                    change[i] = VI_SOURCE_DET;

                                    /*
                                       update param ..
                                     */

                                }
                                else
                                {
                                    printf("xxxxx\n");
                                }
                            }
#endif


                            //change[i] = VI_SOURCE_DET;
                        }			
                    }

                    //change[i] = VI_SOURCE_DET;

                }

                s_astViLastIntCnt[i] = stStat.u32IntCnt;

            }
        }
        usleep(500000);
    }
    close(fd_sdi);
    ctl->bStart = HI_FALSE;

    return NULL;
}

HI_S32 ZK_VI_SetUserPic(HI_CHAR *pszYuvFile, HI_U32 u32Width, HI_U32 u32Height, HI_U32 u32Stride, VIDEO_FRAME_INFO_S *pstFrame)
{
    FILE *pfd;
    VI_USERPIC_ATTR_S stUserPicAttr;

    /* open YUV file */
    pfd = fopen(pszYuvFile, "rb");
    if (!pfd)
    {
        printf("open file -> %s fail \n", pszYuvFile);
        return -1;
    }

    /* read YUV file. WARNING: we only support planar 420) */
    //if (  ZK_COMM_VI_GetVFrameFromYUV(pfd, u32Width, u32Height, u32Stride, pstFrame))
    if (ZK_COMM_VI_GetVFrameFromYUV(pfd, u32Width, u32Height, u32Stride, pstFrame))
    {
        return -1;
    }
    fclose(pfd);

    stUserPicAttr.bPub= HI_TRUE;
    stUserPicAttr.enUsrPicMode = VI_USERPIC_MODE_PIC;
    memcpy(&stUserPicAttr.unUsrPic.stUsrPicFrm, pstFrame, sizeof(VIDEO_FRAME_INFO_S));
    if (HI_MPI_VI_SetUserPic(0, &stUserPicAttr))
    {
        return -1;
    }
    //ZK_PRT("set vi user pic ok, yuvfile:%s\n", pszYuvFile);
    return HI_SUCCESS;
}


//#define USERPIC_PATH				"/mnt/hi3531A/NoSource_640x360_420.yuv"
//#define USERPIC_PATH				"/mnt/hi3531A/NoSource_640x480_420.yuv"
#define USERPIC_PATH				"/zqfs/etc/NoSource_640x360_420.yuv"

static HI_S32 startUserPic(HI_VOID)
{
    HI_S32 s32Ret;

    VIDEO_FRAME_INFO_S stUserFrame;

    printf("ZK_COMM_VI_SetUserPic\n");
    s32Ret = ZK_VI_SetUserPic(USERPIC_PATH, 640, 360, 640, &stUserFrame);
    if (HI_SUCCESS != s32Ret)
    {
        printf("ZK_COMM_VI_SetUserPic failed!\n");
        return HI_FAILURE;
    }

    printf("ZK_VI_StartVLossDet\n");
    s32Ret = ZK_VI_StartVLossDet();
    if (HI_SUCCESS != s32Ret) {
        printf("ZK_VI_StartVLossDet failed!\n");
        return HI_FAILURE;
    }

    return HI_SUCCESS;
}

/* ------------------------------------------------------------------------------------------------ */


/******************************************************************************
 * function : to process abnormal case                                         
 ******************************************************************************/
void ZK_VIO_HandleSig(HI_S32 signo)
{
    if (SIGINT == signo || SIGTSTP == signo)
    {
        ZK_COMM_SYS_Exit();
        printf("\033[0;31mprogram termination abnormally!\033[0;39m\n");
    }
    exit(-1);
}



/******************************************************************************
 * function : VI(1080P: 8 windows) -> VPSS -> HD0(1080P,VGA,BT1120)
 -> HD1(1080P,HDMI)
 -> SD0(D1)
 ******************************************************************************/




HI_VOID zkStopCapture(HI_S32 chn)
{
    printf("StopCapture\n");
    HI_S32 s32Ret;
    ZK_VI_PARAM_S stViParam;                                               
    ZK_VI_MODE_E enViMode = GetCaptureViMode(chn);
    VENC_CHN VencChn = GetCaptureVencChn(chn);
    VPSS_GRP VpssGrp = GetCaptureVpssGrp(chn);
    VO_CHN VoChn = GetCaptureVoChn(chn);
    VI_CHN ViChn = GetCaptureViChn(chn);
    SetCapturePlay(chn,0);
    VPSS_CHN VpssChn = 0;
/*
    ZK_COMM_VENC_UnBindVpss(VencChn, VpssGrp, VpssChn);
    ZK_COMM_VENC_Stop(VencChn);

    ZK_COMM_VO_UnBindVpss(ZK_VO_DEV_DHD0,VoChn,VpssGrp,1);
    ZK_COMM_VI_UnBindVpss_Webrtc(ViChn,VpssGrp,enViMode);
    ZK_COMM_VPSS_Stop(VpssGrp,VPSS_MAX_CHN_NUM);
    HI_MPI_VI_DisableUserPic(ViChn*4);						
    ZK_COMM_VI_Stop(enViMode,ViChn,ViChn*4);
    s32Ret = HI_MPI_VO_ClearChnBuffer(ZK_VO_DEV_DHD0,VoChn,HI_TRUE);
    if(s32Ret != HI_SUCCESS)
    {
        ZK_PRT("HI_MPI_VO_ClearChnBuffer:%x\n",s32Ret);
    }
*/
    PIP_CONF_S *TeacherInteractionPipConf,*TeacherPreviewPipConf,*StudentPreviewPipConf;
    if(IsTeacher()) {
        ZK_COMM_VI_UnBindVpss_Webrtc(ViChn,VpssGrp,enViMode);
        HI_MPI_VI_DisableUserPic(ViChn*4);						
        ZK_COMM_VI_Stop(enViMode,ViChn,ViChn*4);
        if(chn == 0) {
            TeacherInteractionPipConf = GetVirVoTeacherInteraction();
            TeacherPreviewPipConf = GetVirVoTeacherPreview();
            ZK_COMM_VO_UnBindVpss(TeacherInteractionPipConf->virVo->virvoLayer,0,VpssGrp,1);
            ZK_COMM_VO_UnBindVpss(TeacherPreviewPipConf->virVo->virvoLayer,0,VpssGrp,1);
            ZK_COMM_Vpss_UnBindVO(TeacherInteractionPipConf->virVo->virvoVpss,TeacherInteractionPipConf->virVo->virvoLayer);
            ZK_COMM_Vpss_UnBindVO(TeacherPreviewPipConf->virVo->virvoVpss,TeacherPreviewPipConf->virVo->virvoLayer);

            ZK_COMM_VENC_UnBindVpss(VencChn, TeacherInteractionPipConf->virVo->virvoVpss,0);
            ZK_COMM_VO_UnBindVpss(HdmiVoDev,0,TeacherPreviewPipConf->virVo->virvoVpss,1);

            ZK_COMM_VPSS_Stop(VpssGrp,VPSS_MAX_CHN_NUM);
            DisableVirVoChn(TeacherInteractionPipConf);
            DisableVirVoChn(TeacherPreviewPipConf);
            ZK_COMM_VPSS_Stop(TeacherInteractionPipConf->virVo->virvoVpss,VPSS_MAX_CHN_NUM);
            ZK_COMM_VPSS_Stop(TeacherPreviewPipConf->virVo->virvoVpss,VPSS_MAX_CHN_NUM);
        }
        else {
            ZK_COMM_VENC_UnBindVpss(VencChn, VpssGrp,0);
            ZK_COMM_VPSS_Stop(VpssGrp,VPSS_MAX_CHN_NUM);
        }
        ZK_COMM_VENC_Stop(VencChn);
    } else {
        StudentPreviewPipConf = GetVirVoStudentPreview();
        ZK_COMM_VI_UnBindVpss_Webrtc(ViChn,VpssGrp,enViMode);

        ZK_COMM_VENC_UnBindVpss(VencChn, VpssGrp, 0);

        ZK_COMM_VO_UnBindVpss(StudentPreviewPipConf->virVo->virvoLayer,1,VpssGrp,1);
         
        ZK_COMM_Vpss_UnBindVO(StudentPreviewPipConf->virVo->virvoVpss,StudentPreviewPipConf->virVo->virvoLayer);

        ZK_COMM_VO_UnBindVpss(HdmiVoDev,0,StudentPreviewPipConf->virVo->virvoVpss,1);


        HI_MPI_VI_DisableUserPic(ViChn*4);						
        ZK_COMM_VI_Stop(enViMode,ViChn,ViChn*4);
        ZK_COMM_VPSS_Stop(VpssGrp,VPSS_MAX_CHN_NUM);
        ZK_COMM_VENC_Stop(VencChn);
        DisableVirVoChn(StudentPreviewPipConf);
        ZK_COMM_VPSS_Stop(StudentPreviewPipConf->virVo->virvoVpss,VPSS_MAX_CHN_NUM);
//      StopVo();
    }
}
#if 0
HI_S32 ZK_COMM_Vpss_BindVO(VPSS_GRP VpssGrp,VO_LAYER VoLayer)
{
    HI_S32 s32Ret = HI_SUCCESS;
    MPP_CHN_S stSrcChn;
    MPP_CHN_S stDestChn;

    stSrcChn.enModId = HI_ID_VOU;
    stSrcChn.s32DevId = VoLayer;
    stSrcChn.s32ChnId = 0;

    stDestChn.enModId = HI_ID_VPSS;
    stDestChn.s32DevId = VpssGrp;
    stDestChn.s32ChnId = 0;

    s32Ret = HI_MPI_SYS_Bind(&stSrcChn, &stDestChn);
    if (s32Ret != HI_SUCCESS)
    {
        ZK_PRT("failed with %#x!\n", s32Ret);
        return HI_FAILURE;
    }

    return s32Ret;
}
#endif
static HI_U32 u32BlkSize = 0;
int HasCaptureInit = 0;

HI_S32 ZK_VIO_Init(HI_S32 chn)
{
    int s32Ret;
    ZK_VI_MODE_E enViMode = ZK_VI_MODE_8_1080P;
    VENC_CHN CaptureVencChn = 0;
    VPSS_GRP CaptureVpssGrp = 0;
    VI_CHN CaptureViChn = 0;
    VIDEO_NORM_E enNorm = VIDEO_ENCODING_MODE_AUTO;
    VB_CONF_S stVbConf;
#if 0
    memset(&stVbConf,0,sizeof(VB_CONF_S));
    //u32BlkSize = ZK_COMM_SYS_CalcPicVbBlkSize(enNorm,PIC_HD1080, ZK_PIXEL_FORMAT, ZK_SYS_ALIGN_WIDTH,COMPRESS_MODE_SEG);
    u32BlkSize = ZK_COMM_SYS_CalcPicVbBlkSize(VIDEO_ENCODING_MODE_PAL, PIC_HD1080, ZK_PIXEL_FORMAT, ZK_SYS_ALIGN_WIDTH, COMPRESS_MODE_SEG);
    stVbConf.u32MaxPoolCnt = 128;

    /* video buffer */
    //todo: vb=15
    stVbConf.astCommPool[0].u32BlkSize = u32BlkSize;
    stVbConf.astCommPool[0].u32BlkCnt = 8 * 8;

    /******************************************
      step 2: mpp system init. 
     ******************************************/
    s32Ret = ZK_COMM_SYS_Init(&stVbConf);
    if (HI_SUCCESS != s32Ret)
    {
        ZK_PRT("system init failed with %d!\n", s32Ret);
        //goto END_8_1080P_0;
    }
#endif

    if(HasCaptureInit == 0)
    {
        HasCaptureInit = 1;
        printf("startUserPic\n");
        startUserPic();
        sleep(1);
        int i;
        for(i = 0; i < 8; i++)
        {
            source_fmt[i] = FORMAT(SDI_INF, VI_SCAN_PROGRESSIVE, 25, PIC_HD1080);
        }
        int sfd = open("/dev/spi", O_RDWR);
        ioctl(sfd, GV7601_GET_FMT_ALL, source_fmt);
        //video_source_get(source_fmt);
    }

    printf("CaptureViChn:%d\n",CaptureViChn);

    VPSS_GRP_ATTR_S stGrpAttr;
    SIZE_S  stSize;

    /******************************************
      step  1: init variable 
     ******************************************/	
    /******************************************
      step 3: start vi dev & chn
     ******************************************/
     ZK_VI_SOURCE_ATTR SourceAttr;
    SourceAttr.ViFormat = source_fmt[CaptureViChn];
    //s32Ret = ZK_COMM_VI_Start_1(CaptureViChn,enViMode, enNorm);
    ZK_COMM_VI_Start(enViMode, enNorm, CaptureViChn, CaptureViChn*4, &SourceAttr);
    if (HI_SUCCESS != s32Ret)
    {
        ZK_PRT("start vi failed!\n");
        //goto END_8_1080P_0;
    }
    /******************************************
      step 4: start vpss and vi bind vpss
     ******************************************/
    s32Ret = ZK_COMM_SYS_GetPicSize(enNorm, PIC_HD1080, &stSize);
    if (HI_SUCCESS != s32Ret)
    {
        ZK_PRT("ZK_COMM_SYS_GetPicSize failed!\n");
        //goto END_8_1080P_1;
    }
    printf("wW:%d,Hh:%d\n",stSize.u32Width,stSize.u32Height);

    s32Ret = ZK_COMM_VPSS_Start_Webrtc(CaptureVpssGrp, &stSize, VPSS_MAX_CHN_NUM, NULL);
    if (HI_SUCCESS != s32Ret)
    {
        ZK_PRT("Start Vpss failed!\n");
        //goto END_8_1080P_1;
    }

    int vpssChn = 1;
    ZK_COMM_VI_BindVpss(CaptureViChn*4,CaptureVpssGrp);

    int profile = 0;
    s32Ret = ZK_COMM_VENC_Start(CaptureVencChn, VIDEO_ENCODING_MODE_PAL, \
              PIC_HD720, ZK_RC_CBR, PT_H264, \
              profile, 25, 25, \
              1, 1024);
    if (HI_SUCCESS != s32Ret)
    {
        ZK_PRT("Start Venc failed!\n");
    }

#if 0
    if(chn == 1)
    {
        s32Ret = ZK_COMM_VO_BindVpss(ZK_VO_LAYER_VHD1,/*CaptureVoChn*/0,CaptureVpssGrp,vpssChn);
        if (HI_SUCCESS != s32Ret)
        {
            ZK_PRT("ZK_COMM_VO_BindVpss:%x\n",s32Ret);
            // goto END_VENC_8_720p_3;
        }
        
    }
#endif
#if 1
    s32Ret = ZK_COMM_VENC_BindVpss(CaptureVencChn,CaptureVpssGrp,0/* VPSS_BSTR_CHN*/);
    if (HI_SUCCESS != s32Ret)
    {
        ZK_PRT("Start Venc failed!\n");
        // goto END_VENC_8_720p_3;
    }


    int VencFd = HI_MPI_VENC_GetFd(CaptureVencChn);
    if (VencFd < 0)
    {
        ZK_PRT("HI_MPI_VENC_GetFd failed with %#x!\n",
                VencFd);
    }

    printf("init vencfd:%d\n",VencFd);

    return VencFd;    

#endif
#if 0
    s32Ret = ZK_COMM_VENC_StartGetStream(2);
    if (HI_SUCCESS != s32Ret)
    {
        ZK_PRT("Start Venc failed!\n");
        //goto END_VENC_1HD_3;
    }
#endif

}


void ZkStopCaptureVideo()
{

}


/******************************************************************************
 * function    : main()
 * Description : video preview ZK
 ******************************************************************************/

VIDEO_FRAME_INFO_S frame;
unsigned char *yuv;
//unsigned char *h264;

static int fd = 0;

unsigned char *ZK_VI_GetH264Frame(int VencFd, int chn, int *sum)
{
    //int VencChn = 0;
    int s32Ret,offset,i=0;
    unsigned char nalType;
    unsigned char count;
    fd_set read_fds;
    int maxfd = VencFd;
    struct timeval TimeoutVal;
    unsigned char *h264 = NULL;
    VENC_CHN VencChn = 0;
    VENC_CHN_STAT_S stStat;
    VENC_STREAM_S stStream;
    int len = 640*480;
    if(!h264)
    {
        h264 = malloc(len + len/2);
        if(!h264)
        {
            return NULL;
        }
    }
    memset(h264,0,len+len/2);

    do{
        FD_ZERO(&read_fds);
        FD_SET(VencFd, &read_fds);    
        TimeoutVal.tv_sec  = 2;
        TimeoutVal.tv_usec = 0;
        s32Ret = select(maxfd + 1, &read_fds, NULL, NULL, &TimeoutVal);
        if (s32Ret < 0)
        {
            ZK_PRT("select failed!\n");
            break;
        }
        else if (s32Ret == 0)
        {
            ZK_PRT("get venc stream time out[%d]\n",VencFd);
            continue;
        }
        else
        {
            if (FD_ISSET(VencFd, &read_fds))
            {
                /*******************************************************
                  step 2.1 : query how many packs in one-frame stream.
                 *******************************************************/
                memset(&stStream, 0, sizeof(stStream));
                s32Ret = HI_MPI_VENC_Query(VencChn, &stStat);
                if (HI_SUCCESS != s32Ret)
                {
                    ZK_PRT("HI_MPI_VENC_Query chn[%d] failed with %#x!\n", i, s32Ret);
                    break;
                }
                /*******************************************************
                  step 2.2 : suggest to check both u32CurPacks and u32LeftStreamFrames at the same time£¬for example:
                  if(0 == stStat.u32CurPacks || 0 == stStat.u32LeftStreamFrames)
                  {
                  ZK_PRT("NOTE: Current  frame is NULL!\n");
                  continue;
                  }
                 *******************************************************/
                if(0 == stStat.u32CurPacks)
                {

                    ZK_PRT("NOTE: Current  frame is NULL!\n");
                    continue;
                }
                /*******************************************************
                  step 2.3 : malloc corresponding number of pack nodes.
                 *******************************************************/
                stStream.pstPack = (VENC_PACK_S*)malloc(sizeof(VENC_PACK_S) * stStat.u32CurPacks);
                if (NULL == stStream.pstPack)
                {
                    ZK_PRT("malloc stream pack failed!\n");
                    break;
                }

                /*******************************************************
                  step 2.4 : call mpi to get one-frame stream
                 *******************************************************/
                stStream.u32PackCount = stStat.u32CurPacks;
                s32Ret = HI_MPI_VENC_GetStream(VencChn, &stStream, HI_TRUE);
                if (HI_SUCCESS != s32Ret)
                {
                    free(stStream.pstPack);
                    stStream.pstPack = NULL;
                    ZK_PRT("HI_MPI_VENC_GetStream failed with %#x!\n", \
                            s32Ret);
                    break;
                }


                /*******************************************************
                  step 2.5 : handler h264 frame
                 *******************************************************/
#if 1
                 if((fd == 0) && IsTeacher() && getRecord() && (chn == 0))
                 {
                   char filename[128] = {};
                   sprintf(filename,"/mnt/webrtc/teacher_cap.h264");
                   fd = open(filename,O_CREAT | O_WRONLY | O_TRUNC,0666);    
                 }

                 if((fd == 0) && (!IsTeacher()) && getRecord() && (chn == 0))
                 {
                   char filename[128] = {};
                   sprintf(filename,"/mnt/webrtc/student_cap.h264");
                   fd = open(filename,O_CREAT | O_WRONLY | O_TRUNC,0666);    
                 }
#endif
        
                offset = 5;
                memcpy(h264,&chn,1);
                count = stStream.u32PackCount;
                //printf("count:%d\n",count);
                memcpy(h264+offset,&count,1);
                offset += 1;
                for (i = 0; i < stStream.u32PackCount; i++)
                {
                    len = stStream.pstPack[i].u32Len;
                    nalType = stStream.pstPack[i].DataType.enH264EType;
                    //                printf("vi[%d] %d\n",offset,len);
                    memcpy(h264+offset,&len,4);
                    offset += 4;
                    memcpy(h264+offset,&nalType,1);
                    offset += 1;
                    memcpy(h264+offset,stStream.pstPack[i].pu8Addr,len);
#if 1
                 if(fd && getRecord() && (chn == 0))
                 {
                     write(fd,stStream.pstPack[i].pu8Addr,len);
                 }

#endif
                    offset += len;
                }
                memcpy(h264+1,&offset,4);
                *sum = offset;
                //                printf("get h264 stream %d...\n",offset);

                /*******************************************************
                  step 2.6 : release stream
                 *******************************************************/
                s32Ret = HI_MPI_VENC_ReleaseStream(VencChn, &stStream);
                if (HI_SUCCESS != s32Ret)
                {
                    free(stStream.pstPack);
                    stStream.pstPack = NULL;
                    break;
                }
                /*******************************************************
                  step 2.7 : free pack nodes
                 *******************************************************/
                free(stStream.pstPack);
                stStream.pstPack = NULL;
#if 0
                if(chn == 0)
                {
                printf("chn:%d\n",chn);
                printf("u32LeftPics:%d\n",stStat.u32LeftPics);
                printf("u32LeftStreamBytes:%d\n",stStat.u32LeftStreamBytes);
                printf("u32LeftStreamFrames:%d\n",stStat.u32LeftStreamFrames);
                printf("u32LeftRecvPics:%d\n",stStat.u32LeftRecvPics);
                printf("u32LeftEncPics:%d\n",stStat.u32LeftEncPics);
#if 0
                int frm = 0;
                HI_MPI_VO_GetChnFrameRate(ZK_VO_LAYER_VIRT0,0, &frm);
                printf("VO frmrate[%d]:%d\n",0,frm);
                frm = 0;
                HI_MPI_VO_GetChnFrameRate(ZK_VO_LAYER_VIRT0,1, &frm);
                printf("VO frmrate[%d]:%d\n",1,frm);
                VPSS_FRAME_RATE_S pstVpssFrameRate;
                memset(&pstVpssFrameRate,0,sizeof(pstVpssFrameRate));
                if(chn == 0) {
                    HI_MPI_VPSS_SetGrpFrameRate(10,&pstVpssFrameRate);
                    printf("vpss s32SrcFrmRate:%d\n",pstVpssFrameRate.s32SrcFrmRate);
                    printf("vpss s32DstFrmRate:%d\n",pstVpssFrameRate.s32DstFrmRate);
                }
#endif
                //if((stStat.u32LeftStreamFrames > 5) && !key)
                //    continue;
                }
#endif
                break;

            }
        }
    }while(1);

    return h264;
}

unsigned char *ZK_VI_GetYUV420Frame()
{
    HI_S32 s32Ret;//i;
    unsigned char *y,*u;//*v;
    int len = 640*480;
    if(!yuv)
    {
        yuv = malloc(len + len/2);
        if(!yuv)
        {
            return NULL;
        }
    }
    memset(yuv,0,len+len/2);
    memset(&frame,0,sizeof(frame));
    //s32Ret = HI_MPI_VI_GetFrame(0,&frame,-1);
    s32Ret = HI_MPI_VPSS_GetChnFrame(0,0,&frame,-1);
    if(s32Ret != HI_SUCCESS)
    {
        printf("get vi frame err:0x%x\n",s32Ret);
        return NULL;
    }
    y = (HI_U8 *)HI_MPI_SYS_Mmap(frame.stVFrame.u32PhyAddr[0],len);
    u = (HI_U8 *)HI_MPI_SYS_Mmap(frame.stVFrame.u32PhyAddr[1],len/2);
    //v = (HI_U8 *)HI_MPI_SYS_Mmap(frame.stVFrame.u32PhyAddr[2],len/4);
    memcpy(yuv,y,len);
    memcpy(yuv+len,u,len/2);

    HI_MPI_SYS_Munmap(y,len);
    HI_MPI_SYS_Munmap(u,len/2);
    //	HI_MPI_SYS_Munmap(v,len/4);
    //	printf("%d %d %d\n",frame.stVFrame.u32Width,frame.stVFrame.u32Height,frame.stVFrame.enPixelFormat);
    //printf("header:%d %d %d\n",frame.stVFrame.u32HeaderStride[0],frame.stVFrame.u32HeaderStride[1],frame.stVFrame.u32HeaderStride[2]);
    HI_MPI_VPSS_ReleaseChnFrame(0,0,&frame);
    //HI_MPI_VI_ReleaseFrame(0,&frame);
    return yuv;
}


void *capture(void *arg)
{
    HI_S32 fd, sum;
    unsigned char *addr;
    fd = ZK_VIO_Init(0);
    if(fd < 0)
    {
        printf("error\n");
    }
    int interaction = CreatTransmitClient(INTERACTION_STREAM);
    int vga = CreatTransmitClient(VGA_STREAM);
    while(1) {
        sum = 0;
        addr = ZK_VI_GetH264Frame(fd,0,&sum);
        write(interaction,addr,sum);
        write(vga,addr,sum);
        free(addr);
    }    
}

#if 1
int main()
{
    int max = 0;
    fd_set fds;
    pthread_t pid;
    int remote[3] = {};
    unlink(REMOTE0_STREAM);
    unlink(REMOTE1_STREAM);
    unlink(REMOTE2_STREAM);
    pthread_create(&pid,NULL,capture,NULL);
    //int control = CreatTransmitClient(CONTROL_MESSAGE);
    remote[0] = CreatTransmitServer(REMOTE0_STREAM);
    max = remote[0];
    remote[1] = CreatTransmitServer(REMOTE1_STREAM);
    if(max < remote[1])
        max = remote[1];
    remote[2] = CreatTransmitServer(REMOTE2_STREAM);
    if(max < remote[2])
        max = remote[2];
    max ++;


    //int local_audio = CreatTransmitClient(LOCAL_AUDIO_STREAM);
    //int remote_audio = CreatTransmitClient(REMOTE0_STREAM);
    int ret;
    int i,rbytes;
    unsigned char buff[102400];
    while(1)
    {
        FD_ZERO(&fds);
        FD_SET(remote[0],&fds);
        FD_SET(remote[1],&fds);
        FD_SET(remote[2],&fds);
        ret = select(max,&fds,NULL,NULL,NULL);
        if(ret < 0) {
            perror("select");
            break;
        } else if(ret == 0) {
            continue;    
        } else {
            for(i=0;i<3;i++) {
                if(FD_ISSET(remote[i],&fds)) {
                    rbytes = read(remote[i],buff,102400);
                    printf("get %d rbytes %d\n",i,rbytes);    
                }    
            }                  
        }
    }
    return 0;
}
#endif


#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* End of #ifdef __cplusplus */

