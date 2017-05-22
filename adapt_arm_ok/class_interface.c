#include <stddef.h>
#include "zk_config.h"
#include "class_socket.h"
#include "class_interface.h"
#include "param_conf.h"


static ONLINE_CLASS_INTERFACE onClassInterface;
ONLINE_CLASS_INTERFACE *pClassInterface = &onClassInterface;
static int teacherAcrossBind = -1;

typedef struct {
    int listenFd;
    int *pfd;    
}LISTEN_S;

//static interactionMode = MODE_LECTURE;

void setSockTimeOut(int socket, int timeout)
{
    struct timeval tv = {timeout, 0}; 

    if (setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof(tv))) {
        printf("Setting socket rcv timeout to %ds failed!", timeout);
    }   
    if (setsockopt (socket, SOL_SOCKET, SO_SNDTIMEO, (char *)&tv, sizeof(tv))) {
        printf("Setting socket snd timeout to %ds failed!", timeout);
    }   
}


bool makeSocketNonBlocking(int sock)
{
    int curFlags = fcntl(sock, F_GETFL, 0);
    return fcntl(sock, F_SETFL, curFlags|O_NONBLOCK) >= 0;
}

static void delSocket(int socket)
{
    if(socket != -1) {
        closeSocket(socket);
        socket = -1;
    }
}

int readN(int fd,void *data, int len)
{
    int rbytes,offset = 0;
    while(1) {
        rbytes = read(fd,data+offset,len-offset);
        if(rbytes < 0) {
            printf("read,fd[%d],ret[%d],errno[%d]\n",fd,rbytes,errno);
            if((errno == EAGAIN) || (errno == EINTR) || (errno == EWOULDBLOCK))
                continue;
            return -1;    
        } else if(rbytes == 0) {
            printf("read,fd[%d],ret[%d],errno[%d]\n",fd,rbytes,errno);
            return 0;    
        }
        offset += rbytes;
        if(offset == len)
            return offset;
    }
}

int writeN(int fd,void *data, int len)
{
    int wbytes,offset = 0;
    while(1) {
        wbytes = write(fd,data+offset,len-offset);
        if(wbytes <= 0) {
            perror("writeN");
            printf("write,fd[%d],ret[%d],errno[%d]\n",fd,wbytes,errno);
            if((errno == EAGAIN) || (errno == EINTR) |(errno == EWOULDBLOCK))
                continue;
            return -1;    
        }
        offset += wbytes;
        if(offset == len)
            return offset;
    }
}

static char *student_local_path[] = {STUDENT_REMOTE0_STREAM,STUDENT_REMOTE1_STREAM,STUDENT_REMOTE2_STREAM};

void setTeacherAcrossBind(int val)
{
    printf("setTeacherAcrossBind:%d\n",val);
    teacherAcrossBind = val;    
}
int getTeacherAcrossBind()
{
    return teacherAcrossBind;    
}

int getRemoteFd(int chn)
{
    if(IsTeacher()) {
        if(pClassInterface->studentRemoteFd[chn] > 0) {
            return pClassInterface->studentRemoteFd[chn]; 
        } else {
            pClassInterface->studentRemoteFd[chn] = setupLTCPCSocket(student_local_path[chn]);
            return pClassInterface->studentRemoteFd[chn];    
        }
    } else {
        if(teacherAcrossBind < 0)
            return 0;
        if(teacherAcrossBind > 0) {
            if(chn == 1) {
                if(pClassInterface->teacherRemoteFd > 0) {
                    return pClassInterface->teacherRemoteFd;    
                } else {
                    pClassInterface->teacherRemoteFd = setupLTCPCSocket(TEACHER_REMOTE_STREAM);    
                    return pClassInterface->teacherRemoteFd;
                }
            } else {
                if(pClassInterface->vgaRemoteFd > 0) {
                    return pClassInterface->vgaRemoteFd;    
                } else {
                    pClassInterface->vgaRemoteFd = setupLTCPCSocket(VGA_REMOTE_STREAM);    
                    return pClassInterface->vgaRemoteFd;
                }
            }
        } else {
            if(chn == 0) {
                if(pClassInterface->teacherRemoteFd > 0) {
                    return pClassInterface->teacherRemoteFd;    
                } else {
                    pClassInterface->teacherRemoteFd = setupLTCPCSocket(TEACHER_REMOTE_STREAM);    
                    return pClassInterface->teacherRemoteFd;
                }
            } else {
                if(pClassInterface->vgaRemoteFd > 0) {
                    return pClassInterface->vgaRemoteFd;    
                } else {
                    pClassInterface->vgaRemoteFd = setupLTCPCSocket(VGA_REMOTE_STREAM);    
                    return pClassInterface->vgaRemoteFd;
                }
            }
        }
    }
}

int getRemoteAudioFd() 
{
    if(IsTeacher()) {
        if(pClassInterface->studentAudioRemoteFd > 0) {
            return pClassInterface->studentAudioRemoteFd;
        } else {
            pClassInterface->studentAudioRemoteFd = setupLTCPCSocket(STUDENT_AUDIO_REMOTE_STREAM);          
            return pClassInterface->studentAudioRemoteFd;
        }
    } else {
        if(pClassInterface->teacherAudioRemoteFd > 0) {
            return pClassInterface->teacherAudioRemoteFd;
        } else {
            pClassInterface->teacherAudioRemoteFd = setupLTCPCSocket(TEACHER_AUDIO_REMOTE_STREAM);          
            return pClassInterface->teacherAudioRemoteFd;
        }
    }
}

