#include <map>
#include <set>
//#include <bitset>
#include <algorithm>
#include <utility>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/select.h>
#include <sys/types.h>

#include "api.h"
#include "singleton.h"
#include "object.h"
#include "glb_error_num.h"
#include "biz.h"
#include "biz_types.h"
#include "biz_config.h"
#include "biz_net.h"
#include "biz_device.h"
#include "biz_remote_stream.h"
#include "atomic.h"
#include "c_lock.h"
#include "crwlock.h"
#include "ccircular_buffer.h"
#include "cthread.h"
#include "ctimer.h"
#include "ctrlprotocol.h"
#include "net.h"

//������������
class CMediaStreamManager;


/*******************************************************************
CMediaStream ����
*******************************************************************/

class CMediaStream : public CObject
{
	friend class CMediaStreamManager;
	
public:
	CMediaStream();
	~CMediaStream();
	
	int Init();
	
private:
    CMediaStream(CMediaStream &)
	{
		
	}
	void FreeSrc();//�ͷ���Դ
	void threadRcv(uint param);//���շ���������

private:
	C_Lock *plock4param;//mutex
	//ָ�����ϲ�ṹ
	CObject *m_obj;
	PDEAL_FRAME m_deal_frame_cb;//��ע���֡���ݴ�����
	PDEAL_STATUS m_deal_status_cb;//��ע���״̬������

	//���ڲ�����
	EM_DEV_TYPE dev_type;//����������
	u32 dev_ip;//������IP
	u32 stream_id;//�ؼ���ϵͳΨһ
	s32 stream_errno;//������
	EM_STREAM_STATUS_TYPE status;//��״̬
	ifly_TCP_Stream_Req req;//���������ݽṹ
	//������
	VD_BOOL b_thread_running;//�����߳����б�־
	VD_BOOL b_thread_exit;//�����߳��˳���־
	s32 sock_stream;
	Threadlet m_threadlet_rcv;
	CSemaphore sem_exit;//�ȴ�threadRcv�˳��ź���
};

/**************************************************************/


/**************************************************************
CMediaStream ����
����: �ϲ�BizStreamReqxxxx ����
����: �ϲ�BizStreamReqStop ��
		�²�����ϲ㴫��EM_STREAM_MSG_STOP ��Ϣ
		����threadRcv ������EM_STREAM_MSG_STOP ��Ϣ
*******************************************************************/
CMediaStream::CMediaStream()
: plock4param(NULL)
//ָ�����ϲ�ṹ
, m_obj(NULL)
, m_deal_frame_cb(NULL)	//��ע���֡���ݴ�����
, m_deal_status_cb(NULL)	//��ע���״̬������

//���ڲ�����
, dev_type(EM_DEV_TYPE_NONE)	//����������
, dev_ip(INADDR_NONE)			//������IP
, stream_id(INVALID_VALUE)		//�ؼ���ϵͳΨһ
, stream_errno(SUCCESS)				//������
, status(EM_STREAM_STATUS_DISCONNECT) //��״̬

//������
, b_thread_running(FALSE)//�����߳����б�־
, b_thread_exit(FALSE)//�����߳��˳���־
, sock_stream(INVALID_SOCKET)
, sem_exit(0)
{
	memset(&req, 0, sizeof(ifly_TCP_Stream_Req)); //���������ݽṹ
}

int CMediaStream::Init()
{
	plock4param = new CMutex;
	if (NULL == plock4param)
	{
		ERR_PRINT("new plock4param failed\n");
		goto fail;
	}
	
	if (plock4param->Create())//FALSE
	{
		ERR_PRINT("create plock4param failed\n");
		goto fail;
	}
	
	return SUCCESS;
	
fail:

	FreeSrc();
	return -FAILURE;
}

void CMediaStream::FreeSrc()//�ͷ���Դ
{
	if (plock4param)
	{
		delete plock4param;
		plock4param = NULL;
	}
}

CMediaStream::~CMediaStream()
{
	FreeSrc();
}

