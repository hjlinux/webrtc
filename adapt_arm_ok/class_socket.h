/******************************************************************************* 
  Copyright (C), 1993-2013, Zonekey Tech. Co., Ltd.

 ******************************************************************************
 File Name     : zk_class_socket.h
Version       : Initial Draft
Author        : zhengm
Created       : 2017/03/08
Description   : 
 ******************************************************************************/

#ifndef __ZK_CLASS_SOCKET_H__
#define __ZK_CLASS_SOCKET_H__

//#define X86_TEST
//#define RECORDER_IP "192.168.13.174" 

#define TCP_LISTEN_SIZE 6
#define MAX_MSG_LEN 10240
#define closeSocket close

int setupLTCPCSocket(const char *sockPath);
int setupLTCPSSocket(const char *sockPath);
int readLTCPSocket(int socket, char* buffer, int len);
int writeLTCPSocket(int socket, char* buffer, int len);
#endif

