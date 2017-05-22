#ifndef _APP_DISPLAY_H_
#define _APP_DISPLAY_H_


#ifdef __cplusplus
extern "C"
{
#endif




void SwitchInteractionMode(int mode);
int DisplayInit(void);
int StopVo(void);

void DisplayYuv(unsigned char *pyuv, unsigned int width, unsigned int height);



#ifdef __cplusplus
}
#endif
#endif
