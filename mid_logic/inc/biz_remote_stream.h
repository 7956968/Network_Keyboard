#ifndef __BIZ_REMOTE_STREAM_H__
#define __BIZ_REMOTE_STREAM_H__

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>


#include <string.h>

//��״̬
typedef enum
{
	EM_STREAM_STATUS_DISCONNECT,	//δ���ӣ���ʼ״̬
	EM_STREAM_STATUS_RUNNING,		//�����ӣ���������
	EM_STREAM_STATUS_WAIT_DEL,		//�ȴ�ɾ����stream_errno ָʾ�Ƿ����
	EM_STREAM_STATUS_STOP,			//ֹͣ����ģ�������޴�״̬
} EM_STREAM_STATUS_TYPE;


#include "types.h"
#include "ctrlprotocol.h"
#include "biz_device.h"
#include "biz_msg_type.h"


#define MAX_FRAME_SIZE (1 << 20) // 1MB




//֡����ͷ
typedef struct
{
    u8     m_byMediaType; //ý������
    u8    *m_pData;       //���ݻ���
	u32    m_dwPreBufSize;//m_pData����ǰԤ���˶��ٿռ䣬���ڼ�
	// RTP option��ʱ��ƫ��ָ��һ��Ϊ12+4+12
	// (FIXED HEADER + Extence option + Extence bit)
    u32    m_dwDataSize;  //m_pDataָ���ʵ�ʻ����С
    u8     m_byFrameRate; //����֡��,���ڽ��ն�
	u32    m_dwFrameID;   //֡��ʶ�����ڽ��ն�
	u32    m_dwTimeStamp; //ʱ���, ���ڽ��ն�
    union
    {
        struct{
			u32		m_bKeyFrame;    //Ƶ֡����(I or P)
			u16		m_wVideoWidth;  //��Ƶ֡��
			u16		m_wVideoHeight; //��Ƶ֡��
			u32		m_wBitRate;
		}m_tVideoParam;
        u8    m_byAudioMode;//��Ƶģʽ
    };
}FRAMEHDR,*PFRAMEHDR;


/************************************************************
ý������������
��ΪԤ�����طš��ļ����ص��߼���ṹ
************************************************************/
//��ע���֡���ݴ�����
typedef int (CObject:: *PDEAL_FRAME)(u32 stream_id, FRAMEHDR *pframe_hdr);

//��ע���״̬������
typedef int (CObject:: *PDEAL_STATUS)(SBizMsg_t *pmsg, u32 len);


//�ⲿ�ӿ�
#ifdef __cplusplus
extern "C" {
#endif

//extern  API
int BizStreamInit(void);

int BizSendMsg2StreamManager(SBizMsg_t *pmsg, u32 msg_len);

//pstream_id ������ID
//�ɹ����ز�����ʾ���ӳɹ���ֻ��д������Ϣ�б�֮������Ϣ�߳�����
int BizStreamReqPlaybackByFile (
	EM_DEV_TYPE dev_type,
	u32 dev_ip,
	char *file_name,
	u32 offset,
	CObject *obj,
	PDEAL_FRAME deal_frame_cb,
	PDEAL_STATUS deal_status_cb,
	u32 *pstream_id );

//pstream_id ������ID
//�ɹ����ز�����ʾ���ӳɹ���ֻ��д������Ϣ�б�֮������Ϣ�߳�����
int BizStreamReqPlaybackByTime (
	EM_DEV_TYPE dev_type,
	u32 dev_ip,
	u8 chn, 
	u32 start_time, 
	u32 end_time,
	CObject *obj,
	PDEAL_FRAME deal_frame_cb,
	PDEAL_STATUS deal_status_cb,
	u32 *pstream_id );


int BizStreamReqStop(u32 stream_id, s32 stop_reason=SUCCESS);

#ifdef __cplusplus
}
#endif


#endif

