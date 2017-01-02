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

//CMediaStream **************************************************************
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
	
}












//CMediaStreamManager *******************************************************
//u32:stream_id
typedef std::map<u32, CMediaStream*> MAP_ID_PSTREAM;
#define g_biz_stream_manager (*CMediaStreamManager::instance())
#define MAX_STREAM_NO (1<<20)//ע�������2�Ĵ���

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

	int ReqStreamStop(u32 stream_id);
	
	// д��Ϣto stream manager
	int WriteMsg(SBizMsg_t *pmsg, u32 msg_len);
	
private: //function
	CMediaStreamManager();
	CMediaStreamManager(CMediaStreamManager &)
	{

	}
	void FreeSrc();//�ͷ���Դ
	int dealStreamConnect(u32 stream_id);
	int dealStreamDel(u32 stream_id);
	int dealStreamMsgStop(u32 stream_id, s32 stream_errno);
	int dealMsg(SBizMsg_t *pmsg, u32 msg_len);
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
	SBizMsg_t msg;
	
	memset(&msg, 0, sizeof(SBizMsg_t));
	msg.msg_type = EM_STREAM_MSG_CONNECT_FALIURE;
	
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
	memcpy(&req, &pstream->req, sizeof(ifly_TCP_Stream_Req));

	in.s_addr = dev_ip;

	if (EM_STREAM_STATUS_DISCONNECT != pstream->status)
	{
		ERR_PRINT("dev ip: %s, EM_STREAM_STATUS_DISCONNECT != pstream->status(%d)",
			inet_ntoa(in), pstream->status);

		pstream->plock4param->Unlock();
		plock4param->Unlock();
		return -EPARAM;
	}
	
	ret = BizDevReqStreamStart(dev_type, dev_ip, stream_id, &req, &sock_stream);
	if (ret)//failed
	{				
		ERR_PRINT("dev ip: %s, stream_id: %d, req cmd: %d, BizReqStreamStart failed, ret: %d\n",
			inet_ntoa(in), stream_id, req.command, ret);

		//�ڲ�����
		pstream->stream_errno = ret;

		//����Ϣ
		msg.stream_err.stream_id = stream_id;
		msg.stream_err.stream_errno = ret;
	}
	else//success
	{
		DBG_PRINT("dev ip: %s, stream_id: %d, req cmd: %d, BizReqStreamStart success\n",
			inet_ntoa(in), stream_id, req.command);

		//�ڲ�����
		pstream->sock_stream = sock_stream;
		pstream->status = EM_STREAM_STATUS_RUNNING;
		pstream->stream_errno = SUCCESS;
		
		//��������ȡ�߳�
		pstream->b_thread_running = TRUE;
		pstream->m_threadlet_rcv.run("BizStream", pstream, (ASYNPROC)&CMediaStream::threadRcv, 0, 0);

		//����Ϣ
		msg.msg_type = EM_STREAM_MSG_CONNECT_SUCCESS;
		msg.stream_id = stream_id;
	}
	
	//״̬�ص����ϲ�ʵ�ֽ�������Ϣ���ݣ�
	//�������������pstream->plock4param
	if (pstream->m_obj && pstream->m_deal_status_cb)
		(pstream->m_obj->*(pstream->m_deal_status_cb))(&msg, sizeof(SBizMsg_t));
	
	pstream->plock4param->Unlock();
	plock4param->Unlock();

	return ret;
}

