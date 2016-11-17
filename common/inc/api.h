#ifndef __API_H__
#define __API_H__

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

uint SystemGetMSCount(void);
//��Ҫ����һ����������ȡjiffies

uint64 SystemGetMSCount64(void);

void SystemSleep(uint ms);

u32 copyFile(int fd_dst, int fd_src);

//����fd ������ ���
int fdSetNonblockFlag(s32 fd);

//ȥ��fd ������ ���
int fdClrNonblockFlag(s32 fd);

#ifdef __cplusplus
}
#endif


#endif