void CMediaStream::threadRcv(uint param)//���շ���������
{
	int ret = SUCCESS;
	int sock_fd = INVALID_SOCKET;
	FRAMEHDR frame_hdr;
	ifly_MediaFRAMEHDR_t hdr;
	char *pframe_data = NULL;
	u8 req_cmd = INVALID_VALUE;
	u32 _stream_id = INVALID_VALUE;
	int inside_err = SUCCESS;//��ʶ�߳��ڲ��Ƿ����
	CObject *obj = NULL;
	PDEAL_FRAME deal_frame_cb = NULL;
	
	fd_set rset;
	struct timeval timeout;
	struct in_addr in;
	SBizMsg_t msg;

	plock4param->Lock();

	obj = m_obj;
	deal_frame_cb = m_deal_frame_cb;

	sock_fd = sock_stream;
	in.s_addr = dev_ip;
	req_cmd = req.command;
	_stream_id = stream_id;
	
	plock4param->Unlock();

	if (INVALID_SOCKET == sock_fd)
	{
		ERR_PRINT("dev IP: %s, stream_id: %d, req_cmd: %d, INVALID_SOCKET == sock_stream\n",
			inet_ntoa(in), _stream_id, req_cmd);

		inside_err = -EPARAM;
		goto done;
	}
	
	pframe_data = new char[MAX_FRAME_SIZE];
	if (NULL == pframe_data)
	{
		ERR_PRINT("dev IP: %s, stream_id: %d, req_cmd: %d, malloc frame buffer failed\n",
			inet_ntoa(in), _stream_id, req_cmd);

		inside_err = -ESPACE;
		goto done;
	}

	DBG_PRINT("dev IP: %s, stream_id: %d, req_cmd: %d, threadRcv running\n",
			inet_ntoa(in), _stream_id, req_cmd);

	while (1)
	{		
		plock4param->Lock();

		if (EM_STREAM_STATUS_WAIT_DEL == status)//��Ϊ��ɾ��״̬
		{
			//��ʱinside_err �϶�= SUCCESS�������ϴ�ѭ���Ѿ��˳���
			//�߳��˳�ʱ���Ա�ʶ���ⲿ�����˳������ڲ������˳�
			//�ڲ��˳�����Ϣ
			
			if (b_thread_exit)//�ȴ��ⲿ�����˳�
			{	
				
			}
			else
			{
				ERR_PRINT("dev IP: %s, stream_id: %d, req_cmd: %d, EM_STREAM_STATUS_WAIT_DEL == status, "
					"but b_thread_exit == FALSE, inconceivable\n", inet_ntoa(in), _stream_id, req_cmd);
			}

			plock4param->Unlock();	
			break;
		}
		
		plock4param->Unlock();

		FD_ZERO(&rset);
		FD_SET(sock_stream, &rset);
		timeout.tv_sec = 1;
		timeout.tv_usec = 0;

		ret = select(sock_stream+1, &rset, NULL, NULL, &timeout);
		if (ret < 0)
		{
			ERR_PRINT("dev IP: %s, stream_id: %d, req_cmd: %d, select failed, ret: %d, %s\n",
				inet_ntoa(in), _stream_id, req_cmd, ret, strerror(ret));
			
			inside_err = -ERECV;
			break;
		}
		else if (ret == 0)//timeout
		{
			continue;
		}

		if (FD_ISSET(sock_stream, &rset))
		{
			memset(&frame_hdr, 0, sizeof(FRAMEHDR));

			//0��Ԥ�� 1���ļ��ط� 2��ʱ��ط� 3���ļ����� 4������ 
			//5 VOIP 6 �ļ���֡���� 7 ʱ�䰴֡���� 8 ͸��ͨ��
			//9 Զ�̸�ʽ��Ӳ�� 10 ������ץͼ 11 ��·��ʱ��ط� 12 ��ʱ�������ļ�
			
			if (req_cmd < 3)
			{
				ret = looprecv(sock_stream, (char *)&hdr, sizeof(ifly_MediaFRAMEHDR_t));
				if (ret)
				{
					ERR_PRINT("dev IP: %s, stream_id: %d, req_cmd: %d, recv ifly_MediaFRAMEHDR_t failed\n",
						inet_ntoa(in), _stream_id, req_cmd);

					inside_err = -ERECV;
					break;
				}
				
				hdr.m_dwDataSize = ntohl(hdr.m_dwDataSize);
				hdr.m_dwFrameID = ntohl(hdr.m_dwFrameID);
				hdr.m_dwTimeStamp = ntohl(hdr.m_dwTimeStamp);
				hdr.m_nVideoHeight = ntohl(hdr.m_nVideoHeight);
				hdr.m_nVideoWidth = ntohl(hdr.m_nVideoWidth);

				if (hdr.m_dwDataSize > MAX_FRAME_SIZE)
				{
					ERR_PRINT("dev IP: %s, stream_id: %d, req_cmd: %d, recv DataSize(%d) > MAX_FRAME_SIZE(%d) failed\n",
						inet_ntoa(in), _stream_id, req_cmd, hdr.m_dwDataSize, MAX_FRAME_SIZE);

					inside_err = -ERECV;
					break;
				}
				
				ret = looprecv(sock_stream, pframe_data, hdr.m_dwDataSize);
				if (ret)
				{
					ERR_PRINT("dev IP: %s, stream_id: %d, req_cmd: %d, recv one frame failed\n",
						inet_ntoa(in), _stream_id, req_cmd);

					inside_err = -ERECV;
					break;
				}
				
				frame_hdr.m_byMediaType = hdr.m_byMediaType;
				frame_hdr.m_byFrameRate = hdr.m_byFrameRate;
				frame_hdr.m_tVideoParam.m_bKeyFrame = hdr.m_bKeyFrame;
				//printf("m_bKeyFrame: %d\n", hdr.m_bKeyFrame);
				frame_hdr.m_tVideoParam.m_wVideoWidth = hdr.m_nVideoWidth;
				//frame.m_tVideoParam.m_wVideoHeight = hdr.m_nVideoHeight;
				frame_hdr.m_tVideoParam.m_wVideoHeight = hdr.m_nVideoHeight & 0x7fff;			//csp add 20091116
				frame_hdr.m_tVideoParam.m_wBitRate = ((hdr.m_nVideoHeight & 0x8000)?1:0) << 31;	//csp add 20091116
				frame_hdr.m_dwDataSize = hdr.m_dwDataSize;
				frame_hdr.m_dwFrameID = hdr.m_dwFrameID;
				frame_hdr.m_dwTimeStamp = hdr.m_dwTimeStamp;
				frame_hdr.m_pData = (u8 *)pframe_data;
#if 1
				if ((MEDIA_TYPE_H264 == frame_hdr.m_byMediaType)&& hdr.m_bKeyFrame)
				{
					DBG_PRINT("dev IP: %s, stream_id: %d, req_cmd: %d, recv one frame, m_byMediaType: %d, m_bKeyFrame: %d, m_dwFrameID: %d, m_dwDataSize: %d\n", 
						inet_ntoa(in), _stream_id, req_cmd, hdr.m_byMediaType, hdr.m_bKeyFrame, hdr.m_dwFrameID, hdr.m_dwDataSize);
				}
#endif				
				//֡�ص�
				if ((MEDIA_TYPE_H264 == frame_hdr.m_byMediaType) 
						&& obj && deal_frame_cb)
				{
					(obj->*deal_frame_cb)(_stream_id, &frame_hdr);
				}
				
			}
			else if (req_cmd == 3)	//�ļ�����
			{
				ret = recv(sock_stream, pframe_data, MAX_FRAME_SIZE, 0);
				if (ret <= 0)
				{					
					ERR_PRINT("dev IP: %s, stream_id: %d, req_cmd: %d, recv failed\n",
						inet_ntoa(in), _stream_id, req_cmd);					
					
					inside_err = -ERECV;
					break;
				}
				//����һ�����ݰ�����֡�޹أ�ֻ�ǽ���frame_hdr ��������
				frame_hdr.m_pData = (u8 *)pframe_data;
				frame_hdr.m_dwDataSize = ret;

				//֡�ص�
				if (obj && deal_frame_cb)
					(obj->*deal_frame_cb)(_stream_id, &frame_hdr);
			}
			else if (req_cmd == 4)	//����
			{
				ret = recv(sock_stream, pframe_data, MAX_FRAME_SIZE, 0);
				if (ret <= 0)
				{					
					ERR_PRINT("dev IP: %s, stream_id: %d, req_cmd: %d, recv failed\n",
						inet_ntoa(in), _stream_id, req_cmd);					
					
					inside_err = -ERECV;
					break;
				}
				//����һ�����ݰ�����֡�޹أ�ֻ�ǽ���frame_hdr ��������
				frame_hdr.m_pData = (u8 *)pframe_data;
				frame_hdr.m_dwDataSize = ret;

				//֡�ص�
				if (obj && deal_frame_cb)
					(obj->*deal_frame_cb)(_stream_id, &frame_hdr);
			}
			else
			{
				DBG_PRINT("dev IP: %s, stream_id: %d, req command(%d) not supported\n",
					inet_ntoa(in), _stream_id, req_cmd);
			}
		}		
	}

done:

	plock4param->Lock();

	b_thread_running = FALSE;

	if (INVALID_SOCKET != sock_stream)
	{
		close(sock_stream);
		sock_stream = INVALID_SOCKET;
	}
	
	if (SUCCESS == stream_errno
			&& SUCCESS != inside_err)
	{
		 stream_errno = inside_err;
	}

	plock4param->Unlock();

	if (pframe_data)
	{
		delete[] pframe_data;
		pframe_data = NULL;
	}

	//ֻ��ģ���ڲ������˳�ʱ����Ϣ
	if (inside_err) 
	{
	 	memset(&msg, 0, sizeof(SBizMsg_t));
		msg.msg_type = EM_STREAM_MSG_STOP;
		msg.un_part_chn.stream_id = _stream_id;
		msg.un_part_data.stream_errno = inside_err;

		ret = BizSendMsg2StreamManager(&msg, sizeof(SBizMsg_t));
		if (ret)
		{
			ERR_PRINT("BizSendMsg2StreamManager failed, ret: %d, stream_id: %d, msg_type: %d\n",
				ret, msg.un_part_chn.stream_id, msg.msg_type);
		}		
	}
	
	DBG_PRINT("dev IP: %s, req_cmd: %d, threadRcv exit\n",
				inet_ntoa(in), req_cmd);

	sem_exit.Post();
}




