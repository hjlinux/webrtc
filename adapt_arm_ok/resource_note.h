#ifndef _COMMON_H_
#define _COMMON_H_

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_CLIENT_CNT  4
#define MAX_LOCAL_CNT  2

typedef struct zkVIDEO_PARAM_S
{
    char name[64];
    int enViMode;
    int viChn;
    int vpssGrp;
    int vencChn;
    int voChn;
    int vdecChn;
    int spiChn;
    int play;
}VIDEO_PARAM_S;

int GetCaptureChn();
int GetClientChn();
int GetClientVoChn(int clientChn);
void SetDisplayInfo(int clientChn,char *name,int videoChn,int disp_pos_);

char *GetCaptureName(int chn);
char *GetRemoteName(int chn);

#ifdef __cplusplus
}
#endif
#endif
