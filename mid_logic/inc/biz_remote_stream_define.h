#ifndef __BIZ_REMOTE_STREAM_DEFINE_H__
#define __BIZ_REMOTE_STREAM_DEFINE_H__

#include "string.h"

#include "types.h"
#include "ctrlprotocol.h"
#include "biz_device.h"


#define MAX_FRAME_SIZE (768 << 10)
//������ͷ
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

typedef enum
{
	EM_STREAM_CONNECTED,
	EM_STREAM_RCV_ERR,
	EM_STREAM_PROGRESS,	//�ļ����ؽ���
	EM_STREAM_END,	 	//�ļ��������
} EM_STREAM_STATE_TYPE;

/*
ý������������
��ΪԤ�����طš��ļ����صĻ���
*/
class CMediaStream : public CObject
{
public:
	int Start(CMediaStream *pstream);
	int Stop();
	virtual int dealFrameFunc(FRAMEHDR *pframe_hdr)
	{
		return TRUE;
	}
	virtual int dealStateFunc(EM_STREAM_STATE_TYPE state, int param = 0)//param: �ļ����ؽ���ֵ
	{
		return TRUE;
	}

	CMediaStream()
	: bstart(0)
	, dev_ip(0)
	, dev_idx(-1)
	, stream_idx(-1)
	{
		memset(&req, 0, sizeof(ifly_TCP_Stream_Req));
	}

	~CMediaStream()
	{
	}
	
protected:
	u8	bstart;
	u32 dev_ip;
	s32 dev_idx;
	s32 stream_idx;
	ifly_TCP_Stream_Req req;

private:
    CMediaStream(CMediaStream &)
	{
		
	}
};

typedef struct _SDev_StearmRcv_t
{
	u8 bstart;
	int	sockfd;	
	u32 linkid;
	ifly_TCP_Stream_Req req;
	CMediaStream* pstream; //ָ���������ṹ��Ԥ�����طš��ļ�����

	_SDev_StearmRcv_t()
	: bstart(0)
	, sockfd(INVALID_SOCKET)
	, linkid(-1)
	, pstream(NULL)
	{
		memset(&req, 0, sizeof(ifly_TCP_Stream_Req));
	}
} SDev_StearmRcv_t;





#endif