/*******************************************************************
CMediaStreamManager ����
*******************************************************************/

#define g_biz_stream_manager (*CMediaStreamManager::instance())
#define MAX_STREAM_NO (1<<20)//ע�������2�Ĵ���

//u32:stream_id
typedef std::map<u32, CMediaStream*> MAP_ID_PSTREAM;


class CMediaStreamManager : public CObject
{
	friend class CMediaStream;
	
public:
	PATTERN_SINGLETON_DECLARE(CMediaStreamManager);
	~CMediaStreamManager();

	int Init();
	int ReqStreamStart(
			EM_DEV_TYPE dev_type,
			u32 dev_ip,
			ifly_TCP_Stream_Req *preq,
			CObject *obj,
			PDEAL_FRAME deal_frame_cb,
			PDEAL_STATUS deal_status_cb,
			u32 *pstream_id );
	//�ϲ���õ����رգ������������رգ�Ҳ�������ϲ�����ر�
	//֮�����ڲ�threadMsg  �߳���ɾ����
	int ReqStreamStop(u32 stream_id, s32 stop_reason=SUCCESS);
	
	// д��Ϣto stream manager
	int WriteMsg(SBizMsg_t *pmsg, u32 msg_len);
	
private: //function
	CMediaStreamManager();
	CMediaStreamManager(CMediaStreamManager &)
	{

	}
	void FreeSrc();//�ͷ���Դ
	int dealStreamConnect(u32 stream_id);
	int dealStreamDel(u32 stream_id, u32 stop_reason=SUCCESS);
	int dealStreamMsgStop(u32 stream_id, s32 stream_errno);
	int dealStreamMsgProgess(u32 stream_id, u32 cur_pos, u32 total_size);
	int dealStreamMsgFinish(u32 stream_id);
	void threadMsg(uint param);//��Ϣ�߳�ִ�к���
	//�ⲿ����
	u32 _createStreamID();//�õ�һ���µ���ID
	
private: //data member
	C_Lock *plock4param;//mutex

	VD_BOOL b_inited;
	MAP_ID_PSTREAM map_pstream;
	u32 round_stream_no;//[0, MAX_STREAM_NO-1] ��ת
	u32 stream_cnt;
	
	//��Ϣ��������
	pthread_cond_t mq_cond;		//дflash ����������
	pthread_mutex_t mq_mutex;	//��������������
	u32 mq_msg_count;			//������pmsg_queue ����Ϣ��Ŀ
	CcircularBuffer *pmq_ccbuf;	//���λ�����
	Threadlet mq_msg_threadlet;	//��Ϣ�߳�
};


PATTERN_SINGLETON_IMPLEMENT(CMediaStreamManager);


/*******************************************************************
CMediaStreamManager ����
*******************************************************************/
CMediaStreamManager::CMediaStreamManager()
: plock4param(NULL)
, b_inited(FALSE)
, round_stream_no(0) //[0, MAX_STREAM_NO-1] ��ת
, stream_cnt(0)

//��Ϣ��������
, mq_msg_count(0)
, pmq_ccbuf(NULL)
{
	
}

int CMediaStreamManager::Init()
{
	plock4param = new CMutex;
	if (NULL == plock4param)
	{
		ERR_PRINT("new plock4param failed\n");
		goto fail;
	}
	
	if (plock4param->Create())//FALSE
	{
		ERR_PRINT("create plock4param failed\n");
		goto fail;
	}
	
	//������Ϣ�����̼߳��������
	if (pthread_cond_init(&mq_cond, NULL))
	{
		ERR_PRINT("init mq_cond failed\n");
		goto fail;
	}
	
	if (pthread_mutex_init(&mq_mutex, NULL))
	{
		ERR_PRINT("init mq_mutex failed\n");
		goto fail;
	}

	pmq_ccbuf = new CcircularBuffer(1024);
	if (NULL == pmq_ccbuf)
	{
		ERR_PRINT("new CcircularBuffer failed\n");
		goto fail;
	}

	if (pmq_ccbuf->CreateBuffer())
	{
		ERR_PRINT("CreateBuffer CcircularBuffer failed\n");
		goto fail;
	}

	//������Ϣ���ж�ȡ�߳�
	mq_msg_threadlet.run("BizStreamManager-mq", this, (ASYNPROC)&CMediaStreamManager::threadMsg, 0, 0);

	b_inited = TRUE;
	return SUCCESS;
	
fail:

	FreeSrc();
	return -FAILURE;
}

