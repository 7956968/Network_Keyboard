#include "api.h"
#include "singleton.h"
#include "object.h"
#include "glb_error_num.h"
#include "biz.h"
#include "biz_types.h"
#include "biz_config.h"
#include "biz_net.h"
#include "biz_device.h"
#include "biz_playback.h"
#include "biz_msg_type.h"
#include "biz_remote_stream.h"

#include "hisi_sys.h"
#include "atomic.h"
#include "c_lock.h"
#include "crwlock.h"
#include "ccircular_buffer.h"
#include "public.h"
#include "flash.h"
#include "cthread.h"
#include "ctimer.h"
#include "ctrlprotocol.h"
#include "net.h"


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


#include <map>
#include <set>
//#include <bitset>
#include <algorithm>
#include <utility>

//��ģ��::playback_id ��������biz_remote_stream::stream_id

class CBizPlaybackManager;


/*******************************************************************
CBizPlayback ����
*******************************************************************/
class CBizPlayback : public CObject
{	
	friend class CBizPlaybackManager;
	
public:
	CBizPlayback();
	~CBizPlayback();
	
	int Init(void);
	int StartByFile(u32 dev_ip, ifly_recfileinfo_t *pfile_info, u32 *pstream_id);
	int StartByTime(u32 dev_ip, u8 chn, u32 start_time, u32 end_time, u32 *pstream_id);
	//Ԥ���Ƿ��Ѿ����ڽ�����
	VD_BOOL IsStarted();
	int Stop();
	int PlaybackCtrl(SPlayback_Ctrl_t *pb_ctl);
	
	int dealFrameFunc(u32 stream_id, FRAMEHDR *pframe_hdr);
	int dealStateFunc(SBizMsg_t *pmsg, u32 len);
	
	
private:
	
	CBizPlayback(CBizPlayback &)
	{

	}
	void FreeSrc();//�ͷ���Դ
	
	
private:
	VD_BOOL b_inited;
	C_Lock *plock4param;	//mutex
	int playback_type;
	ifly_recfileinfo_t file_info;
	SPlayback_Time_Info_t time_info;

	u32 playback_chn;	//�ط�ͨ��
	u32 stream_id;	//����biz_remote_stream //�ؼ���ϵͳΨһ
	EM_STREAM_STATUS_TYPE status;	//��״̬
	s32 stream_errno;	//������
	s32 rate;		//��ǰ�����ٶ�[-8 -4 -2 1 2 4 8]  <==   1<<[-3, 3]
	u32 cur_pos;	//��ǰ����ʱ��
	u32 total_size;	//��ʱ�䳤��
};


/*******************************************************************
CBizPlayback ����
*******************************************************************/
CBizPlayback::CBizPlayback()
: b_inited(FALSE)
, plock4param(NULL)
, playback_type(EM_PLAYBACK_NONOE)
, playback_chn(INVALID_VALUE)	//�ط�ͨ��
, stream_id(INVALID_VALUE)	//�ؼ���ϵͳΨһ
, status(EM_STREAM_STATUS_DISCONNECT) //��״̬
, stream_errno(SUCCESS) 	//������
, rate(1)					//��������
, cur_pos(INVALID_VALUE);	//��ǰ����ʱ��
, total_size(INVALID_VALUE); //��ʱ�䳤��

{
	memset(&file_info, 0, sizeof(ifly_recfileinfo_t));
	memset(&time_info, 0, sizeof(SPlayback_Time_Info_t));
}

int CBizPlayback::Init(void)
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

	b_inited = TRUE;
	
	return SUCCESS;
	
fail:

	FreeSrc();
	return -FAILURE;
}

void CBizPlayback::FreeSrc()//�ͷ���Դ
{
	b_inited = FALSE;
	
	if (plock4param)
	{
		delete plock4param;
		plock4param = NULL;
	}
}

CBizPlayback::~CBizPlayback()
{
	FreeSrc();
}


