#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>

#define ZK_MINIGUI
//#define INTERACTION

#define T800X_NAME				"t800x"
#define T800X_FILE				"/zqfs/bin/t800x"
#define BOA_NAME				"boa"
#define BOA_FILE				"/zqfs/bin/boa"
#define BOA_CONF_FILE			"/zqfs/etc/T8000/"
#define CRTMPSERVER_NAME		"crtmpserver"
#define CRTMPSERVER_FILE		"/zqfs/bin/crtmpserver-811/sbin/crtmpserver"
#define CRTMPSERVER_CONF_FILE	"/zqfs/bin/crtmpserver-811/etc/crtmpserver.lua"
#define MONITORSERVER_LOG		"/var/images/log/t800x/monitorServer.log"
#define TMP_PROCESS_ID			"/tmp/processid"
#ifdef ZK_MINIGUI
#define GUI_NAME				"gui"
#define GUI_FILE				"source /zqfs/GUI/etc/run"
#define GUI_SH					"/zqfs/GUI/etc/run"
#endif
#ifdef INTERACTION
#define WEBRTC_SERVER_NAME		"interaction_server"
#define WEBRTC_SERVER_FILE		"/zqfs/bin/interaction_server"
#define WEBRTC_MASTER_NAME		"master"
#define WEBRTC_MASTER_FILE		"/zqfs/bin/master"
#endif


static int error_log = -1;

static void getLocalTime(char *localTime)
{
	int year, month, day, hour, min, sec;
	time_t nowtime;
	struct tm *timeinfo;

	time(&nowtime);
	timeinfo = localtime(&nowtime);
	year = timeinfo->tm_year+1900;
	month = timeinfo->tm_mon+1;
	day = timeinfo->tm_mday;
	hour = timeinfo->tm_hour;
	min = timeinfo->tm_min;
	sec = timeinfo->tm_sec;
	sprintf(localTime, "%04d/%02d/%02d-%02d:%02d:%02d", year, month, day, hour, min, sec);
}

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

static int Run_Process(const char *pname, const char *pfile, const char *pconf)
{
	char cmd[256] = {0};
	char localTime[256] = {0};

	sprintf(cmd, "pgrep %s > %s", pname, TMP_PROCESS_ID);
	system(cmd);
	unsigned long long fileSize = getFileSize(TMP_PROCESS_ID);
	memset(cmd, 0, sizeof(cmd));
	sprintf(cmd, "rm %s", TMP_PROCESS_ID);
	system(cmd);
	if(fileSize > 0) {
		//printf("%s has been started\n", pname);
		return 0;
	}

	if(strcmp(pname, GUI_NAME) == 0) {
		if(access(GUI_SH, R_OK)) {
			printf(" Imagefile: %s does not exist!\n", GUI_SH);
			return -1;
		}
	} else {
		if(access(pfile, R_OK)) {
			printf(" Imagefile: %s does not exist!\n", pfile);
			return -1;
		}
	}

	memset(cmd, 0, sizeof(cmd));
	getLocalTime(localTime);
	sprintf(cmd, "%s restart @ %s\n", pname, localTime);
	write(error_log, cmd, strlen(cmd));

	memset(cmd, 0, sizeof(cmd));
	if(strcmp(pname, T800X_NAME) == 0)
		sprintf(cmd, "%s &", pfile);
	else if(strcmp(pname, BOA_NAME) == 0)
		sprintf(cmd, "%s -c %s &", pfile, pconf);
	else if(strcmp(pname, CRTMPSERVER_NAME) == 0)
		sprintf(cmd, "%s %s &", pfile, pconf);
#ifdef ZK_MINIGUI
	else if(strcmp(pname, GUI_NAME) == 0)
		sprintf(cmd, "%s", pfile);
#endif
#ifdef INTERACTION
	else if(strcmp(pname, WEBRTC_SERVER_NAME) == 0)
		sprintf(cmd, "%s &", pfile);
	else if(strcmp(pname, WEBRTC_MASTER_NAME) == 0)
		sprintf(cmd, "%s &", pfile);
#endif
	system(cmd);
	printf("cmd: %s\n", cmd);

	return 0;
}

static int open_log(const char *log_name)
{
	int fd = -1;
	fd = open(log_name, O_WRONLY | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR | S_IROTH | S_IRGRP);
	return fd;
}

void close_log(void)
{
	if (error_log != -1) {
		close(error_log);
		error_log = -1;
	}
}

int main(int argc, char *argv[])
{
	error_log = open_log((const char *)MONITORSERVER_LOG);
	if (error_log < 0) {
		printf("open log file failured!\n");
		exit(1);
	}

	while(1) {
		if(Run_Process(T800X_NAME, T800X_FILE, NULL) != 0) 
			break;
		sleep(3);
#ifdef ZK_MINIGUI
		if(Run_Process(GUI_NAME, GUI_FILE, NULL) != 0) 
			break;
		sleep(3);
#endif
		if(Run_Process(BOA_NAME, BOA_FILE, BOA_CONF_FILE) != 0)
			break;
		if(Run_Process(CRTMPSERVER_NAME, CRTMPSERVER_FILE, CRTMPSERVER_CONF_FILE) != 0)
			break;
		sleep(5);
#ifdef INTERACTION
		if(Run_Process(WEBRTC_SERVER_NAME, WEBRTC_SERVER_FILE, NULL) != 0) 
			break;
		if(Run_Process(WEBRTC_MASTER_NAME, WEBRTC_MASTER_FILE, NULL) != 0) 
			break;
#endif
	}
	close_log();

	return 0;
}
