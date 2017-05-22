#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "resource_note.h"
#include "zk_comm.h"
#include "param_conf.h"
#include "h264_dec_arm.h"
#include "class_interface.h"


static VIDEO_PARAM_S viVideoParam[] = {
    {"",  ZK_VI_MODE_8_1080P,  0, 0, 0, 0, -1, 0, 0}, // SDI TEACHER
    {"",      ZK_VI_MODE_8_1080P,  1, 1, 1, 1, -1, 1, 0}, // SDI STUDENTS 
};

static VIDEO_PARAM_S vdecVideoParam[] = {
    {"", ZK_VI_MODE_8_1080P,  -1, 2, -1, -1, 0, 0, 0}, // SDI TEACHER
    {"", ZK_VI_MODE_8_1080P,  -1, 3, -1, -1, 1, 1, 0}, // SDI STUDENTS 
    {"", ZK_VI_MODE_8_1080P,  -1, 4, -1, -1, 2, 2, 0}, // SDI STUDENTS 
};


char *GetCaptureName(int chn)
{
    return viVideoParam[chn].name;    
}

char *GetRemoteName(int chn)
{
    return vdecVideoParam[chn].name;    
}

int GetClientVoChn(int clientChn)
{
    return vdecVideoParam[clientChn].voChn;
}

int GetCaptureChn()
{
    HI_S32 i;
    for(i=0; i<2; i++)
    {
        if(0 == viVideoParam[i].play)
        {   
            viVideoParam[i].play = 1;
            return i;
        }
    }
    return -1;
}

int GetClientChn()
{
    int i;
    for(i=0; i<MAX_CLIENT_CNT; i++)
    {
        if(vdecVideoParam[i].play == 0)
        {
            vdecVideoParam[i].play = 1;
            break;    
        }
    }
    printf("GetClientChn:%d\n",i);
    if(i >= MAX_CLIENT_CNT)
        return -1;
    else
        return i;
}


void ClientExit(int clientChn)
{
    if((clientChn < MAX_CLIENT_CNT) && (clientChn >= 0))
    {
        vdecVideoParam[clientChn].play = 0;
        vdecVideoParam[clientChn].voChn = -1;
    }
}

void SetDisplayInfo(int clientChn,char *name,int videoChn,int disp_pos_)
{
    int i;
    int voMaxChn = -1;
    printf("nt clientChn:%d,char *name:%s,int videoChn:%d\n",clientChn,name,videoChn);
    if((clientChn < 0) || ((clientChn >=0) && (getTeacherAcrossBind() >= 0)))
    {
        return;
    }

    int flag = (clientChn == videoChn)?0:1;
    setTeacherAcrossBind(flag);
}