int CBizPlayback::StartByFile(u32 dev_ip, ifly_recfileinfo_t *pfile_info, u32 *pstream_id)
{
	if (!b_inited)
	{
		ERR_PRINT("module not inited\n");
		return -FAILURE;
	}
	
	int ret = SUCCESS;
	
	plock4param->Lock();

	if (b_connect)//�Ѿ�����Ԥ��
	{
		DBG_PRINT("module has been started, stop before\n");
		
		ret = Stop();
		if (ret)
		{
			ERR_PRINT("Stop failed, ret: %d\n", ret);
			
			plock4param->Unlock();
			return ret;
		}
	}

	playback_type = EM_PLAYBACK_FILE;
	file_info = *pfile_info;

	//base class
	dev_type = EM_NVR;
	dev_ip = _dev_ip;
	memset(&req, 0, sizeof(ifly_TCP_Stream_Req));
	//0��Ԥ�� 1���ļ��ط� 2��ʱ��ط� 3���ļ����� 4������ 
	//5 VOIP 6 �ļ���֡���� 7 ʱ�䰴֡���� 8 ͸��ͨ��
	//9 Զ�̸�ʽ��Ӳ�� 10 ������ץͼ 11 ��·��ʱ��ط� 12 ��ʱ�������ļ�
	req.command = 1;
	strcpy(req.FilePlayBack_t.filename, pfile_info->filename);
	req.FilePlayBack_t.offset = htonl(pfile_info->offset);
	
	ret = Start();
	if (ret)
	{
		ERR_PRINT("Start failed, ret: %d\n", ret);
		plock4param->Unlock();
		return ret;
	}

	//����
	ret = _StreamProgress(TRUE);//���ս�����Ϣ
	if (ret)
	{
		ERR_PRINT("BizDevStreamProgress failed, ret: %d\n", ret);

		if (Stop())
		{
			ERR_PRINT("Stop failed\n");
		}
		
		plock4param->Unlock();
		return ret;
	}
	
	plock4param->Unlock();
	
	return SUCCESS;
}

int CBizPlayback::StartByTime(u32 dev_ip, u8 chn, u32 start_time, u32 end_time, u32 *pstream_id)
{
	if (!b_inited)
	{
		ERR_PRINT("module not inited\n");
		return -FAILURE;
	}
	
	int ret = SUCCESS;
	
	plock4param->Lock();

	if (b_connect)//�Ѿ�����Ԥ��
	{
		DBG_PRINT("module has been started, stop before\n");
		
		ret = Stop();
		if (ret)
		{
			ERR_PRINT("Stop failed, ret: %d\n", ret);
			
			plock4param->Unlock();
			return ret;
		}
	}
	
	playback_type = EM_PLAYBACK_TIME;
	time_info.chn = chn;
	time_info.type = 0xff;//�����ļ�����
	time_info.start_time = start_time;
	time_info.end_time = end_time;

	//base class
	dev_type = EM_NVR;
	dev_ip = _dev_ip;
	memset(&req, 0, sizeof(ifly_TCP_Stream_Req));
	//0��Ԥ�� 1���ļ��ط� 2��ʱ��ط� 3���ļ����� 4������ 
	//5 VOIP 6 �ļ���֡���� 7 ʱ�䰴֡���� 8 ͸��ͨ��
	//9 Զ�̸�ʽ��Ӳ�� 10 ������ץͼ 11 ��·��ʱ��ط� 12 ��ʱ�������ļ�
	req.command = 2;
	req.TimePlayBack_t.channel = time_info.chn;
	req.TimePlayBack_t.type = htons(time_info.type);
	req.TimePlayBack_t.start_time = htonl(time_info.start_time);
	req.TimePlayBack_t.end_time = htonl(time_info.end_time);	
	
	ret = Start();
	if (ret)
	{
		ERR_PRINT("Start failed, ret: %d\n", ret);
		plock4param->Unlock();
		return ret;
	}
	
	plock4param->Unlock();
	
	return SUCCESS;
}


//�Ƿ��Ѿ����ڽ�����
VD_BOOL CBizPlayback::IsStarted()
{	
	if (!b_inited)
	{
		ERR_PRINT("module not inited\n");
		return -FAILURE;
	}
	
	plock4param->Lock();

	VD_BOOL b = b_connect;
		
	plock4param->Unlock();

	return b;
}

int CBizPlayback::Stop()
{
	if (!b_inited)
	{
		ERR_PRINT("module not inited\n");
		return -FAILURE;
	}
	
	int ret = SUCCESS;
	
	plock4param->Lock();

	if (b_connect)
	{
		ret = Stop();
		if (ret)
		{
			ERR_PRINT("PlaybackStop failed, ret: %d\n", ret);
			
			plock4param->Unlock();
			return ret;
		}

		b_connect = FALSE;
		dev_type = EM_DEV_TYPE_NONE;
		dev_ip = INADDR_NONE;
		stream_idx = INVALID_VALUE;
		memset(&req, 0, sizeof(ifly_TCP_Stream_Req));
	}	
	
	plock4param->Unlock();
	
	return SUCCESS;
}

