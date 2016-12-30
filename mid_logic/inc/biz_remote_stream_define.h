#ifndef __BIZ_REMOTE_STREAM_DEFINE_H__
#define __BIZ_REMOTE_STREAM_DEFINE_H__

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>


#include "string.h"

#include "types.h"
#include "ctrlprotocol.h"
#include "biz_device.h"


#define MAX_FRAME_SIZE (1 << 20) // 1MB
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
	EM_STREAM_MSG_CONNECTED,
	EM_STREAM_MSG_RCV_ERR,
	EM_STREAM_MSG_STOP,
	EM_STREAM_MSG_DEV_OFFLINE,	//�豸����
	EM_STREAM_MSG_PROGRESS,		//�ļ��ط�/���ؽ���
	EM_STREAM_MSG_FINISH,		//�ļ��������
} EM_STREAM_MSG_TYPE;

typedef enum
{
	EM_STREAM_STATUS_DISCONNECT,	//δ���ӣ���ʼ״̬
	EM_STREAM_STATUS_CONNECTED,	//�����ӣ���������
	EM_STREAM_STATUS_STOP,		//����
} EM_STREAM_STATUS_TYPE;


/*
ý������������
��ΪԤ�����طš��ļ����صĻ���
*/
class CMediaStream : public CObject
{
public:
	int Start();
	int Stop();
	virtual int dealFrameFunc(FRAMEHDR *pframe_hdr)
	{
		ERR_PRINT("this is class base virtual function!!!");
		
		return TRUE;
	}
	virtual int dealStateFunc(EM_STREAM_MSG_TYPE state, u32 param = 0)//param: �ļ����ؽ���ֵ
	{
		ERR_PRINT("this is class base virtual function!!!");
		
		return TRUE;
	}

	CMediaStream()
	: plock4param(NULL)
	, dev_type(EM_DEV_TYPE_NONE)
	, dev_ip(INADDR_NONE)
	, link_id(INVALID_VALUE)
	, err_code(SUCCESS)
	, status(EM_STREAM_STATUS_DISCONNECT)
	{
		memset(&req, 0, sizeof(ifly_TCP_Stream_Req));
	}

	~CMediaStream()
	{
	}
	
protected:
	C_Lock *plock4param;//mutex
	
	EM_DEV_TYPE dev_type;
	u32 dev_ip;
	u32 link_id;
	s32 err_code;
	EM_STREAM_STATUS_TYPE status;
	ifly_TCP_Stream_Req req;

private:
    CMediaStream(CMediaStream &)
	{
		
	}
};

typedef struct _SDev_StearmRcv_t
{
	VD_BOOL b_connect;
	int	sock_fd;	
	u32 link_id;
	ifly_TCP_Stream_Req req;
	CMediaStream* pstream; //ָ���������ṹ��Ԥ�����طš��ļ�����

	_SDev_StearmRcv_t()
	: b_connect(FALSE)
	, sock_fd(INVALID_SOCKET)
	, link_id(INVALID_VALUE)
	, pstream(NULL)
	{
		memset(&req, 0, sizeof(ifly_TCP_Stream_Req));
	}
} SDev_StearmRcv_t;





#endif

