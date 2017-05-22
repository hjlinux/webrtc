#include <sys/types.h>  
#include <fcntl.h>
#include <unistd.h>  
#include <stdlib.h>  
#include <sys/socket.h>  
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>  
#include <errno.h>
#include <sys/un.h>  
#include "class_socket.h"
#include "class_interface.h"

//#define DEBUG

static int createSocket(int type)
{
	int sock = -1;
#ifdef X86_TEST
	sock = socket(AF_INET, type, 0);
#else
	sock = socket(AF_UNIX, type, 0);
#endif
	return sock;
}

unsigned short getPort(const char *path)
{
    if(strcmp(path,TEACHER_LOCAL_STREAM) == 0) {
        return 6000;    
    } else if(strcmp(path,VGA_LOCAL_STREAM) == 0) {
        return 6001;    
    } else if(strcmp(path,TEACHER_AUDIO_LOCAL_STREAM) == 0) {
        return 6002;    
    } else if(strcmp(path,STUDENT_LOCAL_STREAM) == 0) {
        return 6003;    
    } else if(strcmp(path,STUDNET_AUDIO_LOCAL_STREAM) == 0) {
        return 6004;    
    } else if(strcmp(path,STUDENT_REMOTE0_STREAM) == 0) {
        return 5000;    
    } else if(strcmp(path,STUDENT_REMOTE1_STREAM) == 0) {
        return 5001;    
    } else if(strcmp(path,STUDENT_REMOTE2_STREAM) == 0) {
        return 5002;    
    } else if(strcmp(path,STUDENT_AUDIO_REMOTE_STREAM) == 0) {
        return 5003;    
    } else if(strcmp(path,TEACHER_REMOTE_STREAM) == 0) {
        return 5004;    
    } else if(strcmp(path,VGA_REMOTE_STREAM) == 0) {
        return 5005;    
    } else if(strcmp(path,TEACHER_AUDIO_REMOTE_STREAM) == 0) {
        return 5006;    
    }
}


static int setupClientSocket(const char *sockPath)
{
    printf("connect,path:%s\n",sockPath);
	int sock = -1;
#ifdef X86_TEST
	struct sockaddr_in serv_addr;
    memset(&serv_addr,0,sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(getPort(sockPath));
    inet_aton(RECORDER_IP,&serv_addr.sin_addr);
#else
	struct sockaddr_un serv_addr;
	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sun_family = AF_UNIX;
	strcpy(serv_addr.sun_path, sockPath);
#endif
	sock = createSocket(SOCK_STREAM);
	if(sock < 0) {
		printf("unable to create stream socket.\n");
		return -1;
	}

	if(connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
		if(errno != EINPROGRESS) {
			printf("failed to connect socket. %d (%s)", errno, strerror(errno));
			closeSocket(sock);
			return -1;
		} else {
			fd_set fdw;
			struct timeval timeout;

			FD_ZERO(&fdw);
			FD_SET(sock, &fdw);

			timeout.tv_sec = 3;
			timeout.tv_usec = 0;

			int res = select(sock + 1, NULL, &fdw, NULL, &timeout);
			if(res != 1) {
				printf("failed to connect socket. %d (%s)", errno, strerror(errno));
				closeSocket(sock);
				return -1;
			}
		}
	}
    printf("connect,path:%s,fd:%d\n",sockPath,sock);

	//setSockTimeOut(sock, 3);

	return sock;
}

static int setupServerSocket(const char *sockPath)
{
	int newSocket = createSocket(SOCK_STREAM);
	if (newSocket < 0) {
		printf("unable to create stream socket.\n");
		return newSocket;
	}   

#ifdef X86_TEST
	struct sockaddr_in local_addr;
    memset(&local_addr,0,sizeof(local_addr));
    local_addr.sin_family = AF_INET;
    local_addr.sin_port = htons(getPort(sockPath));
    local_addr.sin_addr.s_addr = htonl(INADDR_ANY);;
#else
	struct sockaddr_un local_addr;
	memset(&local_addr, 0, sizeof(local_addr));
	local_addr.sun_family = AF_UNIX;
	strcpy(local_addr.sun_path, sockPath);
	unlink(local_addr.sun_path);
#endif


	if (bind(newSocket, (struct sockaddr*)&local_addr, sizeof(local_addr)) != 0) {
		printf("%s, bind() error: %d (%s)\n", __FUNCTION__, errno, strerror(errno));
		closeSocket(newSocket);
		return -1;
	}

	return newSocket;
}

int setupLTCPCSocket(const char *sockPath)
{
	return setupClientSocket(sockPath);
}

int setupLTCPSSocket(const char *sockPath)
{
	int ourSocket = -1;

	do {
		ourSocket = setupServerSocket(sockPath);
		if (ourSocket < 0)
			break;

		if (listen(ourSocket, TCP_LISTEN_SIZE) < 0) {
			printf("%s, listen() error: %d (%s)\n", __FUNCTION__, errno, strerror(errno));
			break;
		}
		return ourSocket;
	} while (0);

	if (ourSocket != -1) 
		closeSocket(ourSocket);

	return -1;
}

int readLTCPSocket(int socket, char* buffer, int len)
{
	int offset = 0, bytesRead = 0;

	memset(buffer, 0, len);

	while(offset < len) {
		bytesRead = recv(socket, buffer+offset, len-offset, 0);
		if (bytesRead < 0) {
			if (errno == 111 || errno == 113 || errno == EAGAIN) {
				continue;
			} else {
				//printf("recv() error: %d (%s)\n", errno, strerror(errno));
				return -1;
			}
		} else if (bytesRead == 0) {
			//printf("recv() error: %d (%s)\n", errno, strerror(errno));
			return -1;
		} else {
			offset += bytesRead;
		}
	}

	return 0;
}

int writeLTCPSocket(int socket, char* buffer, int len)
{
	int bytesSent = 0, offset = 0;
	int s32Ret;

	while (offset < len) {
		bytesSent = send(socket, buffer+offset, len-offset, 0);

		if (bytesSent < 0) {
			printf("%s, socket send error %d (%d bytes)\n", __FUNCTION__, errno, len-offset);

			if (errno == EINTR)
				continue;

			break;
		}
		if (bytesSent == 0)
			break;

		offset += bytesSent;
	}

	if(offset != len) {
		if(socket != -1) {
			closeSocket(socket);
			socket = -1;
		}
		s32Ret = -1;
	} else
		s32Ret = 0;

	return s32Ret;
}

