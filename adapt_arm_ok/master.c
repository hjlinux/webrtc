#include <stdio.h>
#include <stdlib.h> 
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/msg.h>
#include "zk_msg_type.h"
#include "zk_msg_process.h"

static int msgRecv,msgSnd,msgWebrtc;
//#define INTERACTION_BIN "/mnt/webrtc/worker"
#define INTERACTION_BIN "/zqfs/bin/worker"
#define INTERACTION_SERVER_BIN "/zqfs/bin/interaction_server"
#define TMP_PROCESS_ID			"/tmp/workerid"

static unsigned long long getFileSize(const char *fileName)
{
    struct stat fileInfo;
    unsigned long long fileSize;

    if(lstat(fileName, &fileInfo) < 0)
        fileSize = 0;
    else
        fileSize = fileInfo.st_size;

    return fileSize;
}


int Run_Process(char *bin)
{
    char cmd[256] = {0};
    //    printf("%s\n",bin);

    sprintf(cmd, "pgrep %s > %s", bin, TMP_PROCESS_ID);
    system(cmd);
    unsigned long long fileSize = getFileSize(TMP_PROCESS_ID);
    memset(cmd, 0, sizeof(cmd));
    sprintf(cmd, "rm %s", TMP_PROCESS_ID);
    system(cmd);
    if(fileSize > 0) {
        //printf("%s has been started\n", pname);
        return 0;
    }

    memset(cmd, 0, sizeof(cmd));
    sprintf(cmd,"%s &",bin);
    system(cmd);
    printf("cmd: %s\n", cmd);

    return 1;
}



int msg_create(int *msgid, key_t key)
{
    if((*msgid = msgget(key, IPC_CREAT | 0666)) < 0)
        return -1; 
    return 0;
}

int msg_webrtc_create(int *msgidp)
{
    int msgid = msgget((key_t)1234, 0666 | IPC_CREAT | IPC_EXCL);
    printf("msgget:%d\n",msgid);
    if(msgid == -1)
    {
        if (errno == EEXIST)
        {
            int ret = 0;
            char msgp[1400];
            msgid = msgget((key_t)1234, 0666 | IPC_CREAT);  
            if(msgid < 0) {
                *msgidp = msgid;
                return msgid;    
            }   
            while(1) {
                ret = msgrcv(msgid, msgp, 1400-4,0,IPC_NOWAIT);
                if((ret < 0) && (errno == ENOMSG))
                    break;
            }   
            *msgidp = msgid;
            return msgid;
        }
    }
    *msgidp = msgid;
    return msgid;
}

int RECV(int msgid, int *msgType, void *msgBuf)
{
    MSG_INFO_S msgInfo;
    int msgLen = -1;

    if(msgType == NULL || msgBuf == NULL)
        return msgLen;

    memset(&msgInfo, 0, sizeof(MSG_INFO_S));
    if((msgLen = msgrcv(msgid, &msgInfo, MSG_LEN, 0, MSG_NOERROR)) == -1)
        return msgLen;

    *msgType = msgInfo.msgType;
    if(msgLen > 0)
        memcpy(msgBuf, msgInfo.msgText, msgLen);

    return msgLen;
}

int SEND(int msgid, int msgType, void *msgBuf, int msgLen)
{
    MSG_INFO_S msgInfo;
    int s32Ret = -1;

    memset(&msgInfo, 0, sizeof(MSG_INFO_S));
    msgInfo.msgType = msgType;
    if(msgBuf != NULL && msgLen > 0)
        memcpy(msgInfo.msgText, msgBuf, msgLen);
    s32Ret = msgsnd(msgid, &msgInfo, msgLen, 0);
    printf("%d\n",s32Ret);
    perror("");

    return s32Ret;
}

int message_queue_init(void)
{
    msg_create(&msgRecv,CLASS_KEY_OUT);
    msg_create(&msgSnd,CLASS_KEY_IN);
    msg_webrtc_create(&msgWebrtc);

    return 0;
}

int RecorderMsgHandler()
{
    int msgLen;
    int msgType;
    unsigned char msgText[MSG_LEN];
    int ret = -1;
    msgLen = RECV(msgRecv,&msgType,msgText);
    if(msgLen < 0)
        return -1;
    switch(msgType) 
    {
        case MSG_CLASS_START:
            printf("MSG_CLASS_START\n");
            ret = 1;
            break; 
        case MSG_CLASS_STOP:
            printf("MSG_CLASS_STOP\n");
            ret = 2;
            break; 
    }
    return ret;
}

void *check_interaction_bin(void *arg)
{
    pthread_detach(pthread_self());
    sleep(5);
    SEND(msgWebrtc,3,"list_test",9);
    pthread_exit(NULL);
}

void *monitor(void *arg)
{
    pthread_detach(pthread_self());
    while(1) {
#if 0
        if(Run_Process(INTERACTION_SERVER_BIN)) {
        } 
        sleep(1);
#endif
#if 1
        if(Run_Process(INTERACTION_BIN)) {
            SEND(msgSnd,MSG_CLASS_STOP,NULL,0);
            printf("MSG_CLASS_STOP\n");
            //pthread_t pid;
            //pthread_create(&pid,NULL,check_interaction_bin,NULL);
        } 
#endif
        sleep(1);
    }    
}

int main()
{
    int ret;
    system("pkill worker");
    sleep(1);
    message_queue_init();
    pthread_t pid;
    pthread_create(&pid,NULL,monitor,NULL);
#if 0
    SEND(msgSnd,MSG_CLASS_STOP,NULL,0);
    while(1) {
        ret = RecorderMsgHandler();
        switch(ret) {
            case 1:
                printf("msgid:%d\n",msgWebrtc);
                SEND(msgWebrtc,3,"list_start",10);
                break;
            case 2:
                SEND(msgWebrtc,3,"list_end",8);
                SEND(msgSnd,MSG_CLASS_STOP,NULL,0);
                //usleep(500000);
                //system("pkill worker");
                //system("pkill interaction_server");
                break;
        }
    }
#else
    while(1) {
        sleep(5);    
    }
#endif
    return 0;    
}
