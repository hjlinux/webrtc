#include <stdio.h>
#include <stdlib.h> 
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/msg.h>
#include <netinet/in.h>
#include "h264_dec_arm.h"
#include "display_arm.h"
#include "resource_note.h"
#include "class_socket.h"
#include "class_interface.h"
#include "param_conf.h"
#include "zk_msg_type.h"
#include "zk_config.h"

static int BitRate = 0;

#define LISTEN_BACKLOG  32
#define MSG_LEN	10240
typedef struct zkMSG_INFO_S
{
	long int msgType;
	char msgText[MSG_LEN];
}MSG_INFO_S;

static int msgRecv,msgSnd,msgWebrtc;

int msg_create(int *msgid, key_t key)
{
    if((*msgid = msgget(key, IPC_CREAT | 0666)) < 0)
        return -1; 
    return 0;
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

int RECV2(int *msgType, void *msgBuf)
{
    MSG_INFO_S msgInfo;
    int msgLen = -1;
    int msgid = msgRecv;

    if(msgType == NULL || msgBuf == NULL)
        return msgLen;

    memset(&msgInfo, 0, sizeof(MSG_INFO_S));
    if((msgLen = msgrcv(msgid, &msgInfo, MSG_LEN, 0, MSG_NOERROR)) == -1)
        return msgLen;

    //*msgType = msgInfo.msgType;
    switch(msgInfo.msgType)
    {
        case MSG_CLASS_START:    
            *msgType = 1;
            break;
        case MSG_CLASS_STOP:    
            *msgType = 2;
            break;
    }

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

    return s32Ret;
}

#ifdef X86_TEST
int SendClient(char *cmd)
{
    int ret;
    struct sockaddr_in serveraddr;
    int sock = socket(AF_INET,SOCK_STREAM,0);
    if(sock < 0)
    {
        perror("socket");
        return -1;    
    }
    memset(&serveraddr,0,sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_port = htons(1230);
    inet_aton(RECORDER_IP,&serveraddr.sin_addr);
    ret = connect(sock,(struct sockaddr *)&serveraddr,sizeof(serveraddr));
    if(ret < 0)
    {
        perror("connect");
        return -1;    
    }
    ret = write(sock,cmd,strlen(cmd));
    close(sock);
    return 0;
}
#endif

int CreateServer(unsigned short port)
{
    int ret;
    struct sockaddr_in serveraddr;
    int sock = socket(AF_INET,SOCK_STREAM,0);
    if(sock < 0)
    {
        perror("socket");
        return -1;    
    }
    memset(&serveraddr,0,sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_port = htons(port);
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);

    int yes = 1;
    setsockopt(sock,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(yes));

    int socklen = sizeof(serveraddr);
    ret = bind(sock,(struct sockaddr *)&serveraddr,socklen);
    if(ret < 0)
    {
        perror("bind");
        close(sock);
        return -1;    
    }

    ret = listen(sock,LISTEN_BACKLOG);
    if(ret < 0)
    {
        perror("listen");
        close(sock);
        return -1;    
    }
    return sock;
}


typedef struct ClientInf_s
{
    int clientsock;
    pthread_t pid;
}ClientInf_t;

#define CLIENT_BUFF_LEN 1024*8

void closeClient(ClientInf_t *clientInf)
{
    if(clientInf)
    {
        close(clientInf->clientsock);
        free(clientInf);
    }
}


void ClassBeginCmd(char *args)
{
    printf("RecoreCMd=ClassBegin:%s\n",args); 
}

void SwitchClassCmd(char *args)
{
    printf("%s",args);
    int class = 0;
    char *p = strstr(args,"class=");
    if(!p)
        return;
    p += 6;
    class = atoi(p);
    printf("SwitchClass:%d\n",class);
    if(class < 0) {
        //        SwitchInteractionMode(LECTURE);
    }
    else
    {
        //        SwitchInteractionMode(INTERACTION);
        //        SwitchClass(class);
    }
}

int record;
int getRecord(void)
{
    return record;
}

void StartRecordCmd(char *args)
{
    record = atoi(args);
    printf("StartRecordCmd:%d\n",record);
}

void StartInteraction()
{
    int role;
#ifdef X86_TEST
    char cmd[256] = {};
    sprintf(cmd,"ClassCmd=StartClass&RoleType=%s\r\n",IsTeacher()?"Teacher":"Student");
    printf("%s",cmd);
    SendClient(cmd);
#else
    if(IsTeacher()) {
        role = ROLE_TEACHER;    
    } else {
        role = ROLE_STUDENT;    
    }
    printf("ClassCmd=StartClass&RoleType=%s\r\n",IsTeacher()?"Teacher":"Student");
    ST_MSG_CLASS_START msg1;
    msg1.roleType = role;
    SEND(msgSnd,MSG_CLASS_START,&msg1,sizeof(msg1));
#endif
   /* 
    ST_MSG_CLASS_SWITCH_MODE msg2;
    msg2.classMode = MODE_LECTURE;
    msg2.studentId = 0;
    SEND(msgSnd,MSG_CLASS_SWITCH_MODE,&msg2,sizeof(msg2));
    */
}

void StopInteraction3()
{
#ifdef X86_TEST
    SendClient("ClassCmd=StopClass\r\n");
#else
    SEND(msgSnd,MSG_CLASS_STOP,NULL,0);    
#endif
}

void StopInteraction2()
{
#ifdef X86_TEST
    SendClient("ClassCmd=StopClass\r\n");
#else
    SEND(msgSnd,MSG_CLASS_STOP,NULL,0);
    SEND(msgRecv,MSG_CLASS_STOP,NULL,0);
    online_class_release();
    //raise(SIGINT);
    //_exit(0);
//    exit(0);
#endif
}

void StopInteraction()
{
    setStudent();
#ifdef X86_TEST
    SendClient("ClassCmd=StopClass\r\n");
    online_class_release();
#else
    SEND(msgSnd,MSG_CLASS_STOP,NULL,0);
    if(!IsTeacher()) {
        SEND(msgRecv,MSG_CLASS_STOP,NULL,0);
    }
    online_class_release();
    //raise(SIGINT);
    //_exit(0);
//    exit(0);
#endif
}

void SetBitrate(int bitrate)
{
    if(BitRate == bitrate)
        return;
    printf("Current bitrate:%d\n",bitrate);
    ST_MSG_CLASS_MODIFY_BITRATE msg;
    msg.bitRate = bitrate;
    BitRate = bitrate;
    SEND(msgSnd,MSG_CLASS_MODIFY_BITRATE,&msg,sizeof(msg));
}

void InteractionMode2(char *args)
{
    printf("%s",args);
    int mode = 0;
    char *p = strstr(args,"mode=");
    if(!p)
        return;
    p += 5;
    mode = *p - '0';
    printf("InteractionMode:%d\n",mode);
    //    SwitchInteractionMode(mode);
#if 0
    if(mode == 0) {
        int role;
        if(IsTeacher()) {
            role = ROLE_TEACHER;    
        } else {
            role = ROLE_STUDENT;    
        }
        ST_MSG_CLASS_START msg1;
        msg1.roleType = role;
        SEND(msgSnd,MSG_CLASS_START,&msg1,sizeof(msg1));
    } else {
#if 1
        ST_MSG_CLASS_SWITCH_MODE msg2;
        if(mode == 1) { 
            msg2.classMode = MODE_LECTURE;
        } else if(mode == 2) {
            msg2.classMode = MODE_INTERACTION;//MODE_LECTURE;
        }
        msg2.studentId = 0;
        printf("MSG_CLASS_SWITCH_MODE\n");
        SEND(msgSnd,MSG_CLASS_SWITCH_MODE,&msg2,sizeof(msg2));
#endif
    }
#endif
    
}

struct _cmd_line_
{
    char cmd[64];
    void (*handler)(char *args);
} CmdLine[] = {
    {"RecordCmd=ClassBegin",ClassBeginCmd},
    {"RecordCmd=SwitchClass",SwitchClassCmd},
    {"RecordCmd=StartRecord",StartRecordCmd},
    {"RecordCmd=InteractionMode",InteractionMode2},
    {"",NULL}, 
};

void *handler(void *arg)
{
    int time_out = 0, i = 0;
    int bytes = 0, offset = 0;
    char buff[CLIENT_BUFF_LEN] = {};
    char *p;
    ClientInf_t *clientInf = (ClientInf_t *)arg;
    pthread_detach(pthread_self());
    while(1)
    {
        bytes = read(clientInf->clientsock,buff+offset,CLIENT_BUFF_LEN);
        if(bytes < 0)
        {
            if((errno == EINTR) || ((errno == EAGAIN) && ((time_out++) < 10)))
            {
                continue;
            }
            printf("read:%s[%d]\n",strerror(errno),errno);
            closeClient(clientInf); 
            return NULL;
        }
        else if(bytes == 0)
        {
            printf("read:%s[%d]\n",strerror(errno),errno);
            closeClient(clientInf); 
            return NULL;
        }

        offset += bytes;
        if(buff[offset-1] == '\n')
        {
            break;    
        }
    }
    p = strchr(buff,'?');
    if(!p)
    {
        closeClient(clientInf); 
        return NULL;
    }
    *p = '\0';

    while(CmdLine[i].handler)
    {
        if(0 == strcmp(CmdLine[i].cmd,buff))
        {
            CmdLine[i].handler(p+1);
            break;
        }
        i ++;
    }
    i ++;
    if(i >= sizeof(CmdLine)/sizeof(struct _cmd_line_))
    {
        int ret = write(clientInf->clientsock,"unsuport cmd\n",strlen("unsuport cmd\n"));   
    }
    closeClient(clientInf); 
    return NULL;
}

void *ServerHandler(void *arg)
{
    int sock = *(int *)arg;
    int newsock, flags,ret;
    ClientInf_t *clientInf = NULL;
    struct sockaddr_in clientaddr;
    socklen_t socklen = sizeof(clientaddr);
    while(1)
    {
        memset(&clientaddr,0,sizeof(clientaddr));
        newsock = accept(sock,(struct sockaddr *)&clientaddr,&socklen);
        if(newsock < 0)
        {
            perror("accept");
            continue;    
        }
        //flags = fcntl(newsock, F_GETFL, 0);
        //fcntl(newsock, F_SETFL, flags|O_NONBLOCK);

        clientInf = (ClientInf_t *)malloc(sizeof(ClientInf_t));
        //     printf("newsock:%d\n",newsock);
        if(clientInf)
        {
            clientInf->clientsock = newsock;
            ret = pthread_create(&clientInf->pid,NULL,handler,(void *)clientInf);
            if(ret < 0)
            {
                printf("pthread_create:%s\n",strerror(ret));
                close(newsock);    
            }
        }
        else
        {
            close(newsock);    
        }
    }
}

int servsock;

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

int message_queue_init(void)
{
    pthread_t pid;
    servsock = CreateServer(1250);
    if(servsock < 0)
    {
        return -1;    
    }

    msg_create(&msgRecv,CLASS_KEY_OUT);
    msg_create(&msgSnd,CLASS_KEY_IN);
    msg_create(&msgWebrtc,(key_t)1234);

    int ret = pthread_create(&pid,NULL,ServerHandler,(void *)&servsock);
    if(ret < 0)
    {
        printf("pthread_create:%s\n",strerror(ret));
        close(servsock);    
    }
    return 0;
    //ServerHandler(servsock);    
}
/*
   int main()
   {
   message_queue_init();
   while(1) sleep(1);
   return 0;    
   }*/

