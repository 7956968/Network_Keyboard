#ifndef __BIZ_PREVIEW_H__
#define __BIZ_PREVIEW_H__

#include "types.h"
#include "ctrlprotocol.h"


//�ⲿ�ӿ�
#ifdef __cplusplus
extern "C" {
#endif

//ģ���ʼ��
int BizPreviewInit(void);

//������ʾ��ʼ��
int BizPreviewDisplayInit(void);

//bmain �Ƿ�������
int BizPreviewStart(EM_DEV_TYPE _dev_type, u32 _dev_ip, u8 chn, u8 bmain);

int BizPreviewStop();

//biz_device ����
int BizPreviewNotify(u32 dev_ip, u8 chn, u8 stream_state);

//��̨����
int BizPtzControl(s32 handle, const ifly_PtzCtrl_t *p_para);


#ifdef __cplusplus
}
#endif



#endif