int CBizPlayback::PlaybackCtrl(SPlayback_Ctrl_t *pb_ctl)
{
	
}















/*******************************************************************
CBizPlaybackManager ����
*******************************************************************/

#define g_biz_playback_manager (*CBizPlaybackManager::instance())

//playback_chn, stream_id
typedef std::map<u32, u32> MAP_PBChn_SID;

//u32:stream_id
typedef std::map<u32, CBizPlayback*> MAP_ID_PCPLAYBACK;


class CBizPlaybackManager : public CObject
{	
public:
	PATTERN_SINGLETON_DECLARE(CBizPlaybackManager);
	~CBizPlaybackManager();
	
	int Init(void);
	// д��Ϣto stream manager
	int WriteMsg(SBizMsg_t *pmsg, u32 msg_len);
	int dealFrameFunc(u32 stream_id, FRAMEHDR *pframe_hdr);
	int dealStateFunc(SBizMsg_t *pmsg, u32 len);
	int PlaybackStartByFile(u32 playback_chn, u32 dev_ip, ifly_recfileinfo_t *pfile_info);
	int PlaybackStartByTime(u32 playback_chn, u32 dev_ip, u8 chn, u32 start_time, u32 end_time);
	//Ԥ���Ƿ��Ѿ����ڽ�����
	VD_BOOL PlaybackIsStarted(u32 playback_chn);
	int PlaybackStop(u32 playback_chn);
	int PlaybackCtrl(u32 playback_chn, SPlayback_Ctrl_t *pb_ctl);
	
private:
	CBizPlaybackManager();
	CBizPlaybackManager(CBizPlaybackManager &)
	{

	}
	void FreeSrc();//�ͷ���Դ
	void threadMsg(uint param);//��Ϣ�߳�ִ�к���
	int dealStreamMsgConnectSuccess(u32 stream_id);
	int dealStreamMsgConnectFail(u32 stream_id, s32 stream_errno);
	int dealStreamMsgStop(u32 stream_id, s32 stream_errno);
	int dealStreamMsgProgess(u32 stream_id,u32 cur_pos, u32 total_size);
	int dealStreamMsgFinish(u32 stream_id);
	
private:
	VD_BOOL b_inited;
	C_Lock *plock4param;//mutex
	MAP_PBChn_SID map_pbchn_sid;
	MAP_ID_PCPLAYBACK map_pcplayback;
	u32 cplayback_cnt;
	
	//��Ϣ��������
	pthread_cond_t mq_cond;		//дflash ����������
	pthread_mutex_t mq_mutex;	//��������������
	u32 mq_msg_count;			//������pmsg_queue ����Ϣ��Ŀ
	CcircularBuffer *pmq_ccbuf;	//���λ�����
	Threadlet mq_msg_threadlet;	//��Ϣ�߳�
	
};

PATTERN_SINGLETON_IMPLEMENT(CBizPlaybackManager);


/*******************************************************************
CBizPlaybackManager ����
*******************************************************************/
CBizPlaybackManager::CBizPlaybackManager()
: b_inited(FALSE)
, plock4param(NULL)
, cplayback_cnt(0)

//��Ϣ��������
, mq_msg_count(0)
, pmq_ccbuf(NULL)
{
	
}

CBizPlaybackManager::~CBizPlaybackManager()
{
	FreeSrc();
}

int CBizPlaybackManager::Init(void)
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
	mq_msg_threadlet.run("BizPlaybackManager-mq", this, (ASYNPROC)&CBizPlaybackManager::threadMsg, 0, 0);

	b_inited = TRUE;
	return SUCCESS;
	
fail:

	FreeSrc();
	return -FAILURE;
}

void CBizPlaybackManager::FreeSrc()
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

// д��Ϣto Playback manager
int CBizPlaybackManager::WriteMsg(SBizMsg_t *pmsg, u32 msg_len)
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

