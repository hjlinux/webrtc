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

#include "zk_comm.h"
#include "resource_note.h"
#include "param_conf.h"
#include "message_queue.h"
#include "display_arm.h"


void StopCapture2(int chn)
{
    printf("StopCapture\n");
}

int ZK_VIO_Init(int chn)
{
    printf("ZK_VIO_Init\n");
    return -1;
}

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* End of #ifdef __cplusplus */