void CMediaStreamManager::FreeSrc()//�ͷ���Դ
{		
	b_inited = FALSE;

	if (plock4param)
	{
		delete plock4param;
		plock4param = NULL;
	}

	if (pmq_ccbuf)
	{
		delete pmq_ccbuf;
		pmq_ccbuf = NULL;
	}
}

CMediaStreamManager::~CMediaStreamManager()
{
	FreeSrc();
}

//�ⲿ����
u32 CMediaStreamManager::_createStreamID()//�õ�һ���µ���ID
{
	u32 stream_id = round_stream_no;

	round_stream_no = (round_stream_no+1) & (MAX_STREAM_NO-1);//[0, MAX_STREAM_NO-1] ��ת

	return stream_id;
}

int CMediaStreamManager::WriteMsg(SBizMsg_t *pmsg, u32 msg_len)
{
	int ret = SUCCESS;

	if (!b_inited)
	{
		ERR_PRINT("module not init\n");
		return -FAILURE;
	}

	if (NULL == pmsg)
	{
		ERR_PRINT("NULL == pmsg\n");
		
		return -EPARAM;
	}

	if (sizeof(SBizMsg_t) != msg_len)
	{
		ERR_PRINT("sizeof(SBizMsg_t)(%d) != msg_len(%d)\n",
			sizeof(SBizMsg_t), msg_len);
		
		return -EPARAM;
	}

	ret = pthread_mutex_lock(&mq_mutex);
	if (ret)
	{
		ERR_PRINT("pthread_mutex_lock failed, err(%d, %s)\n", ret, strerror(ret));
		
		return -FAILURE;
	}

	ret = pmq_ccbuf->Put((u8 *)pmsg, msg_len);
	if (ret)
	{
		ERR_PRINT("pmsg_queue Put msg failed\n");

		pmq_ccbuf->PutRst();
		goto fail;
	} 

	mq_msg_count++;

	ret = pthread_cond_signal(&mq_cond);
	if (ret)
	{
		ERR_PRINT("pthread_cond_signal failed, err(%d, %s)\n", ret, strerror(ret));
		goto fail;
	}

	ret = pthread_mutex_unlock(&mq_mutex);
	if (ret)
	{
		ERR_PRINT("pthread_mutex_unlock failed, err(%d, %s)\n", ret, strerror(ret));
		
		return -FAILURE;
	}
	
	return SUCCESS;

fail:
	pthread_mutex_unlock(&mq_mutex);
	return ret;
}

int CMediaStreamManager::dealStreamConnect(u32 stream_id)
{
	int ret = SUCCESS;
	s32 sock_stream = INVALID_VALUE;
	struct in_addr in;
	EM_DEV_TYPE dev_type;//����������
	u32 dev_ip = INADDR_NONE;//������IP
	ifly_TCP_Stream_Req req;//���������ݽṹ
	CMediaStream *pstream = NULL;
	MAP_ID_PSTREAM::iterator map_iter;
	CObject *obj = NULL;
	PDEAL_STATUS deal_status_cb = NULL;
	SBizMsg_t msg;
	memset(&msg, 0, sizeof(SBizMsg_t));
	
	
	plock4param->Lock();
	
	map_iter = map_pstream.find(stream_id);
	if (map_iter == map_pstream.end())
	{
		ERR_PRINT("stream_id(%d) not found in map\n", stream_id);

		plock4param->Unlock();
		return -EPARAM;
	}
	
	pstream = map_iter->second;
	if (NULL == pstream)
	{
		ERR_PRINT("stream_id(%d) NULL == pstream\n", stream_id);

		plock4param->Unlock();
		return -EPARAM;
	}
	
	pstream->plock4param->Lock();

	obj = pstream->m_obj;
	deal_status_cb = pstream->m_deal_status_cb;
	
	dev_type = pstream->dev_type;
	dev_ip = pstream->dev_ip;
	req = pstream->req;

	in.s_addr = dev_ip;

	if (EM_STREAM_STATUS_DISCONNECT != pstream->status)
	{
		ERR_PRINT("dev ip: %s, EM_STREAM_STATUS_DISCONNECT != pstream->status(%d)",
			inet_ntoa(in), pstream->status);

		pstream->plock4param->Unlock();
		plock4param->Unlock();
		return -EPARAM;
	}
	
	pstream->plock4param->Unlock();
	plock4param->Unlock();
	
	ret = BizDevReqStreamStart(dev_type, dev_ip, stream_id, &req, &sock_stream);
	if (SUCCESS == ret)
	{
		ret = BizDevStreamProgress(dev_type, dev_ip, stream_id, TRUE);//���ջطŽ�����Ϣ
		if (ret)
		{
			BizDevReqStreamStop(dev_type, dev_ip, stream_id, ret);
		}
	}
	
	if (ret)//failed
	{				
		ERR_PRINT("dev ip: %s, stream_id: %d, req cmd: %d, BizReqStreamStart failed, ret: %d\n",
			inet_ntoa(in), stream_id, req.command, ret);

		plock4param->Lock();

		//ɾ��
		map_pstream.erase(stream_id);//�Ƴ�
		
		delete pstream;//�ͷ�
		pstream = NULL;

		plock4param->Unlock();
		
		//����Ϣ
		msg.msg_type = EM_STREAM_MSG_CONNECT_FALIURE;
		msg.un_part_chn.stream_id = stream_id;
		msg.un_part_data.stream_errno = ret;
	}
	else//success
	{
		DBG_PRINT("dev ip: %s, stream_id: %d, req cmd: %d, BizReqStreamStart success\n",
			inet_ntoa(in), stream_id, req.command);
		
		plock4param->Lock();
		pstream->plock4param->Lock();

		//�ڲ�����
		pstream->sock_stream = sock_stream;
		pstream->status = EM_STREAM_STATUS_RUNNING;
		pstream->stream_errno = SUCCESS;
		
		//��������ȡ�߳�
		pstream->b_thread_running = TRUE;
		pstream->b_thread_exit = FALSE;
		pstream->m_threadlet_rcv.run("BizStream", pstream, (ASYNPROC)&CMediaStream::threadRcv, 0, 0);

		pstream->plock4param->Unlock();
		plock4param->Unlock();
		
		//����Ϣ
		msg.msg_type = EM_STREAM_MSG_CONNECT_SUCCESS;
		msg.un_part_chn.stream_id = stream_id;		
	}
	
	if (obj && deal_status_cb)
	{
		ret = (obj->*deal_status_cb)(&msg, sizeof(SBizMsg_t));
		if (ret)
		{
			ERR_PRINT("deal_status_cb failed, ret: %d, stream_id: %d, msg_type: %d\n",
				ret, stream_id, msg.msg_type);
		}
	}
	
	
#if 0
	ret = BizSendMsg2StreamManager(&msg, sizeof(SBizMsg_t));//�����Լ�����������
	if (ret)
	{
		ERR_PRINT("BizSendMsg2StreamManager failed, ret: %d, stream_id: %d, msg_type: %d\n",
			ret, stream_id, msg.msg_type);
	}
	
#endif
	return ret;
}