int CBizPlaybackManager::dealStreamMsgConnectSuccess(u32 stream_id)
{
	int ret = SUCCESS;
	u32 playback_chn = INVALID_VALUE;
	u32 stream_id = INVALID_VALUE;
	CBizPlayback *pcplayback = NULL;
	MAP_ID_PCPLAYBACK::iterator map_ppb_iter;
	SBizMsg_t msg;
	
	plock4param->Lock();

	map_ppb_iter = map_pcplayback.find(stream_id);
	if (map_ppb_iter == map_pcplayback.end())
	{
		ERR_PRINT("MAP_PBChn_SID find success, but MAP_ID_PCPLAYBACK find failed, playback_chn: %d, stream_id: %d\n",
			playback_chn, stream_id);

		ret = -EPARAM;
	}
	
	pcplayback = map_ppb_iter.second;

	pcplayback->plock4param->Lock();

	playback_chn = pcplayback->playback_chn;
	pcplayback->status = EM_STREAM_STATUS_RUNNING;
	pcplayback->stream_errno = SUCCESS;
	pcplayback->rate = 1;
	pcplayback->cur_pos = 0;

	
	plock4param->Unlock();
}

int CBizPlaybackManager::dealStreamMsgConnectFail(u32 stream_id, s32 stream_errno)
{
}

int CBizPlaybackManager::dealStreamMsgStop(u32 stream_id, s32 stream_errno)
{
}

int CBizPlaybackManager::dealStreamMsgProgess(u32 stream_id,u32 cur_pos, u32 total_size)
{
}

int CBizPlaybackManager::dealStreamMsgFinish(u32 stream_id)
{
}


void CBizPlaybackManager::threadMsg(uint param)//����Ϣ
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
			ret = SUCCESS;
			s32 msg_type = msg.msg_type;
			DBG_PRINT("msg type: %d\n", msg_type);		
			
			switch (msg_type)
			{
				//��Ϣ
				case EM_STREAM_MSG_CONNECT_SUCCESS:	//���ӳɹ�
				{
					//biz_remote_stream �ϴ�
					u32 stream_id = msg.stream_id;
					
					ret = dealStreamMsgConnectSuccess(stream_id);
				} break;
				
				case EM_STREAM_MSG_CONNECT_FALIURE:	//����ʧ��
				{
					//biz_remote_stream �ϴ�
					u32 stream_id = msg.stream_err.stream_id;
					s32 stream_errno = msg.stream_err.stream_errno;
					
					ret = dealStreamMsgConnectFail(stream_id, stream_errno);
				} break;
				
				case EM_STREAM_MSG_STOP:	//biz_dev ���ϴ������رգ������豸����
				{
					u32 stream_id = msg.stream_err.stream_id;
					s32 stream_errno = msg.stream_err.stream_errno;

					ret = dealStreamMsgStop(stream_id, stream_errno);
					
				} break;
				
				case EM_STREAM_MSG_PROGRESS:		//�ļ��ط�/���ؽ���
				{
					ret = dealStreamMsgProgess(msg.stream_progress.stream_id,
												msg.stream_progress.cur_pos,
												msg.stream_progress.total_size);
				} break;
				
				case EM_STREAM_MSG_FINISH:		//�ļ��������
				{
					ret = dealStreamMsgFinish(msg.stream_progress.stream_id);
				} break;

				default:
					ERR_PRINT("msg_type: %d, not support\n", msg_type);
			}
		}
	}

thread_exit:
	
	ERR_PRINT("CMediaStreamManager::threadMsg exit, inconceivable\n");
}

void CBizPlaybackManager::dealFrameFunc(u32 stream_id, FRAMEHDR *pframe_hdr)
{
	if (!b_inited)
	{
		ERR_PRINT("module not inited\n");
		return -FAILURE;
	}
	
	int rtn = 0;
	vdec_stream_s in_stream;
	in_stream.rsv = 0;
	in_stream.pts = pframe_hdr->m_dwTimeStamp;
	in_stream.pts *= 1000;
	in_stream.data = pframe_hdr->m_pData;
	in_stream.len = pframe_hdr->m_dwDataSize;

#if 0	
	if (pframe_hdr->m_tVideoParam.m_bKeyFrame)
	{
		DBG_PRINT("m_dwFrameID: %d\n", pframe_hdr->m_dwFrameID);
	}
#endif

	rtn = nvr_preview_vdec_write(0, &in_stream);
	if (rtn < 0)
	{
		DBG_PRINT("nvr_preview_vdec_write failed\n");
		return FAILURE;
	}
	
	return SUCCESS;
}

