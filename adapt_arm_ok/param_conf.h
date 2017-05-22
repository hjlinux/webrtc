#ifndef _PARAM_COANF_H_
#define _PARAM_COANF_H_
#ifdef __cplusplus
extern "C"
{
#endif

#define PARAM_CONF_PATH "/etc/T8000/interaction.conf"
#define INVALID_CONF  NULL

typedef struct _param_conf_
{
    char devtype[16];
    char server[16];
    int videodev;
    int vgadev;
    unsigned short port;
}ParamConf_t;

unsigned short GetServerPort();
void GetServerIp(char *ip);
void GetDevType(char *type);
void ParseLoadConf();
int IsTeacher();
int GetVideoDev();
int GetVgaDev();
void setTeacher();
void setStudent();
#ifdef __cplusplus
}
#endif

#endif