int CMediaStreamManager::dealStreamDel(u32 stream_id, u32 stop_reason)
{
	int ret = SUCCESS;
	struct in_addr in;
	EM_DEV_TYPE dev_type;//����������
	u32 dev_ip = INADDR_NONE;//������IP
	CMediaStream *pstream = NULL;
	MAP_ID_PSTREAM::iterator map_iter;
	u8 req_cmd = INVALID_VALUE;
	VD_BOOL b_thread_running = FALSE;
	
	plock4param->Lock();
	
	map_iter = map_pstream.find(stream_id);
	if (map_iter == map_pstream.end())
	{
		ERR_PRINT("stream_id(%d) not found in map\n", stream_id);

		plock4param->Unlock();
		return -EPARAM;
	}
	
	pstream = map_iter->second;	
	if (NULL == pstream)
	{
		ERR_PRINT("stream_id(%d) NULL == pstream\n", stream_id);

		plock4param->Unlock();
		return -EPARAM;
	}
	
	pstream->plock4param->Lock();

	dev_type = pstream->dev_type;
	dev_ip = pstream->dev_ip;
	req_cmd = pstream->req.command;
	
	pstream->b_thread_exit = TRUE;
	b_thread_running = pstream->b_thread_running;

	in.s_addr = dev_ip;
	
	if (EM_STREAM_STATUS_WAIT_DEL != pstream->status)
	{
		ERR_PRINT("dev ip: %s, stream_id: %d, EM_STREAM_STATUS_WAIT_DEL != pstream->status(%d)\n",
			inet_ntoa(in), stream_id, pstream->status);

		pstream->plock4param->Unlock();
		plock4param->Unlock();
		return -EPARAM;
	}

	pstream->plock4param->Unlock();
	
	if (b_thread_running)
	{
		ret = pstream->sem_exit.TimedPend(5);
		
		DBG_PRINT("dev ip: %s, stream_id: %d, req cmd: %d, stream thread exit!!!, sem_exit pend ret: %d\n",
			inet_ntoa(in), stream_id, req_cmd, ret);
	}

	//��ȫ��
	map_pstream.erase(stream_id);//�Ƴ�
	
	delete pstream;//�ͷ�
	pstream = NULL;

	plock4param->Unlock();


	ret = BizDevReqStreamStop(dev_type, dev_ip, stream_id, stop_reason);
	if (ret)
	{
		ERR_PRINT("dev ip: %s, stream_id: %d, req cmd: %d, BizDevReqStreamStop failed, ret: %d\n",
			inet_ntoa(in), stream_id, req_cmd, ret);

		return ret;
	}
	
	DBG_PRINT("dev ip: %s, stream_id: %d, req cmd: %d, BizDevReqStreamStop success\n",
		inet_ntoa(in), stream_id, req_cmd);

	return SUCCESS;
}

//biz_dev ���ϴ������ر�
// 1���豸���߻�����ͨ�ų�����ã�
// 2��GUI��ɾ���豸����
//��Ҫ�ڱ�������ϱ�
int CMediaStreamManager::dealStreamMsgStop(u32 stream_id, s32 stream_errno)
{	
	// 1���������رգ����ϱ�
	// 2������ϢEM_STREAM_CMD_DEL���Ӻ�ɾ����
	int ret = SUCCESS;
	CMediaStream *pstream = NULL;
	MAP_ID_PSTREAM::iterator map_iter;
	
	CObject *obj = NULL;
	PDEAL_STATUS deal_status_cb = NULL;
	s32 stop_reason = SUCCESS;
	SBizMsg_t msg;
	

	plock4param->Lock();
	
	map_iter = map_pstream.find(stream_id);
	if (map_iter == map_pstream.end())
	{
		ERR_PRINT("stream_id(%d) not found in map\n", stream_id);

		plock4param->Unlock();
		return -EPARAM; 
	}
	
	pstream = map_iter->second;
	if (NULL == pstream)
	{
		ERR_PRINT("stream_id(%d) NULL == pstream\n", stream_id);

		plock4param->Unlock();
		return -EPARAM;
	}

	pstream->plock4param->Lock();

	obj = pstream->m_obj;
	deal_status_cb = pstream->m_deal_status_cb;
	
	if (SUCCESS == pstream->stream_errno
			&& SUCCESS != stream_errno)
	{
		 pstream->stream_errno = stream_errno;
	}

	stop_reason = pstream->stream_errno;
		
	pstream->b_thread_exit = TRUE;
	pstream->status = EM_STREAM_STATUS_WAIT_DEL;//��Ϊ��ɾ��״̬

	pstream->plock4param->Unlock();
	plock4param->Unlock();

	//���Լ�����Ϣ���Ӻ�ɾ����
	memset(&msg, 0, sizeof(SBizMsg_t));
	msg.msg_type = EM_STREAM_CMD_DEL;//ɾ����
	msg.un_part_chn.stream_id = stream_id;
	msg.un_part_data.stream_errno = stop_reason;
	ret = BizSendMsg2StreamManager(&msg, sizeof(SBizMsg_t));
	if (ret)
	{
		ERR_PRINT("BizSendMsg2StreamManager failed, ret: %d, stream_id: %d, msg_type: %d\n",
			ret, msg.un_part_chn.stream_id, msg.msg_type);
	}
	
	//�ϱ�
	//״̬�ص����ϲ�ʵ�ֽ�������Ϣ���ݣ�
	//�������������pstream->plock4param
	memset(&msg, 0, sizeof(SBizMsg_t));
	msg.msg_type = EM_STREAM_MSG_STOP;
	msg.un_part_chn.stream_id = stream_id;
	msg.un_part_data.stream_errno = stop_reason;
	
	if (obj && deal_status_cb)
	{
		ret = (obj->*deal_status_cb)(&msg, sizeof(SBizMsg_t));
		if (ret)
		{
			ERR_PRINT("deal_status_cb failed, ret: %d, stream_id: %d, msg_type: %d\n",
				ret, stream_id, msg.msg_type);
		}
	}	

	return SUCCESS;
}