void CBizPlaybackManager::dealStateFunc(SBizMsg_t *pmsg, u32 len)
{
	if (!b_inited)
	{
		ERR_PRINT("module not inited\n");
		return -FAILURE;
	}
	
	DBG_PRINT("status: %d\n", status);

	SBizEventPara para;

	if (EM_STREAM_CONNECTED == status)
	{
		para.emType = EM_BIZ_EVENT_PLAYBACK_START; //�طſ�ʼ
		para.playback_para.dev_ip = dev_ip;
	}
	else if (EM_STREAM_RCV_ERR == status)
	{
		para.emType = EM_BIZ_EVENT_PLAYBACK_NETWORK_ERR; //�ط�ʱ�����������
		para.playback_para.dev_ip = dev_ip;
	}
	else if (EM_STREAM_STOP == status)
	{
		para.emType = EM_BIZ_EVENT_PLAYBACK_DONE; //�طŽ���
		para.playback_para.dev_ip = dev_ip;
	}
	else
	{
		ERR_PRINT("status: %d not support\n", status);
		return -EPARAM;
	}

	BizEventCB(&para);
	
	return SUCCESS;
}



int CBizPlaybackManager::PlaybackStartByFile(u32 playback_chn, u32 dev_ip, ifly_recfileinfo_t *pfile_info)
{
	int ret = SUCCESS;
	u32 stream_id = INVALID_VALUE;
	struct in_addr in;
	in.s_addr = dev_ip;
	
	if (NULL == pfile_info)
	{
		ERR_PRINT("NULL == pfile_info\n");
		return -EPARAM;
	}
	
	if (!b_inited)
	{
		ERR_PRINT("module not inited\n");
		return -FAILURE;
	}

	if (PlaybackIsStarted(playback_chn))
	{
		DBG_PRINT("dev_ip: %s, playback_chn: %d has been started, first stop it\n", inet_ntoa(in), playback_chn);
		
		ret = PlaybackStop(playback_chn);
		if (ret)
		{
			ERR_PRINT("dev_ip: %s, PlaybackStop playback_chn: %d failed, ret: %d\n", inet_ntoa(in), playback_chn, ret);

			return ret;
		}
	}

	u32 stream_id = INVALID_VALUE;
	CBizPlayback *pcplayback = NULL;

	pcplayback = new CBizPlayback;
	if (NULL == pcplayback)
	{
		ERR_PRINT("dev_ip: %s, playback_chn: %d, new CBizPlayback failed\n", inet_ntoa(in), playback_chn);
		
		return -ESPACE;
	}

	ret = pcplayback->Init();
	if (ret)
	{
		ERR_PRINT("dev_ip: %s, playback_chn: %d, CBizPlayback init failed, ret: %d\n", inet_ntoa(in), playback_chn, ret);
		
		ret = -ESPACE;
		goto fail;
	}	

	//�ɹ����ز�����ʾ���ӳɹ���ֻ��д������Ϣ�б�֮������Ϣ�߳�����
	//���ӳɹ��������Ϣ�ϴ�����ʱ���ս�����Ϣ
	ret = BizStreamReqPlaybackByFile (
			EM_NVR,
			dev_ip,
			pfile_info.filename,
			pfile_info.offset,
			this,
			(PDEAL_FRAME)&dealFrameFunc,
			(PDEAL_STATUS)&dealStateFunc,
			&stream_id);
	if (ret)
	{
		ERR_PRINT("dev_ip: %s, playback_chn: %d, BizStreamReqPlaybackByFile failed, ret: %d\n", inet_ntoa(in), playback_chn, ret);
		
		goto fail;
	}
	
	plock4param->Lock();
	
	if (!map_pbchn_sid.insert(std::make_pair(playback_chn, stream_id)).second)
	{
		ERR_PRINT("dev_ip: %s, playback_chn: %d, MAP_PBChn_SID insert failed\n", inet_ntoa(in), playback_chn);

		ret = -FAILURE;
		goto fail2;
	}

	if (!map_pcplayback.insert(std::make_pair(stream_id, pcplayback)).second)
	{
		ERR_PRINT("dev_ip: %s, playback_chn: %d, MAP_ID_PCPLAYBACK insert failed\n", inet_ntoa(in), playback_chn);

		
		ret = -FAILURE;
		goto fail3;
	}
	
	pcplayback->plock4param->Lock();

	pcplayback.playback_type = EM_PLAYBACK_FILE;
	pcplayback.file_info = *pfile_info;
	pcplayback.playback_chn = playback_chn;
	pcplayback.stream_id = stream_id;
	pcplayback.status = EM_STREAM_STATUS_DISCONNECT;
	pcplayback.stream_errno = SUCCESS;	

	pcplayback->plock4param->Unlock();
	
	plock4param->Unlock();
	
	return SUCCESS;

fail3:

	map_pbchn_sid.erase(playback_chn);
	
fail2:
	
	plock4param->Unlock();
	
	BizStreamReqStop(stream_id);
	
fail:
	if (pcplayback)
	{
		delete pcplayback;
		pcplayback = NULL;
	}

	return ret;
}

