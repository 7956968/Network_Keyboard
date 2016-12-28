#ifndef __BIZ_PLAYBACK_H__
#define __BIZ_PLAYBACK_H__

#include "types.h"
#include "ctrlprotocol.h"

typedef enum
{
	EM_PLAYBACK_NONOE,
	EM_PLAYBACK_FILE,
	EM_PLAYBACK_TIME,
} EM_PLAYBACK_TYPE;

typedef struct
{
	u8		chn;				//ͨ����
	u16		type;				//����
	u32		start_time;			//��ʼʱ��
	u32		end_time;			//��ֹʱ��
} SPlayback_Time_Info_t;

//�ⲿ�ӿ�
#ifdef __cplusplus
extern "C" {
#endif

//ģ���ʼ��
int BizPlaybackInit(void);

int BizPlaybackStartByFile(u32 _dev_ip, ifly_recfileinfo_t *pfile_info);
int BizPlaybackStartByTime(u32 _dev_ip, u8 chn, u32 start_time, u32 end_time);


//�Ƿ��Ѿ����ڽ�����
VD_BOOL BizPlaybackIsStarted();

int BizPlaybackStop();


//���ſ���
int BizPlaybackCtl();


#ifdef __cplusplus
}
#endif



#endif