int CMediaStreamManager::dealStreamMsgProgess(u32 stream_id, u32 cur_pos, u32 total_size)
{
	int ret = SUCCESS;
	struct in_addr in;
	CMediaStream *pstream = NULL;
	MAP_ID_PSTREAM::iterator map_iter;
	EM_DEV_TYPE dev_type;//����������
	u32 dev_ip = INADDR_NONE;//������IP
	u8 req_cmd = INVALID_VALUE;
	CObject *obj = NULL;
	PDEAL_STATUS deal_status_cb = NULL;
	SBizMsg_t msg;
	

	plock4param->Lock();
	
	map_iter = map_pstream.find(stream_id);
	if (map_iter == map_pstream.end())
	{
		ERR_PRINT("stream_id(%d) not found in map\n", stream_id);

		plock4param->Unlock();
		return -EPARAM; 
	}
	
	pstream = map_iter->second;
	if (NULL == pstream)
	{
		ERR_PRINT("stream_id(%d) NULL == pstream\n", stream_id);

		plock4param->Unlock();
		return -EPARAM;
	}

	pstream->plock4param->Lock();
	plock4param->Unlock();

	obj = pstream->m_obj;
	deal_status_cb = pstream->m_deal_status_cb;
		
	dev_type = pstream->dev_type;
	dev_ip = pstream->dev_ip;
	req_cmd = pstream->req.command;
	in.s_addr = dev_ip;

	pstream->plock4param->Unlock();

	memset(&msg, 0, sizeof(SBizMsg_t));
	msg.msg_type = EM_STREAM_MSG_PROGRESS;
	msg.un_part_chn.stream_id = stream_id;
	msg.un_part_data.stream_progress.cur_pos = cur_pos;
	msg.un_part_data.stream_progress.total_size = total_size;

	if (obj && deal_status_cb)
	{
		ret = (obj->*deal_status_cb)(&msg, sizeof(SBizMsg_t));
		if (ret)
		{
			ERR_PRINT("deal_status_cb failed, ret: %d, stream_id: %d, msg_type: %d\n",
				ret, stream_id, msg.msg_type);
		}
	}

	DBG_PRINT("dev ip: %s, stream_id: %d, req cmd: %d, cur_pos: %u, total_size: %u\n",
			inet_ntoa(in), stream_id, req_cmd, cur_pos, total_size);
	
	
	return SUCCESS;
}

int CMediaStreamManager::dealStreamMsgFinish(u32 stream_id)
{
	int ret = SUCCESS;
	struct in_addr in;
	CMediaStream *pstream = NULL;
	MAP_ID_PSTREAM::iterator map_iter;
	EM_DEV_TYPE dev_type;//����������
	u32 dev_ip = INADDR_NONE;//������IP
	u8 req_cmd = INVALID_VALUE;
	CObject *obj = NULL;
	PDEAL_STATUS deal_status_cb = NULL;
	SBizMsg_t msg;
	

	plock4param->Lock();
	
	map_iter = map_pstream.find(stream_id);
	if (map_iter == map_pstream.end())
	{
		ERR_PRINT("stream_id(%d) not found in map\n", stream_id);

		plock4param->Unlock();
		return -EPARAM; 
	}
	
	pstream = map_iter->second;
	if (NULL == pstream)
	{
		ERR_PRINT("stream_id(%d) NULL == pstream\n", stream_id);

		plock4param->Unlock();
		return -EPARAM;
	}

	pstream->plock4param->Lock();
	plock4param->Unlock();
	
	obj = pstream->m_obj;
	deal_status_cb = pstream->m_deal_status_cb;
		
	dev_type = pstream->dev_type;
	dev_ip = pstream->dev_ip;
	req_cmd = pstream->req.command;
	in.s_addr = dev_ip;

	pstream->plock4param->Unlock();

	memset(&msg, 0, sizeof(SBizMsg_t));
	msg.msg_type = EM_STREAM_MSG_FINISH;
	msg.un_part_chn.stream_id = stream_id;
	if (obj && deal_status_cb)
	{
		ret = (obj->*deal_status_cb)(&msg, sizeof(SBizMsg_t));
		if (ret)
		{
			ERR_PRINT("deal_status_cb failed, ret: %d, stream_id: %d, msg_type: %d\n",
				ret, stream_id, msg.msg_type);
		}
	}
	
	

	DBG_PRINT("dev ip: %s, stream_id: %d, req cmd: %d\n",
			inet_ntoa(in), stream_id, req_cmd);
	
	return SUCCESS;
}

