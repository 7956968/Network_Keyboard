#ifndef __FLASH_H__
#define __FLASH_H__

#include <sys/ioctl.h>
#include <unistd.h>
#include "zlib.h"
#include <linux/types.h>
#include <sys/types.h>
#include <mtd/mtd-user.h>
#include <string.h>

#include "types.h"


typedef struct
{
	u32 nConfSize;	//ѹ����Ĵ�С
	u32 nFileSize;	//ѹ��ǰ�Ĵ�С
	u32 nCheckSum;	//У���
}SConfigFlashHead;

#ifdef __cplusplus
extern "C" {
#endif

s32 ReadFlashToFile(const s8 *pFilePath, const s8 *pDevPath, u32 offset, u32 flashSizeLimit);

s32 SetZipConf(int dev_fd, u32 offset, s8* pData, u32 len, u32 limit);

#ifdef __cplusplus
}
#endif

#endif