int CBizPlaybackManager::PlaybackStartByTime(u32 playback_chn, u32 dev_ip, u8 chn, u32 start_time, u32 end_time)
{
	int ret = SUCCESS;
	u32 stream_id = INVALID_VALUE;
	struct in_addr in;
	in.s_addr = dev_ip;
	
	if (NULL == pfile_info)
	{
		ERR_PRINT("NULL == pfile_info\n");
		return -EPARAM;
	}
	
	if (!b_inited)
	{
		ERR_PRINT("module not inited\n");
		return -FAILURE;
	}

	if (PlaybackIsStarted(playback_chn))
	{
		DBG_PRINT("dev_ip: %s, playback_chn: %d has been started, first stop it\n", inet_ntoa(in), playback_chn);
		
		ret = PlaybackStop(playback_chn);
		if (ret)
		{
			ERR_PRINT("dev_ip: %s, PlaybackStop playback_chn: %d failed, ret: %d\n", inet_ntoa(in), playback_chn, ret);

			return ret;
		}
	}

	u32 stream_id = INVALID_VALUE;
	CBizPlayback *pcplayback = NULL;

	pcplayback = new CBizPlayback;
	if (NULL == pcplayback)
	{
		ERR_PRINT("dev_ip: %s, playback_chn: %d, new CBizPlayback failed\n", inet_ntoa(in), playback_chn);
		
		return -ESPACE;
	}

	ret = pcplayback->Init();
	if (ret)
	{
		ERR_PRINT("dev_ip: %s, playback_chn: %d, CBizPlayback init failed, ret: %d\n", inet_ntoa(in), playback_chn, ret);
		
		ret = -ESPACE;
		goto fail;
	}	

	//�ɹ����ز�����ʾ���ӳɹ���ֻ��д������Ϣ�б�֮������Ϣ�߳�����
	//���ӳɹ��������Ϣ�ϴ�����ʱ���ս�����Ϣ
	ret = BizStreamReqPlaybackByTime (
			EM_NVR,
			dev_ip,
			chn,
			start_time,
			end_time,
			this,
			(PDEAL_FRAME)&dealFrameFunc,
			(PDEAL_STATUS)&dealStateFunc,
			&stream_id);
	if (ret)
	{
		ERR_PRINT("dev_ip: %s, playback_chn: %d, BizStreamReqPlaybackByTime failed, ret: %d\n", inet_ntoa(in), playback_chn, ret);
		
		goto fail;
	}
	
	plock4param->Lock();
	
	if (!map_pbchn_sid.insert(std::make_pair(playback_chn, stream_id)).second)
	{
		ERR_PRINT("dev_ip: %s, playback_chn: %d, MAP_PBChn_SID insert failed\n", inet_ntoa(in), playback_chn);

		ret = -FAILURE;
		goto fail2;
	}

	if (!map_pcplayback.insert(std::make_pair(stream_id, pcplayback)).second)
	{
		ERR_PRINT("dev_ip: %s, playback_chn: %d, MAP_ID_PCPLAYBACK insert failed\n", inet_ntoa(in), playback_chn);

		
		ret = -FAILURE;
		goto fail3;
	}
	
	pcplayback->plock4param->Lock();

	pcplayback.playback_type = EM_PLAYBACK_TIME;
	pcplayback.time_info.chn = chn;
	pcplayback.time_info.type = 0xff;
	pcplayback.time_info.start_time = start_time;
	pcplayback.time_info.end_time = end_time;
	
	pcplayback.playback_chn = playback_chn;
	pcplayback.stream_id = stream_id;
	pcplayback.status = EM_STREAM_STATUS_DISCONNECT;
	pcplayback.stream_errno = SUCCESS;	

	pcplayback->plock4param->Unlock();
	
	plock4param->Unlock();
	
	return SUCCESS;

fail3:

	map_pbchn_sid.erase(playback_chn);
	
fail2:
	
	plock4param->Unlock();
	
	BizStreamReqStop(stream_id);
	
fail:
	if (pcplayback)
	{
		delete pcplayback;
		pcplayback = NULL;
	}

	return ret;
}


