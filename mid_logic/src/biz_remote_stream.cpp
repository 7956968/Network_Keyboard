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
, err_code(SUCCESS)				//������
, status(EM_STREAM_STATUS_DISCONNECT) //��״̬

//������
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

	// д��Ϣto stream manager
	int WriteMsg(SStreamMsg_t *pmsg, u32 msg_len);
	
private: //function
	CMediaStreamManager();
	CMediaStreamManager(CMediaStreamManager &)
	{

	}
	void FreeSrc();//�ͷ���Դ
	
	
	int dealMsg(SStreamMsg_t *pmsg, u32 msg_len);
	void threadMsg(uint param);//��Ϣ�߳�ִ�к���
	//�ⲿ����
	u32 _createStreamID();//�õ�һ���µ���ID
	
private: //data member
	C_Lock *plock4param;//mutex

	VD_BOOL b_inited;
	MAP_ID_PSTREAM map_pstream;
	u32 round_stream_no;//[0, MAX_STREAM_NO-1] ��ת
	u32 stream_cnt;
	std::list<u32> list_delete;//��ɾ������ID �б�
	
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

int CMediaStreamManager::WriteMsg(SStreamMsg_t *pmsg, u32 msg_len)
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

	if (sizeof(SStreamMsg_t) != msg_len)
	{
		ERR_PRINT("sizeof(SStreamMsg_t)(%d) != msg_len(%d)\n",
			sizeof(SStreamMsg_t), msg_len);
		
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

int CMediaStreamManager::dealMsg(SStreamMsg_t *pmsg, u32 msg_len)
{
	int ret = SUCCESS;
	struct in_addr in;

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

	if (sizeof(SStreamMsg_t) != msg_len)
	{
		ERR_PRINT("sizeof(SStreamMsg_t)(%d) != msg_len(%d)\n",
			sizeof(SStreamMsg_t), msg_len);
		
		return -EPARAM;
	}
	
	s32 msg_type = pmsg->msg_type;
	u32 stream_id = pmsg->stream_id;
	MAP_ID_PSTREAM::iterator map_iter;
	CMediaStream *pstream = NULL;

	plock4param->Lock();
	
	map_iter = map_pstream.find(stream_id);
	if (map_iter == map_pstream.end())
	{
		ERR_PRINT("stream_id(%d) not found in map\n", stream_id);

		plock4param->Unlock();
		return -FAILURE;
	}
	pstream = map_iter->second;
	
	plock4param->Unlock();
	
	switch (msg_type)
	{
		//��Ϣ
		case EM_STREAM_MSG_CONNECT_SUCCESS:	//���ӳɹ�
		{
		} break;
		
		case EM_STREAM_MSG_CONNECT_FALIURE:	//����ʧ��
		{
		} break;
		
		case EM_STREAM_MSG_STREAM_ERR:		//������
		{
		} break;
		
		case EM_STREAM_MSG_STUTDOWN:			//���ر�
		{
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
			EM_DEV_TYPE dev_type;//����������
			u32 dev_ip;//������IP
			ifly_TCP_Stream_Req req;//���������ݽṹ
			
			pstream->plock4param->Lock();

			if (pstream->status != EM_STREAM_STATUS_DISCONNECT)
			{
				ERR_PRINT("pstream->status(%d) != EM_STREAM_STATUS_DISCONNECT(%d)",
					pstream->status, EM_STREAM_STATUS_DISCONNECT);

				pstream->plock4param->Unlock();
				return -EPARAM;
			}

			dev_type = pstream->dev_type;
			dev_ip = pstream->dev_ip;
			memcpy(&req, &pstream->req, sizeof(ifly_TCP_Stream_Req));

			in.s_addr = dev_ip;
			SStreamMsg_t msg;
			memset(&msg, 0, sizeof(SStreamMsg_t));

			s32 sock_stream = INVALID_VALUE;
			
			ret = BizReqStreamStart(dev_type, dev_ip, stream_id, &req, &sock_stream);
			if (ret)
			{				
				ERR_PRINT("dev ip: %s, cmd: %d, BizReqStreamStart failed, ret: %d\n",
					inet_ntoa(in), req.command, ret);

				msg.msg_type = EM_STREAM_MSG_CONNECT_FALIURE;
				msg.stream_id = stream_id;
				msg.stream_errno = ret;
			}
			else
			{
				DBG_PRINT("dev ip: %s, cmd: %d, BizReqStreamStart success\n",
					inet_ntoa(in), req.command);
				//�ڲ�����
				pstream->sock_stream = sock_stream;
				
				msg.msg_type = EM_STREAM_MSG_CONNECT_SUCCESS;
				msg.stream_id = stream_id;
			}
			//״̬�ص�
			(pstream->m_obj->*(pstream->m_deal_status_cb))(stream_id, pmsg, sizeof(SStreamMsg_t));
			
			pstream->plock4param->Unlock();
		} break;

		case EM_STREAM_CMD_DEL:	//ɾ����
		{
		} break;

		default:
			ERR_PRINT("msg_type: %d, not support\n", msg_type);
	}

	return SUCCESS;
}

void CMediaStreamManager::threadMsg(uint param)//����Ϣ
{
	VD_BOOL b_process = FALSE;
	int ret = SUCCESS;
	SStreamMsg_t msg;
	
	while (1)
	{
		ret = SUCCESS;
		memset(&msg, 0, sizeof(SStreamMsg_t));
		b_process = FALSE;
		
		ret = pthread_mutex_lock(&mq_mutex);
		if (ret)
		{
			ERR_PRINT("pthread_mutex_lock failed, err(%d, %s)\n", ret, strerror(ret));
			
			break;
		}
		
		if (mq_msg_count)	//����Ϣ
		{
			ret = pmq_ccbuf->Get((u8 *)&msg, sizeof(SStreamMsg_t));
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
			dealMsg(&msg, sizeof(SStreamMsg_t));
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
	
	//insert map
	if (!map_pstream.insert(std::make_pair(pstream->stream_id, pstream)).second)
	{
		ERR_PRINT("dev ip: %s, cmd: %d, map insert failed\n", inet_ntoa(in), preq->command);

		plock4param->Unlock();
		goto fail;
	}

	plock4param->Unlock();
	
	//write msg to threadmsg, in threadmsg connect
	SStreamMsg_t msg;
	memset(&msg, 0, sizeof(SStreamMsg_t));
	msg.msg_type = EM_STREAM_CMD_CONNECT;
	msg.stream_id = pstream->stream_id;

	ret = WriteMsg(&msg, sizeof(SStreamMsg_t));
	if (ret)
	{
		ERR_PRINT("dev ip: %s, cmd: %d, WriteMsg failed, ret: %d\n", inet_ntoa(in), preq->command, ret);

		goto fail;
	}

	return SUCCESS;

fail:
	if (pstream)
	{
		delete pstream;
		pstream = NULL;
	}

	return -FAILURE;		
}
















//extern API
//������API
//pstream_id ������ID
int BizReqPlaybackByFile (
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

int BizSendMsg2StreamManager(SStreamMsg_t *pmsg, u32 msg_len)
{
	return g_biz_stream_manager.WriteMsg(pmsg, msg_len);
}

