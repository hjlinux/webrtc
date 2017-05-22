/******************************************************************************* 
  Copyright (C), 1993-2013, Zonekey Tech. Co., Ltd.

 ******************************************************************************
 File Name     : zk_class_interface.h
Version       : Initial Draft
Author        : zhengm
Created       : 2017/03/08
Description   : 
 ******************************************************************************/

#ifndef __ZK_CLASS_INTERFACE_H__
#define __ZK_CLASS_INTERFACE_H__

#ifdef __cplusplus
extern "C"
{
#endif

#define PACKET_LEN_BYTES 4
#define MAX_TEACHER_RECEIVE_NUM 4
#define MAX_TEACHER_SEND_NUM 4
#define MAX_STUDENT_RECEIVE_NUM 3
#define MAX_STUDENT_SEND_NUM 2


    /************** teacher receive *******************/
#define STUDENT_REMOTE0_STREAM    "/zqfs/etc/localsocket/student_0_rv"
#define STUDENT_REMOTE1_STREAM    "/zqfs/etc/localsocket/student_1_rv"
#define STUDENT_REMOTE2_STREAM    "/zqfs/etc/localsocket/student_2_rv"
#define STUDENT_AUDIO_REMOTE_STREAM    "/zqfs/etc/localsocket/student_012_ra"

    /************** teacher send *******************/
#define TEACHER_LOCAL_STREAM    "/zqfs/etc/localsocket/teacher+_sv"
#define VGA_LOCAL_STREAM    "/zqfs/etc/localsocket/vga_sv"
//#define INTERACTION_LOCAL_STREAM    "/zqfs/etc/localsocket/pgm_sv"
#define TEACHER_AUDIO_LOCAL_STREAM    "/zqfs/etc/localsocket/teacher_sa"

    /************** student receive *******************/
#define TEACHER_REMOTE_STREAM    "/zqfs/etc/localsocket/teacher+_rv"
#define VGA_REMOTE_STREAM    "/zqfs/etc/localsocket/vga_rv"
#define TEACHER_AUDIO_REMOTE_STREAM    "/zqfs/etc/localsocket/t+2s_ra"

    /************** student send *******************/
#define STUDENT_LOCAL_STREAM    "/zqfs/etc/localsocket/student_sv"
#define STUDNET_AUDIO_LOCAL_STREAM    "/zqfs/etc/localsocket/student_sa"



typedef struct {
    int teacherLocalFd;
    int vgaLocalFd;
    int interactionLocalFd;
    int teacherAudioLocalFd;

    int studentLocalFd;
    int studentAudioLocalFd;

    int studentRemoteFd[3];
    int studentAudioRemoteFd;
    
    int teacherRemoteFd;
    int vgaRemoteFd;
    int teacherAudioRemoteFd;
}__attribute__((__packed__)) ONLINE_CLASS_INTERFACE;

typedef enum {
    ROLE_TEACHER,
    ROLE_STUDENT,
}ROLE_TYPE_E;

typedef enum {
    ONLINE_CLASS_STOP,
    ONLINE_CLASS_START,
}ONLINE_CLASS_STATE_E;


int writeN(int fd,void *data, int len);
int readN(int fd,void *data, int len);
int getCaptureFd(int chn);
int getRemoteFd(int chn);
int getRemoteAudioFd();
void setTeacherAcrossBind(int val);
int getTeacherAcrossBind();
void online_class_init();
int getAudioInFd();
void online_class_release();
#ifdef __cplusplus
}
#endif

#endif

