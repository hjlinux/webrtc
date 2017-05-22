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


int DisplayInit(void)
{
    return 1;
}


void DisplayStop(void)
{
}



void DisplayYuv(unsigned char *pyuv, unsigned int width, unsigned int height)
{
}