int CMediaStreamManager::dealStreamDel(u32 stream_id)
{
	int ret = SUCCESS;
	struct in_addr in;
	EM_DEV_TYPE dev_type;//����������
	u32 dev_ip = INADDR_NONE;//������IP
	CMediaStream *pstream = NULL;
	MAP_ID_PSTREAM::iterator map_iter;
	u8 req_cmd = INVALID_VALUE;
	VD_BOOL b_thread_running = FALSE;
	CObject *obj = NULL;
	PDEAL_STATUS deal_status_cb = NULL;
	SBizMsg_t msg;
	memset(&msg, 0, sizeof(SBizMsg_t));
	msg.msg_type = EM_STREAM_MSG_DEL_FALIURE;
	
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
	obj = pstream->m_obj;
	deal_status_cb = pstream->m_deal_status_cb;

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

	ret = BizDevReqStreamStop(dev_type, dev_ip, stream_id);
	if (ret)
	{
		ERR_PRINT("dev ip: %s, stream_id: %d, BizDevReqStreamStop failed, ret: %d\n",
			inet_ntoa(in), ret, stream_id);

		//����Ϣ
		msg.stream_err.stream_id = stream_id;
		msg.stream_err.stream_errno = ret;

		if (obj && deal_status_cb)
			(obj->*deal_status_cb)(&msg, sizeof(SBizMsg_t));

		plock4param->Unlock();
		return ret;
	}
	
	DBG_PRINT("dev ip: %s, stream_id: %d, BizDevReqStreamStop success\n", inet_ntoa(in), stream_id);

	//����Ϣ
	msg.msg_type = EM_STREAM_MSG_DEL_SUCCESS;
	msg.stream_id = stream_id;

	if (b_thread_running)
	{
		pstream->sem_exit.TimedPend(5);
		
		DBG_PRINT("dev ip: %s, stream_id: %d, req cmd: %d, stream thread exit!!!\n",
			inet_ntoa(in), stream_id, req_cmd);
	}

	//��ȫ��
	map_pstream.erase(stream_id);//�Ƴ�
	
	delete pstream;//�ͷ�
	pstream = NULL;


	if (obj && deal_status_cb)
		(obj->*deal_status_cb)(&msg, sizeof(SBizMsg_t));

	plock4param->Unlock();

	return SUCCESS;
}

//�²㴫�����������رգ��������豸���ߣ�Ҳ�����ǲ�������ر�
//������map find��ʱ���Ҳ����ǿ��ܵģ���Ϊ�ϲ��Ѿ�erase
int CMediaStreamManager::dealStreamMsgStop(u32 stream_id, s32 stream_errno)
{	
	CMediaStream *pstream = NULL;
	MAP_ID_PSTREAM::iterator map_iter;
	SBizMsg_t msg;
	memset(&msg, 0, sizeof(SBizMsg_t));
	msg.msg_type = EM_STREAM_MSG_ERR;

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
	
	//״̬�ص����ϲ�ʵ�ֽ�������Ϣ���ݣ�
	//�������������pstream->plock4param
	msg.stream_err.stream_id = stream_id;
	msg.stream_err.stream_errno = stream_errno;
	
	if (pstream->m_obj && pstream->m_deal_status_cb)
		(pstream->m_obj->*(pstream->m_deal_status_cb))(&msg, sizeof(SBizMsg_t));
	
	pstream->plock4param->Unlock();
	plock4param->Unlock();

	return SUCCESS;
}

int CMediaStreamManager::dealMsg(SBizMsg_t *pmsg, u32 msg_len)
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

	DBG_PRINT("msg type: %d\n", pmsg->msg_type);

	if (sizeof(SBizMsg_t) != msg_len)
	{
		ERR_PRINT("sizeof(SBizMsg_t)(%d) != msg_len(%d)\n",
			sizeof(SBizMsg_t), msg_len);
		
		return -EPARAM;
	}
	
	s32 msg_type = pmsg->msg_type;
	
	switch (msg_type)
	{
		//��Ϣ
	#if 0 //�������ã�����ֵ��֪�ɹ����
		case EM_STREAM_MSG_CONNECT_SUCCESS:	//���ӳɹ�
		{
		} break;
		
		case EM_STREAM_MSG_CONNECT_FALIURE:	//����ʧ��
		{
		} break;
	#endif
		case EM_STREAM_MSG_ERR:		//������
		{
		} break;
		
		case EM_STREAM_MSG_STOP:			//biz_dev ���ϴ������رգ������豸����
		{
			u32 stream_id = pmsg->stream_err.stream_id;
			s32 stream_errno = pmsg->stream_err.stream_errno;

			ret = dealStreamMsgStop(stream_id, stream_errno);
			
		} break;
		
		case EM_STREAM_MSG_PROGRESS:		//�ļ��ط�/���ؽ���
		{
		} break;
		
		case EM_STREAM_MSG_FINISH:		//�ļ��������
		{
		} break;
		
		
		//CMediaStreamManager �ڲ�����
		case EM_STREAM_CMD_CONNECT:	//������
		{
			ret = dealStreamConnect(pmsg->stream_id);
			
		} break;

		case EM_STREAM_CMD_DEL:	//ɾ����
		{
			ret = dealStreamDel(pmsg->stream_id);
			
		} break;

		default:
			ERR_PRINT("msg_type: %d, not support\n", msg_type);
	}

	return ret;
}