void CMediaStreamManager::threadMsg(uint param)//����Ϣ
{
	VD_BOOL b_process = FALSE;
	int ret = SUCCESS;
	SBizMsg_t msg;
	
	while (1)
	{
		ret = SUCCESS;
		b_process = FALSE;
		
		ret = pthread_mutex_lock(&mq_mutex);
		if (ret)
		{
			ERR_PRINT("pthread_mutex_lock failed, err(%d, %s)\n", ret, strerror(ret));
			
			break;
		}
		
		if (mq_msg_count)	//����Ϣ
		{
			memset(&msg, 0, sizeof(SBizMsg_t));
			ret = pmq_ccbuf->Get((u8 *)&msg, sizeof(SBizMsg_t));
			if (ret)
			{
				ERR_PRINT("msg_queue get msg fail(%d), reset it\n", ret);

				pmq_ccbuf->Reset();
				mq_msg_count = 0;
			}
			else
			{
				mq_msg_count--;
				b_process = TRUE;
			}
		}
		else	//����Ϣ
		{
			ret = pthread_cond_wait(&mq_cond, &mq_mutex);
			if (ret)
			{
				ERR_PRINT("pthread_cond_wait failed, err(%d, %s)\n", ret, strerror(ret));

				pthread_mutex_unlock(&mq_mutex);
				break;
			}
		}
		
		ret = pthread_mutex_unlock(&mq_mutex);
		if (ret)
		{
			ERR_PRINT("pthread_cond_wait failed, err(%d, %s)\n", ret, strerror(ret));
			
			break;
		}
		
		if (b_process)
		{
			ret = SUCCESS;
			s32 msg_type = msg.msg_type;
			DBG_PRINT("msg type: %d\n", msg_type);		
			
			switch (msg_type)
			{
				//��Ϣ
			#if 0 //�������ã�����ֵ��֪�ɹ����
				case EM_STREAM_MSG_CONNECT_SUCCESS:	//���ӳɹ�
				{
					u32 stream_id = msg.stream_id;

					ret = dealStreamMsgConnectSuccess(stream_id);
				} break;
				
				case EM_STREAM_MSG_CONNECT_FALIURE:	//����ʧ��
				{
					u32 stream_id = msg.stream_err.stream_id;
					s32 stream_errno = msg.stream_err.stream_errno;

					ret = dealStreamMsgConnectFail(stream_id, stream_errno);
				} break;
			#endif

				//biz_dev ���ϴ������ر�
				// 1���豸���߻�����ͨ�ų�����ã�
				// 2���ϲ�ɾ���豸����
				case EM_STREAM_MSG_STOP:
				{
					u32 stream_id = msg.un_part_chn.stream_id;
					s32 stream_errno = msg.un_part_data.stream_errno;

					ret = dealStreamMsgStop(stream_id, stream_errno);
					
				} break;
				
				case EM_STREAM_MSG_PROGRESS:		//�ļ��ط�/���ؽ��ȣ�biz_dev ���ϴ�
				{
					ret = dealStreamMsgProgess(msg.un_part_chn.stream_id,
												msg.un_part_data.stream_progress.cur_pos,
												msg.un_part_data.stream_progress.total_size);
				} break;
				
				case EM_STREAM_MSG_FINISH:		//�ļ�������ɣ��ϲ㴫������
				{
					ret = dealStreamMsgFinish(msg.un_part_chn.stream_id);
				} break;
				
				
				//CMediaStreamManager �ڲ�����
				case EM_STREAM_CMD_CONNECT:	//������
				{
					ret = dealStreamConnect(msg.un_part_chn.stream_id);
					
				} break;

				//�ϲ���õ����رգ������������رգ�Ҳ�������ϲ�����ر�
				case EM_STREAM_CMD_DEL:	//ɾ����
				{
					u32 stream_id = msg.un_part_chn.stream_id;
					s32 stream_errno = msg.un_part_data.stream_errno;
					ret = dealStreamDel(stream_id, stream_errno);
					
				} break;

				default:
					ERR_PRINT("msg_type: %d, not support\n", msg_type);
			}
		}
	}

thread_exit:
	
	ERR_PRINT("CMediaStreamManager::threadMsg exit, inconceivable\n");
}


int CMediaStreamManager::ReqStreamStart(
	EM_DEV_TYPE dev_type,
	u32 dev_ip,
	ifly_TCP_Stream_Req *preq,
	CObject *obj,
	PDEAL_FRAME deal_frame_cb,
	PDEAL_STATUS deal_status_cb,
	u32 *pstream_id )
{
	int ret = SUCCESS;
	CMediaStream *pstream = NULL;
	struct in_addr in;
	in.s_addr = dev_ip;

	if (NULL == preq 		
		|| NULL == obj
		|| NULL == deal_frame_cb
		|| NULL == deal_status_cb
		|| NULL == pstream_id)
	{
		ERR_PRINT("param invalid\n");
		
		return -EPARAM;
	}
	
	if (!b_inited)
	{
		ERR_PRINT("module not init\n");
		return -FAILURE;
	}

	pstream = new CMediaStream;
	ret = pstream->Init();
	if (ret)
	{
		ERR_PRINT("dev ip: %s, cmd: %d, CMediaStream init failed, ret: %d\n", inet_ntoa(in), preq->command, ret);

		return ret;
	}

	pstream->dev_type = dev_type;
	pstream->dev_ip = dev_ip;
	pstream->req = *preq;
	pstream->m_obj = obj;
	pstream->m_deal_frame_cb = deal_frame_cb;//��ע���֡���ݴ�����
	pstream->m_deal_status_cb = deal_status_cb;//��ע���״̬������
	
	plock4param->Lock();

	pstream->stream_id = _createStreamID();
	*pstream_id = pstream->stream_id;
	
	//insert map
	if (!map_pstream.insert(std::make_pair(pstream->stream_id, pstream)).second)
	{
		ERR_PRINT("dev ip: %s, cmd: %d, map insert failed\n", inet_ntoa(in), preq->command);

		goto fail;
	}
	
	//write msg to threadmsg, in threadmsg connect
	SBizMsg_t msg;
	memset(&msg, 0, sizeof(SBizMsg_t));
	msg.msg_type = EM_STREAM_CMD_CONNECT;
	msg.un_part_chn.stream_id = *pstream_id;

	ret = WriteMsg(&msg, sizeof(SBizMsg_t));
	if (ret)
	{
		ERR_PRINT("dev ip: %s, stream req cmd: %d, WriteMsg failed, ret: %d\n", inet_ntoa(in), preq->command, ret);

		goto fail2;
	}	

	plock4param->Unlock();
	return SUCCESS;

fail2:

	map_pstream.erase(*pstream_id);

fail:

	plock4param->Unlock();
	
	if (pstream)
	{
		delete pstream;
		pstream = NULL;
	}

	*pstream_id = INVALID_VALUE;
	
	return -FAILURE;
}

