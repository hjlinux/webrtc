#include "param_conf.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEBUG

static ParamConf_t ParamConf;

void param_server_handler(char *value)
{
   strcpy(ParamConf.server,value);
#ifdef DEBUG
    printf("server:%s\n",ParamConf.server);
#endif
}

void param_port_handler(char *value)
{
    ParamConf.port = atoi(value);    
#ifdef DEBUG
    printf("port:%d\n",ParamConf.port);
#endif
}

void param_devtype_handler(char *value)
{
    if(1)//((0 == strcmp(value,"teacher")) || (0 == strcmp(value,"student")))
    {
        strcpy(ParamConf.devtype,value);
        char buf[256];
        sprintf(buf,"role=%s",value);
    }
    else
    {
        printf("invalid devtype:%s\n",value);    
    }
#ifdef DEBUG
    printf("devtype:%s\n",ParamConf.devtype);
#endif
}

void param_videodev_handler(char *value)
{
    ParamConf.videodev = atoi(value);
#ifdef DEBUG
    printf("videotype:%d\n",ParamConf.videodev);
#endif
}

void param_vgadev_handler(char *value)
{
    ParamConf.vgadev = atoi(value);
#ifdef DEBUG
    printf("vgatype:%d\n",ParamConf.vgadev);
#endif
}

struct _cmd_line_
{
    char cmd[32];
    void (*handler)(char *value);   
} CmdLines[] = {
    {"videodev",param_videodev_handler},
    {"vgadev",param_vgadev_handler},
    {"devtype",param_devtype_handler},
    {"server",param_server_handler},
    {"port",param_port_handler},
    {"",NULL}, 
};

void ParseLoadConf()
{
    int i;
    char *p;
    char line[256];
    char cmd[64],value[64];
    FILE *fp = fopen(PARAM_CONF_PATH,"rb");
    if(INVALID_CONF == fp)
    {
        printf("%s:",PARAM_CONF_PATH);
        perror("");
        exit(-1);    
    }
    
    while(fgets(line,256,fp))
    {
        i = 0;
        p = line;
        memset(cmd,0,64);
        memset(value,0,64);
        while((' ' == *p) || ('\t' == *p))
            p ++;
        if(('\n' == *p) || ('#' == *p))
            continue;
        while((' ' != *p) && ('\t' != *p) && ('=' != *p))
        {
            cmd[i] = *p;
            p ++;
            i ++;
        }
        cmd[i] = '\0';
        while('=' != *p) p++;
        p ++;
        while((' ' == *p) || ('\t' == *p))
            p ++;
        i = 0;
        while((' ' != *p) && ('\t' != *p) && ('=' != *p) && ('\n' != *p) && ('#' != *p))
        {
            value[i] = *p;
            p ++;
            i ++;
        }
        value[i] = '\0';
        i = 0;

        while(CmdLines[i].cmd[0] && (0 != strcmp(CmdLines[i].cmd,cmd)))
        {
            i ++;
        }
        if(CmdLines[i].cmd[0])
        {
            CmdLines[i].handler(value);
        }
        else
        {
            printf("invalid cmd:%s=%s\n",cmd,value);    
        }
    }
    fclose(fp);

}

int IsTeacher()
{
    if(0 == strcmp(ParamConf.devtype,"teacher"))
        return 1;
    return 0;  
}

void setTeacher()
{
    strcpy(ParamConf.devtype,"teacher");    
}

void setStudent()
{
    strcpy(ParamConf.devtype,"student");    
}

int GetVideoDev()
{
    return ParamConf.videodev;    
}

int GetVgaDev()
{
    return ParamConf.vgadev;    
}

void GetDevType(char *type)
{
    strcpy(type,ParamConf.devtype);
}

void GetServerIp(char *ip)
{
    strcpy(ip,ParamConf.server);    
}

unsigned short GetServerPort()
{
    return ParamConf.port;    
}

#if 0
int main()
{
    ParseLoadConf();
    return 0;    
}
#endif
