#include <sys/types.h>  
#include <fcntl.h>
#include <unistd.h>  
#include <stdlib.h>  
#include <sys/socket.h>  
#include <stdio.h>  
#include <errno.h>
#include <sys/un.h>  
#include "class_interface.h"

static int createSocket(int type)
{
	int sock = -1;
	sock = socket(AF_UNIX, type, 0);
	return sock;
}

static int setupClientSocket(const char *sockPath)
{
	int sock = -1;
	struct sockaddr_un serv_addr;

	sock = createSocket(SOCK_STREAM);
	if(sock < 0) {
		printf("unable to create stream socket.\n");
		return -1;
	}

	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sun_family = AF_UNIX;
	strcpy(serv_addr.sun_path, sockPath);

	if(connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
		if(errno != EINPROGRESS) {
			printf("failed to connect socket. %d (%s)", errno, strerror(errno));
			close(sock);
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
				close(sock);
				return -1;
			}
		}
	}

	return sock;
}

int main()
{
    int fd1 = setupClientSocket(TEACHER_LOCAL_STREAM); 
    int fd2 = setupClientSocket(VGA_LOCAL_STREAM); 
    int ret;
    int len = 5;
    while(1) {
        ret = write(fd1,&len,4);    
        ret = write(fd1,"12345",5);    
        printf("ret:%d\n",ret);
        ret = write(fd2,&len,4);    
        ret = write(fd2,"12345",5);    
        printf("ret:%d\n",ret);
        usleep(25000);
    }
    return 0;    
}
