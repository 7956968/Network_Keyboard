#ifndef __BIZ_PLAYBACK_H__
#define __BIZ_PLAYBACK_H__

#include "types.h"
#include "ctrlprotocol.h"
#include "biz_msg_type.h"

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

typedef struct
{
	
} SPlayback_Ctrl_t;


//�ⲿ�ӿ�
#ifdef __cplusplus
extern "C" {
#endif

//ģ���ʼ��
int BizPlaybackInit(void);
int BizSendMsg2PlaybackManager(SBizMsg_t *pmsg, u32 msg_len);


//playback_chn �ϲ㴫��
int BizModulePlaybackStartByFile(u32 playback_chn, u32 _dev_ip, ifly_recfileinfo_t *pfile_info);
int BizModulePlaybackStartByTime(u32 playback_chn, u32 _dev_ip, u8 chn, u32 start_time, u32 end_time);


//�Ƿ��Ѿ����ڽ�����
VD_BOOL BizModulePlaybackIsStarted(u32 playback_chn);

int BizModulePlaybackStop(u32 playback_chn);

//�طſ���
int BizModulePlaybackPause(u32 playback_chn);
int BizModulePlaybackResume(u32 playback_chn);
int BizModulePlaybackStep(u32 playback_chn);//֡��
int BizModulePlaybackRate(u32 playback_chn, s32 rate);//{-8, -4, -2, 1, 2, 4, 8}
int BizModulePlaybackSeek(u32 playback_chn, u32 time);




#ifdef __cplusplus
}
#endif



#endif