//�Ƿ��Ѿ����ڽ�����
VD_BOOL CBizPlaybackManager::PlaybackIsStarted(u32 playback_chn)
{	
	VD_BOOL b = FALSE;
	
	if (!b_inited)
	{
		ERR_PRINT("module not inited\n");
		return FALSE;
	}
	
	plock4param->Lock();

	if (map_pbchn_sid.end() != map_pbchn_sid.find(playback_chn))//����
	{
		b = TRUE;
	}
	else
	{
		b = FALSE;
	}
		
	plock4param->Unlock();

	return b;
}

int CBizPlaybackManager::PlaybackStop(u32 playback_chn)
{
	if (!b_inited)
	{
		ERR_PRINT("module not inited\n");
		return -FAILURE;
	}
	
	int ret = SUCCESS;
	u32 stream_id = INVALID_VALUE;
	CBizPlayback *pcplayback = NULL;
	MAP_PBChn_SID::iterator map_id_iter;
	MAP_ID_PCPLAYBACK::iterator map_ppb_iter;
	
	plock4param->Lock();

	map_id_iter = map_pbchn_sid.find(playback_chn);
	if (map_id_iter != map_pbchn_sid.end())//���ڣ���ر�
	{
		stream_id = map_id_iter.second;
		map_ppb_iter = map_pcplayback.find(stream_id);
		if (map_ppb_iter != map_pcplayback.end())
		{
			pcplayback = map_ppb_iter.second;

			ret = BizStreamReqStop(stream_id);
			if (ret)
			{
				ERR_PRINT("BizStreamReqStop failed, ret: %d, playback_chn: %d, stream_id: %d\n",
					ret, playback_chn, stream_id);
			}

			map_pcplayback.erase(map_ppb_iter);
			map_pbchn_sid.erase(map_id_iter);

			delete pcplayback;
			pcplayback = NULL;
		}
		else
		{
			ERR_PRINT("MAP_PBChn_SID find success, but MAP_ID_PCPLAYBACK find failed, playback_chn: %d, stream_id: %d\n",
				playback_chn, stream_id);

			ret = -EPARAM;
		}
	}
	else
	{
		DBG_PRINT("MAP_PBChn_SID find failed, playback_chn: %d\n", playback_chn);

		ret = -EPARAM;
	}
	
	plock4param->Unlock();
	
	return ret;
}

int CBizPlaybackManager::PlaybackCtrl(u32 playback_chn, SPlayback_Ctrl_t *pb_ctl)
{
	
}










/*****************************************************
			�ⲿ�ӿ�ʵ��
*****************************************************/
int BizPlaybackInit(void)
{
	return g_biz_playback_manager.Init();
}

int BizSendMsg2PlaybackManager(SBizMsg_t *pmsg, u32 msg_len)
{
	return g_biz_playback_manager.WriteMsg(pmsg, msg_len);
}


//����ʱ��ȡһ��playback_id�����������ͨ����ID
int BizModulePlaybackStartByFile(u32 playback_chn, u32 dev_ip, ifly_recfileinfo_t *pfile_info)
{
	return g_biz_playback_manager.PlaybackStartByFile(playback_chn, dev_ip, pfile_info);
}

int BizModulePlaybackStartByTime(u32 playback_chn, u32 dev_ip, u8 chn, u32 start_time, u32 end_time)
{
	return g_biz_playback_manager.PlaybackStartByTime(playback_chn, dev_ip, chn, start_time, end_time);
}

//�Ƿ��Ѿ����ڽ�����
VD_BOOL BizModulePlaybackIsStarted(u32 playback_chn)
{
	return g_biz_playback_manager.PlaybackIsStarted(playback_chn);
}

int BizModulePlaybackStop(u32 playback_chn)
{
	return g_biz_playback_manager.PlaybackStop(playback_chn);
}


//���ſ���
int BizModulePlaybackCtrl(u32 playback_chn, SPlayback_Ctrl_t *pb_ctl)
{
	return g_biz_playback_manager.PlaybackCtrl(playback_chn, pb_ctl);
}