int getCaptureFd(int chn) 
{
    if(IsTeacher()) {
        if(chn == 0) {
            return pClassInterface->teacherLocalFd;
        } else if(chn == 1) {
            return pClassInterface->vgaLocalFd;
        }    
    } else {
        return pClassInterface->studentLocalFd;
    }    
}

int getAudioInFd()
{
    if(IsTeacher()) {
        return pClassInterface->teacherAudioLocalFd;
    } else {
        return pClassInterface->studentAudioLocalFd;
    }    
}

static void * classIncomingConnectionHandler(void *p)
{
    LISTEN_S *plisten = (LISTEN_S *)p;
    if(p == NULL)
        return NULL;

    while (1) {
#if 1
        struct sockaddr_un remote_addr;
        memset(&remote_addr, 0, sizeof(remote_addr));
        socklen_t len = sizeof(remote_addr);
        int clientSocket = accept(plisten->listenFd, (struct sockaddr*)&remote_addr, &len);
        if (clientSocket < 0) {
            if (errno != EWOULDBLOCK) {
                printf("%s, accept() error: %d (%s)\n", __FUNCTION__, errno, strerror(errno));
            }
            printf("%s, accept() error: %d (%s)\n", __FUNCTION__, errno, strerror(errno));
            break;
        }

        //makeSocketNonBlocking(clientSocket);
        /*
        struct timeval tv = {3, 0};
        if (setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof(tv))) {
            printf("%s, Setting socket rcv timeout to 3s failed!\n", __FUNCTION__);
        }
        if (setsockopt(clientSocket, SOL_SOCKET, SO_SNDTIMEO, (char *)&tv, sizeof(tv))) {
            printf("%s, Setting socket snd timeout to 3s failed!\n", __FUNCTION__);
        }
        */
        *plisten->pfd = clientSocket;
        printf("listen:%d,clientSocket:%d\n",plisten->listenFd,clientSocket);
#endif
    }
    return NULL;
}


int webrtc_class_lsocket_interface(const char *path,int offset)
{
    pthread_t mListenPid;
    LISTEN_S *plisten = malloc(sizeof(LISTEN_S));
    plisten->pfd = (int *)((unsigned char *)pClassInterface + offset);
    printf("webrtc_class_lsocket_interface:offsetof:%d,path:%s\n",offset,path);
    int mListenSocket = setupLTCPSSocket(path);
    if(mListenSocket != -1) {
        plisten->listenFd = mListenSocket;
        printf("mListenSocket: %d\n", mListenSocket);
        pthread_create(&mListenPid, 0, classIncomingConnectionHandler, (void *)plisten);
    }
    return 0;
}

void online_class_release()
{
    int i;
    printf("online_class_release\n");
#if 1
    for(i=0;i<3;i++) {
        if(pClassInterface->studentRemoteFd[i] > 0) {
            close(pClassInterface->studentRemoteFd[i]);
            //pClassInterface->studentRemoteFd[i] = 0; 
        } 
    }
    if(pClassInterface->studentAudioRemoteFd > 0) {
        close(pClassInterface->studentAudioRemoteFd);
        //pClassInterface->studentAudioRemoteFd = 0;    
    }
    if(pClassInterface->teacherRemoteFd > 0) {
        close(pClassInterface->teacherRemoteFd);
        //pClassInterface->teacherRemoteFd = 0;
    }
    if(pClassInterface->vgaRemoteFd > 0) {
        close(pClassInterface->vgaRemoteFd);
        //pClassInterface->vgaRemoteFd = 0;    
    }
    if(pClassInterface->teacherAudioRemoteFd > 0) {
        close(pClassInterface->teacherAudioRemoteFd);
        //pClassInterface->teacherAudioRemoteFd = 0;    
    }
    teacherAcrossBind = -1;
    if(pClassInterface->teacherLocalFd > 0) {
        close(pClassInterface->teacherLocalFd);
    }
    if(pClassInterface->vgaLocalFd > 0) {
        close(pClassInterface->vgaLocalFd);
    }
    if(pClassInterface->interactionLocalFd > 0) {
        close(pClassInterface->interactionLocalFd);
    }
    if(pClassInterface->teacherAudioLocalFd > 0) {
        close(pClassInterface->teacherAudioLocalFd);
    }
    if(pClassInterface->studentLocalFd > 0) { 
        close(pClassInterface->studentLocalFd);
    }
    if(pClassInterface->studentAudioLocalFd > 0) {
        close(pClassInterface->studentAudioLocalFd);
    }
    memset(pClassInterface,0,sizeof(ONLINE_CLASS_INTERFACE));
#endif
}

void online_class_init() 
{
    printf("online_class_init\n");
    memset(pClassInterface,0,sizeof(ONLINE_CLASS_INTERFACE));
    webrtc_class_lsocket_interface(TEACHER_LOCAL_STREAM,offsetof(ONLINE_CLASS_INTERFACE,teacherLocalFd));    
    webrtc_class_lsocket_interface(VGA_LOCAL_STREAM,offsetof(ONLINE_CLASS_INTERFACE,vgaLocalFd));    
    //webrtc_class_lsocket_interface(INTERACTION_LOCAL_STREAM,offsetof(ONLINE_CLASS_INTERFACE,interactionLocalFd));    
    webrtc_class_lsocket_interface(TEACHER_AUDIO_LOCAL_STREAM,offsetof(ONLINE_CLASS_INTERFACE,teacherAudioLocalFd));    
    webrtc_class_lsocket_interface(STUDENT_LOCAL_STREAM,offsetof(ONLINE_CLASS_INTERFACE,studentLocalFd));    
    webrtc_class_lsocket_interface(STUDNET_AUDIO_LOCAL_STREAM,offsetof(ONLINE_CLASS_INTERFACE,studentAudioLocalFd));    
}