void CMediaStreamManager::threadMsg(uint param)//����Ϣ
{
	VD_BOOL b_process = FALSE;
	int ret = SUCCESS;
	SBizMsg_t msg;
	
	while (1)
	{
		ret = SUCCESS;
		memset(&msg, 0, sizeof(SBizMsg_t));
		b_process = FALSE;
		
		ret = pthread_mutex_lock(&mq_mutex);
		if (ret)
		{
			ERR_PRINT("pthread_mutex_lock failed, err(%d, %s)\n", ret, strerror(ret));
			
			break;
		}
		
		if (mq_msg_count)	//����Ϣ
		{
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
			dealMsg(&msg, sizeof(SBizMsg_t));
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

	if (NULL == preq || NULL == pstream_id)
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
	memcpy(&pstream->req, preq, sizeof(ifly_TCP_Stream_Req));
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

		plock4param->Unlock();
		goto fail;
	}

	plock4param->Unlock();
	
	//write msg to threadmsg, in threadmsg connect
	SBizMsg_t msg;
	memset(&msg, 0, sizeof(SBizMsg_t));
	msg.msg_type = EM_STREAM_CMD_CONNECT;
	msg.stream_id = *pstream_id;

	ret = WriteMsg(&msg, sizeof(SBizMsg_t));
	if (ret)
	{
		ERR_PRINT("dev ip: %s, stream req cmd: %d, WriteMsg failed, ret: %d\n", inet_ntoa(in), preq->command, ret);

		goto fail2;
	}	

	return SUCCESS;

fail2:
	plock4param->Lock();

	map_pstream.erase(*pstream_id);
		
	plock4param->Unlock();

fail:
	if (pstream)
	{
		delete pstream;
		pstream = NULL;
	}

	*pstream_id = INVALID_VALUE;
	
	return -FAILURE;
}

int CMediaStreamManager::ReqStreamStop(u32 stream_id)
{
	int ret = SUCCESS;
	CMediaStream *pstream = NULL;
	MAP_ID_PSTREAM::iterator map_iter;
	struct in_addr in;
	
	if (!b_inited)
	{
		ERR_PRINT("module not init\n");
		return -FAILURE;
	}
	
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
	pstream->status = EM_STREAM_STATUS_WAIT_DEL;
	
	//write msg to threadmsg, in threadmsg delete
	SBizMsg_t msg;
	memset(&msg, 0, sizeof(SBizMsg_t));
	msg.msg_type = EM_STREAM_CMD_DEL;
	msg.stream_id = pstream->stream_id;

	ret = WriteMsg(&msg, sizeof(SBizMsg_t));
	if (ret)
	{
		ERR_PRINT("dev ip: %s, msg type: %d, WriteMsg failed, ret: %d\n", inet_ntoa(in), msg.msg_type, ret);

		pstream->plock4param->Unlock();
		plock4param->Unlock();
		return -FAILURE;
	}

	pstream->plock4param->Unlock();
	plock4param->Unlock();
	
	return SUCCESS;
}















//extern API
//������API
//pstream_id ������ID
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

//��ֻ�����ϲ�ɾ��
int BizStreamReqStop(u32 stream_id)
{
	return g_biz_stream_manager.ReqStreamStop(stream_id);
}


int BizSendMsg2StreamManager(SBizMsg_t *pmsg, u32 msg_len)
{
	return g_biz_stream_manager.WriteMsg(pmsg, msg_len);
}

