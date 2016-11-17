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

//stream_type: 0 ��������1 ������
//handle: �ɹ��󷵻صľ��
int BizPreviewStart(u32 svr_ip, u8 chn, u8 bmain);

int BizPreviewStop();

//biz_device ����
int BizPreviewNotify(u32 dev_ip, u8 chn, u8 stream_state);

//��̨����
int BizPtzControl(s32 handle, const ifly_PtzCtrl_t *p_para);


#ifdef __cplusplus
}
#endif



#endif