//�ϲ���õ����رգ������������رգ�Ҳ�������ϲ�����ر�
//֮�����ڲ�threadMsg  �߳���ɾ����
int CMediaStreamManager::ReqStreamStop(u32 stream_id, s32 stop_reason)
{
	int ret = SUCCESS;
	struct in_addr in;
	CMediaStream *pstream = NULL;
	MAP_ID_PSTREAM::iterator map_iter;
	SBizMsg_t msg;
	
		
	if (!b_inited)
	{
		ERR_PRINT("module not init\n");
		return -FAILURE;
	}

#if 1	
	plock4param->Lock();
	
	map_iter = map_pstream.find(stream_id);
	if (map_iter == map_pstream.end())
	{
		ERR_PRINT("stream_id(%d) not found in map\n", stream_id);
		
		plock4param->Unlock();
		return -EPARAM; 
	}
	
	pstream = map_iter->second;
	if (NULL == pstream)
	{
		ERR_PRINT("stream_id(%d) NULL == pstream\n", stream_id);

		plock4param->Unlock();
		return -EPARAM;
	}

	pstream->plock4param->Lock();

	in.s_addr = pstream->dev_ip;
	pstream->status = EM_STREAM_STATUS_WAIT_DEL;//��Ϊ��ɾ��״̬
	if (SUCCESS == pstream->stream_errno
		&& SUCCESS != stop_reason)
	{
		 pstream->stream_errno = stop_reason;
	}

	pstream->plock4param->Unlock();
	plock4param->Unlock();


	//write msg to threadmsg, in threadmsg delete
	memset(&msg, 0, sizeof(SBizMsg_t));
	msg.msg_type = EM_STREAM_CMD_DEL;//ɾ����
	msg.un_part_chn.stream_id = pstream->stream_id;
	msg.un_part_data.stream_errno = stop_reason;
	ret = WriteMsg(&msg, sizeof(SBizMsg_t));
	if (ret)
	{
		ERR_PRINT("WriteMsg failed, ret: %d, stream_id: %d, msg_type: %d\n",
			ret, msg.un_part_chn.stream_id, msg.msg_type);
		
		return -FAILURE;
	}

	DBG_PRINT("dev ip: %s, stream id: %d, req cmd: %d\n",
		inet_ntoa(in), pstream->stream_id, pstream->req.command);

#else
	//write msg to threadmsg, in threadmsg delete
	SBizMsg_t msg;
	memset(&msg, 0, sizeof(SBizMsg_t));
	msg.msg_type = EM_STREAM_MSG_STOP;
	msg.stream_err.stream_id = stream_id;
	msg.stream_err.stream_errno = stop_reason;

	ret = BizSendMsg2StreamManager(&msg, sizeof(SBizMsg_t));
	if (ret)
	{
		ERR_PRINT("BizSendMsg2StreamManager failed, ret: %d, stream_id: %d, msg_type: %d\n",
			ret, msg.stream_err.stream_id, msg.msg_type);
	}
#endif

	return ret;
}















//extern API
int BizStreamInit(void)
{
	return g_biz_stream_manager.Init();
}


//������API
int BizSendMsg2StreamManager(SBizMsg_t *pmsg, u32 msg_len)
{
	return g_biz_stream_manager.WriteMsg(pmsg, msg_len);
}

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
	u32 *pstream_id )
{
	ifly_TCP_Stream_Req req;

	memset(&req, 0, sizeof(ifly_TCP_Stream_Req));

	if (NULL == file_name || NULL == pstream_id)
	{
		return -EPARAM;
	}

	if (strlen(file_name) >= sizeof(req.FilePlayBack_t.filename))
	{
		ERR_PRINT("file_name len(%d) too long\n", strlen(file_name));
		return -EPARAM;
	}

	//0��Ԥ�� 1���ļ��ط� 2��ʱ��ط� 3���ļ����� 4������ 
	//5 VOIP 6 �ļ���֡���� 7 ʱ�䰴֡���� 8 ͸��ͨ��
	//9 Զ�̸�ʽ��Ӳ�� 10 ������ץͼ 11 ��·��ʱ��ط� 12 ��ʱ�������ļ�
	req.command = 1;
	strcpy(req.FilePlayBack_t.filename, file_name);
	req.FilePlayBack_t.offset = offset;
	
	return g_biz_stream_manager.ReqStreamStart(dev_type, dev_ip, &req, obj, deal_frame_cb, deal_status_cb, pstream_id);
}

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
	u32 *pstream_id )
{
	ifly_TCP_Stream_Req req;

	memset(&req, 0, sizeof(ifly_TCP_Stream_Req));

	//0��Ԥ�� 1���ļ��ط� 2��ʱ��ط� 3���ļ����� 4������ 
	//5 VOIP 6 �ļ���֡���� 7 ʱ�䰴֡���� 8 ͸��ͨ��
	//9 Զ�̸�ʽ��Ӳ�� 10 ������ץͼ 11 ��·��ʱ��ط� 12 ��ʱ�������ļ�
	req.command = 2;
	req.TimePlayBack_t.channel = chn;
	req.TimePlayBack_t.type = 0xff;//ȫ������
	req.TimePlayBack_t.start_time = start_time;
	req.TimePlayBack_t.end_time = end_time;
	
	return g_biz_stream_manager.ReqStreamStart(dev_type, dev_ip, &req, obj, deal_frame_cb, deal_status_cb, pstream_id);
}

//��ֻ�����ϲ�ɾ��
int BizStreamReqStop(u32 stream_id, s32 stop_reason)
{
	return g_biz_stream_manager.ReqStreamStop(stream_id, stop_reason);
}


