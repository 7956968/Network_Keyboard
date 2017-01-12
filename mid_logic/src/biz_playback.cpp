#include "api.h"
#include "singleton.h"
#include "object.h"
#include "glb_error_num.h"


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

#include "biz.h"
#include "biz_net.h"
#include "biz_msg_type.h"
#include "biz_types.h"
#include "biz_config.h"
#include "biz_remote_stream.h"
#include "biz_device.h"


#include "biz_playback.h"


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
	u32 dev_ip;//������IP
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
, dev_ip(INADDR_NONE)			//������IP
, stream_id(INVALID_VALUE)	//�ؼ���ϵͳΨһ
, status(EM_STREAM_STATUS_INIT) //��״̬
, stream_errno(SUCCESS) 	//������
, rate(1)					//��������
, cur_pos(INVALID_VALUE)	//��ǰ����ʱ��
, total_size(INVALID_VALUE) //��ʱ�䳤��

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
	int dealFrameFunc(u32 stream_id, FRAMEHDR *pframe_hdr);//to BizDataCB
	int dealStateFunc(SBizMsg_t *pmsg, u32 len);//WriteMsg
	int PlaybackStartByFile(u32 playback_chn, u32 dev_ip, ifly_recfileinfo_t *pfile_info);
	int PlaybackStartByTime(u32 playback_chn, u32 dev_ip, u8 chn, u32 start_time, u32 end_time);
	//Ԥ���Ƿ��Ѿ����ڽ�����
	VD_BOOL PlaybackIsStarted(u32 playback_chn);
	int PlaybackStop(u32 playback_chn);
	//�طſ���
	int PlaybackPause(u32 playback_chn);
	int PlaybackResume(u32 playback_chn);
	int PlaybackStep(u32 playback_chn);//֡��
	int PlaybackRate(u32 playback_chn, s32 rate);//{-8, -4, -2, 1, 2, 4, 8}
	int PlaybackSeek(u32 playback_chn, u32 time);
	
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
	u32 dev_ip = INADDR_NONE;
	CBizPlayback *pcplayback = NULL;
	MAP_ID_PCPLAYBACK::iterator map_ppb_iter;
	
	plock4param->Lock();

	map_ppb_iter = map_pcplayback.find(stream_id);
	if (map_ppb_iter == map_pcplayback.end())
	{
		ERR_PRINT("MAP_PBChn_SID find success, but MAP_ID_PCPLAYBACK find failed, stream_id: %d\n", stream_id);

		ret = -EPARAM;
	}
	
	pcplayback = map_ppb_iter->second;
	if (NULL == pcplayback)
	{
		ERR_PRINT("NULL == pcplayback, stream_id: %d\n", stream_id);

		plock4param->Unlock();
		return -EPARAM;
	}

	pcplayback->plock4param->Lock();	

	//�ڲ�
	if (EM_STREAM_STATUS_DISCONNECT != pcplayback->status)
	{
		ERR_PRINT("EM_STREAM_STATUS_DISCONNECT != pcplayback->status(%d)\n", pcplayback->status);
	}
	pcplayback->status = EM_STREAM_STATUS_RUNNING;
	pcplayback->stream_errno = SUCCESS;
	pcplayback->rate = 1;
	//pcplayback->cur_pos = 0;

	//����
	dev_ip = pcplayback->dev_ip;
	playback_chn = pcplayback->playback_chn;

	pcplayback->plock4param->Unlock();
	plock4param->Unlock();//��ɾ�������������ͷŸ���

	if (playback_chn < 0x10)//�ط�
	{
		ret = BizDevStreamProgress(EM_NVR, dev_ip, stream_id, TRUE);//���ջطŽ�����Ϣ
		if (ret)
		{
			ERR_PRINT("BizDevStreamProgress, playback_chn: %d failed, ret: %d\n",
				playback_chn, ret);
		}
	}
	else	//����
	{
		DBG_PRINT("download file connect success, playback_chn: %d\n", playback_chn);
	}
#if 0
	//�ϴ�msg 2 biz manager
	SBizMsg_t msg;

	memset(&msg, 0, sizeof(SBizMsg_t));
	msg.msg_type = EM_PLAYBACK_MSG_CONNECT_SUCCESS;
	msg.un_part_chn.playback_chn = playback_chn;

	ret = BizSendMsg2BizManager(&msg, sizeof(SBizMsg_t));
	if (ret)
	{
		ERR_PRINT("BizSendMsg2StreamManager failed, ret: %d, playback_chn: %d, msg_type: %d\n",
			ret, playback_chn, msg.msg_type);
	}
#else
	SBizEventPara para;
	memset(&para, 0, sizeof(SBizEventPara));

	para.type = EM_BIZ_EVENT_PLAYBACK_START;
	para.un_part_chn.playback_chn = playback_chn;

	DBG_PRINT("BizEventCB, playback_chn: %d, msg_type: %d\n",
			playback_chn, para.type);
	
	ret = BizEventCB(&para);
	if (ret)
	{
		ERR_PRINT("BizEventCB failed, ret: %d, playback_chn: %d, msg_type: %d\n",
			ret, playback_chn, para.type);
	}

	#if 0
		ret = BizStreamReqProgress(playback_chn, TRUE);//���ջطŽ�����Ϣ
		if (ret)
		{
			ERR_PRINT("BizStreamReqProgress failed, ret: %d, playback_chn: %d, msg_type: %d\n",
				ret, playback_chn, para.type);
		}
	#endif
#endif

	return ret;
}

int CBizPlaybackManager::dealStreamMsgConnectFail(u32 stream_id, s32 stream_errno)
{
	int ret = SUCCESS;
	u32 playback_chn = INVALID_VALUE;
	CBizPlayback *pcplayback = NULL;
	MAP_ID_PCPLAYBACK::iterator map_ppb_iter;
	
	plock4param->Lock();

	map_ppb_iter = map_pcplayback.find(stream_id);
	if (map_ppb_iter == map_pcplayback.end())
	{
		ERR_PRINT("MAP_PBChn_SID find success, but MAP_ID_PCPLAYBACK find failed, stream_id: %d\n", stream_id);

		ret = -EPARAM;
	}
	
	pcplayback = map_ppb_iter->second;
	if (NULL == pcplayback)
	{
		ERR_PRINT("NULL == pcplayback, stream_id: %d\n", stream_id);

		plock4param->Unlock();
		return -EPARAM;
	}

	pcplayback->plock4param->Lock();

	if (EM_STREAM_STATUS_INIT != pcplayback->status)
	{
		ERR_PRINT("EM_STREAM_STATUS_INIT != pcplayback->status(%d)\n", pcplayback->status);
	}

	pcplayback->status = EM_STREAM_STATUS_INIT;
	
	playback_chn = pcplayback->playback_chn;

	pcplayback->plock4param->Unlock();

	//ɾ��
	map_pcplayback.erase(stream_id);
	map_pbchn_sid.erase(playback_chn);

	delete pcplayback;
	pcplayback = NULL;
	
	plock4param->Unlock();

#if 0
	//�ϴ�msg 2 biz manager
	SBizMsg_t msg;

	memset(&msg, 0, sizeof(SBizMsg_t));
	msg.msg_type = EM_PLAYBACK_MSG_CONNECT_FALIURE;
	msg.un_part_chn.playback_chn = playback_chn;
	msg.un_part_data.stream_errno = stream_errno;
	
	ret = BizSendMsg2BizManager(&msg, sizeof(SBizMsg_t));
	if (ret)
	{
		ERR_PRINT("BizSendMsg2StreamManager failed, ret: %d, playback_chn: %d, msg_type: %d\n",
			ret, playback_chn, msg.msg_type);
	}
#else
	SBizEventPara para;
	memset(&para, 0, sizeof(SBizEventPara));

	para.type = EM_BIZ_EVENT_PLAYBACK_NETWORK_ERR;
	para.un_part_chn.playback_chn = playback_chn;
	para.un_part_data.stream_errno = stream_errno;

	DBG_PRINT("BizEventCB, playback_chn: %d, msg_type: %d, stream_errno: %d\n",
			playback_chn, para.type, stream_errno);
	ret = BizEventCB(&para);
	if (ret)
	{
		ERR_PRINT("BizEventCB failed, ret: %d, playback_chn: %d, msg_type: %d\n",
			ret, playback_chn, para.type);
	}
			
#endif


	return ret;
}

int CBizPlaybackManager::dealStreamMsgStop(u32 stream_id, s32 stream_errno)
{
	int ret = SUCCESS;
	u32 playback_chn = INVALID_VALUE;
	CBizPlayback *pcplayback = NULL;
	MAP_ID_PCPLAYBACK::iterator map_ppb_iter;
	
	plock4param->Lock();

	map_ppb_iter = map_pcplayback.find(stream_id);
	if (map_ppb_iter == map_pcplayback.end())
	{
		ERR_PRINT("MAP_PBChn_SID find success, but MAP_ID_PCPLAYBACK find failed, stream_id: %d\n", stream_id);

		ret = -EPARAM;
	}
	
	pcplayback = map_ppb_iter->second;
	if (NULL == pcplayback)
	{
		ERR_PRINT("NULL == pcplayback, stream_id: %d\n", stream_id);

		plock4param->Unlock();
		return -EPARAM;
	}

	pcplayback->plock4param->Lock();

	playback_chn = pcplayback->playback_chn;

	pcplayback->plock4param->Unlock();

	//ɾ��
	map_pcplayback.erase(stream_id);
	map_pbchn_sid.erase(playback_chn);

	delete pcplayback;
	pcplayback = NULL;
	
	plock4param->Unlock();

	//�ϴ�msg 2 biz manager
#if 0
	SBizMsg_t msg;

	memset(&msg, 0, sizeof(SBizMsg_t));
	msg.msg_type = EM_PLAYBACK_MSG_STOP;
	msg.un_part_chn.playback_chn = playback_chn;
	msg.un_part_data.stream_errno = stream_errno;

	ret = BizSendMsg2BizManager(&msg, sizeof(SBizMsg_t));
	if (ret)
	{
		ERR_PRINT("BizSendMsg2StreamManager failed, ret: %d, playback_chn: %d, msg_type: %d\n",
			ret, playback_chn, msg.msg_type);
	}
#else
	SBizEventPara para;
	memset(&para, 0, sizeof(SBizEventPara));
	
	if (stream_errno)
	{
		para.type = EM_BIZ_EVENT_PLAYBACK_NETWORK_ERR;
		para.un_part_chn.playback_chn = playback_chn;
		para.un_part_data.stream_errno = stream_errno;
	}
	else//�޴�
	{
		para.type = EM_BIZ_EVENT_PLAYBACK_DONE;
		para.un_part_chn.playback_chn = playback_chn;
	}

	DBG_PRINT("BizEventCB, playback_chn: %d, msg_type: %d, stream_errno: %d\n",
			playback_chn, para.type, stream_errno);
	
	ret = BizEventCB(&para);
	if (ret)
	{
		ERR_PRINT("BizEventCB failed, ret: %d, playback_chn: %d, msg_type: %d\n",
			ret, playback_chn, para.type);
	}
				
#endif


	return ret;
}

//
int CBizPlaybackManager::dealStreamMsgProgess(u32 stream_id,u32 cur_pos, u32 total_size)
{
	int ret = SUCCESS;
	u32 playback_chn = INVALID_VALUE;
	CBizPlayback *pcplayback = NULL;
	MAP_ID_PCPLAYBACK::iterator map_ppb_iter;

	//stream_id  to  playback_chn
	plock4param->Lock();

	//��Ӧ�ĻطŽṹ������ھ��ϴ���Ϣ
	map_ppb_iter = map_pcplayback.find(stream_id);
	if (map_ppb_iter == map_pcplayback.end())
	{
		ERR_PRINT("MAP_PBChn_SID find success, but MAP_ID_PCPLAYBACK find failed, stream_id: %d\n", stream_id);

		ret = -EPARAM;
	}
	
	pcplayback = map_ppb_iter->second;
	if (NULL == pcplayback)
	{
		ERR_PRINT("NULL == pcplayback, stream_id: %d\n", stream_id);

		plock4param->Unlock();
		return -EPARAM;
	}

	pcplayback->plock4param->Lock();

	playback_chn = pcplayback->playback_chn;

	pcplayback->plock4param->Unlock();
	
	plock4param->Unlock();

	//�ϴ�msg 2 biz manager
#if 0
	SBizMsg_t msg;

	memset(&msg, 0, sizeof(SBizMsg_t));
	msg.msg_type = EM_PLAYBACK_MSG_PROGRESS;
	msg.un_part_chn.playback_chn = playback_chn;
	msg.un_part_data.stream_progress.cur_pos = cur_pos;
	msg.un_part_data.stream_progress.total_size = total_size;

	ret = BizSendMsg2BizManager(&msg, sizeof(SBizMsg_t));
	if (ret)
	{
		ERR_PRINT("BizSendMsg2StreamManager failed, ret: %d, playback_chn: %d, msg_type: %d\n",
			ret, playback_chn, msg.msg_type);
	}
#else
	SBizEventPara para;
	memset(&para, 0, sizeof(SBizEventPara));

	para.type = EM_BIZ_EVENT_PLAYBACK_RUN;
	para.un_part_chn.playback_chn = playback_chn;
	para.un_part_data.stream_progress.cur_pos = cur_pos;
	para.un_part_data.stream_progress.total_size = total_size;

	//DBG_PRINT("BizEventCB, playback_chn: %d, msg_type: %d, cur_pos: %d, total_size: %d\n",
	//		playback_chn, para.type, cur_pos, total_size);

	ret = BizEventCB(&para);
	if (ret)
	{
		ERR_PRINT("BizEventCB failed, ret: %d, playback_chn: %d, msg_type: %d\n",
			ret, playback_chn, para.type);
	}
						
#endif


	return ret;
}

//NVR�����ļ��ط�ʱֻ�ϱ����ȣ����ϱ�����
int CBizPlaybackManager::dealStreamMsgFinish(u32 stream_id)
{
	int ret = SUCCESS;
	u32 playback_chn = INVALID_VALUE;
	CBizPlayback *pcplayback = NULL;
	MAP_ID_PCPLAYBACK::iterator map_ppb_iter;

	//stream_id  to  playback_chn
	plock4param->Lock();

	//��Ӧ�ĻطŽṹ������ھ��ϴ���Ϣ
	map_ppb_iter = map_pcplayback.find(stream_id);
	if (map_ppb_iter == map_pcplayback.end())
	{
		ERR_PRINT("MAP_PBChn_SID find success, but MAP_ID_PCPLAYBACK find failed, stream_id: %d\n", stream_id);

		ret = -EPARAM;
	}
	
	pcplayback = map_ppb_iter->second;
	if (NULL == pcplayback)
	{
		ERR_PRINT("NULL == pcplayback, stream_id: %d\n", stream_id);

		plock4param->Unlock();
		return -EPARAM;
	}

	pcplayback->plock4param->Lock();

	playback_chn = pcplayback->playback_chn;

	pcplayback->plock4param->Unlock();
	
	plock4param->Unlock();

	//�ϴ�msg 2 biz manager
#if 0
	SBizMsg_t msg;

	memset(&msg, 0, sizeof(SBizMsg_t));
	msg.msg_type = EM_PLAYBACK_MSG_FINISH;
	msg.un_part_chn.playback_chn = playback_chn;

	ret = BizSendMsg2BizManager(&msg, sizeof(SBizMsg_t));
	if (ret)
	{
		ERR_PRINT("BizSendMsg2StreamManager failed, ret: %d, playback_chn: %d, msg_type: %d\n",
			ret, playback_chn, msg.msg_type);
	}

#else
	SBizEventPara para;
	memset(&para, 0, sizeof(SBizEventPara));

	para.type = EM_BIZ_EVENT_PLAYBACK_DONE;
	para.un_part_chn.playback_chn = playback_chn;

	DBG_PRINT("BizEventCB, playback_chn: %d, msg_type: %d\n",
		playback_chn, para.type);

	ret = BizEventCB(&para);
	if (ret)
	{
		ERR_PRINT("BizEventCB failed, ret: %d, playback_chn: %d, msg_type: %d\n",
			ret, playback_chn, para.type);
	}
						
#endif


	return ret;
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
					u32 stream_id = msg.un_part_chn.stream_id;
					
					ret = dealStreamMsgConnectSuccess(stream_id);
				} break;
				
				case EM_STREAM_MSG_CONNECT_FALIURE:	//����ʧ��
				{
					//biz_remote_stream �ϴ�
					u32 stream_id = msg.un_part_chn.stream_id;
					s32 stream_errno = msg.un_part_data.stream_errno;
					
					ret = dealStreamMsgConnectFail(stream_id, stream_errno);
				} break;
				
				case EM_STREAM_MSG_STOP:	//biz_dev ���ϴ������رգ������豸����
				{
					u32 stream_id = msg.un_part_chn.stream_id;
					s32 stream_errno = msg.un_part_data.stream_errno;

					ret = dealStreamMsgStop(stream_id, stream_errno);
					
				} break;
				
				case EM_STREAM_MSG_PROGRESS:		//�ļ��ط�/���ؽ���
				{
					ret = dealStreamMsgProgess(msg.un_part_chn.stream_id,
												msg.un_part_data.stream_progress.cur_pos,
												msg.un_part_data.stream_progress.total_size);
				} break;
				
				case EM_STREAM_MSG_FINISH:		//�ļ��������
				{
					ret = dealStreamMsgFinish(msg.un_part_chn.stream_id);
				} break;

				default:
					ERR_PRINT("msg_type: %d, not support\n", msg_type);
			}
		}
	}

thread_exit:
	
	ERR_PRINT("CBizPlaybackManager::threadMsg exit, inconceivable\n");
}

int CBizPlaybackManager::dealFrameFunc(u32 stream_id, FRAMEHDR *pframe_hdr)
{
	if (!b_inited)
	{
		ERR_PRINT("module not inited\n");
		return -FAILURE;
	}

	if (NULL == pframe_hdr)
	{
		ERR_PRINT("NULL == pframe_hdr\n");
		return -EPARAM;
	}

	//int ret = SUCCESS;
	u32 playback_chn = INVALID_VALUE;
	CBizPlayback *pcplayback = NULL;
	MAP_ID_PCPLAYBACK::iterator map_ppb_iter;

	//stream_id  to  playback_chn
	plock4param->Lock();

	//��Ӧ�ĻطŽṹ������ھ��ϴ���Ϣ
	map_ppb_iter = map_pcplayback.find(stream_id);
	if (map_ppb_iter == map_pcplayback.end())
	{
		ERR_PRINT("MAP_ID_PCPLAYBACK find failed, stream_id: %d\n", stream_id);

		plock4param->Unlock();
		return -EPARAM;
	}
	
	pcplayback = map_ppb_iter->second;
	if (NULL == pcplayback)
	{
		ERR_PRINT("NULL == pcplayback, stream_id: %d\n", stream_id);

		plock4param->Unlock();
		return -EPARAM;
	}

	pcplayback->plock4param->Lock();

	playback_chn = pcplayback->playback_chn;

	pcplayback->plock4param->Unlock();
	
	plock4param->Unlock();

	SBizDataPara para;
	memset(&para, 0, sizeof(SBizDataPara));
	para.type = EM_BIZ_DATA_PLAYBACK;
	para.un_part_chn.playback_chn = playback_chn;
	para.un_part_data.pframe_hdr = pframe_hdr;

	return BizDataCB(&para);
	
}

//WriteMsg
int CBizPlaybackManager::dealStateFunc(SBizMsg_t *pmsg, u32 len)
{
	int ret = SUCCESS;
	
	if (!b_inited)
	{
		ERR_PRINT("module not inited\n");
		return -FAILURE;
	}

	if (len != sizeof(SBizMsg_t))
	{
		ERR_PRINT("len(%d) != sizeof(SBizMsg_t)(%d)\n", len, sizeof(SBizMsg_t));

		return -EPARAM;
	}
	
	ret = WriteMsg(pmsg, len);
	if (ret)
	{
		ERR_PRINT("WriteMsg failed, ret: %d\n", ret);

		return ret;
	}
	
	return SUCCESS;
}



int CBizPlaybackManager::PlaybackStartByFile(u32 playback_chn, u32 dev_ip, ifly_recfileinfo_t *pfile_info)
{
	int ret = SUCCESS;
	u32 stream_id = INVALID_VALUE;
	int connect_type = 0;
	struct in_addr in;
	in.s_addr = dev_ip;

	//DBG_PRINT("start\n"); 
	
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

	if (playback_chn < 0x10) //�ط�
	{
		connect_type = 0;
	}
	else	//����
	{
		connect_type = 1;
	}
	
	//�ɹ����ز�����ʾ���ӳɹ���ֻ��д������Ϣ�б�֮������Ϣ�߳�����
	//���ӳɹ��������Ϣ�ϴ�����ʱ���ս�����Ϣ
	ret = BizStreamReqPlaybackByFile (
			EM_NVR,
			connect_type,/* 0 �ط� 1 ����*/
			dev_ip,
			pfile_info->filename,
			pfile_info->offset,
			pfile_info->size,
			this,
			(PDEAL_FRAME)&CBizPlaybackManager::dealFrameFunc,
			(PDEAL_STATUS)&CBizPlaybackManager::dealStateFunc,
			&stream_id);
	if (ret)
	{
		ERR_PRINT("dev_ip: %s, playback_chn: %d, BizStreamReqPlaybackByFile failed, ret: %d\n", inet_ntoa(in), playback_chn, ret);
		
		goto fail;
	}

	//DBG_PRINT("lock\n");
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

	pcplayback->playback_type = EM_PLAYBACK_FILE;
	pcplayback->file_info = *pfile_info;
	pcplayback->playback_chn = playback_chn;
	pcplayback->dev_ip = dev_ip;
	pcplayback->stream_id = stream_id;

	if (EM_STREAM_STATUS_INIT != pcplayback->status)
	{
		ERR_PRINT("EM_STREAM_STATUS_INIT != pcplayback->status(%d)\n", pcplayback->status);
	}
	pcplayback->status = EM_STREAM_STATUS_DISCONNECT;
	pcplayback->stream_errno = SUCCESS;	

	pcplayback->plock4param->Unlock();
	
	plock4param->Unlock();

	//DBG_PRINT("end\n");
	
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
	int connect_type = 0;
	
	struct in_addr in;
	in.s_addr = dev_ip;
	
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

	if (playback_chn < 0x10) //�ط�
	{
		connect_type = 0;
	}
	else	//����
	{
		connect_type = 1;
	}

	//�ɹ����ز�����ʾ���ӳɹ���ֻ��д������Ϣ�б�֮������Ϣ�߳�����
	//���ӳɹ��������Ϣ�ϴ�����ʱ���ս�����Ϣ
	ret = BizStreamReqPlaybackByTime (
			EM_NVR,
			connect_type,/* 0 �ط� 1 ����*/
			dev_ip,
			chn,
			start_time,
			end_time,
			this,
			(PDEAL_FRAME)&CBizPlaybackManager::dealFrameFunc,
			(PDEAL_STATUS)&CBizPlaybackManager::dealStateFunc,
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

	pcplayback->playback_type = EM_PLAYBACK_TIME;
	pcplayback->time_info.chn = chn;
	pcplayback->time_info.type = 0xff;
	pcplayback->time_info.start_time = start_time;
	pcplayback->time_info.end_time = end_time;
	
	pcplayback->playback_chn = playback_chn;
	pcplayback->dev_ip = dev_ip;
	pcplayback->stream_id = stream_id;
	pcplayback->status = EM_STREAM_STATUS_DISCONNECT;
	pcplayback->stream_errno = SUCCESS;	

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

	//DBG_PRINT("start\n");

	//DBG_PRINT("manager lock\n");
	plock4param->Lock();

	map_id_iter = map_pbchn_sid.find(playback_chn);
	if (map_id_iter == map_pbchn_sid.end())//������
	{
		ERR_PRINT("MAP_PBChn_SID find failed, playback_chn: %d\n", playback_chn);

		plock4param->Unlock();
		return -EPARAM;
	}

	stream_id = map_id_iter->second;
	map_ppb_iter = map_pcplayback.find(stream_id);
	if (map_ppb_iter == map_pcplayback.end())
	{
		ERR_PRINT("MAP_PBChn_SID find success, but MAP_ID_PCPLAYBACK find failed, playback_chn: %d, stream_id: %d\n",
			playback_chn, stream_id);

		plock4param->Unlock();
		return -EPARAM;
	}
	
	pcplayback = map_ppb_iter->second;
	if (NULL == pcplayback)
	{
		ERR_PRINT("NULL == pcplayback, playback_chn: %d, stream_id: %d\n",
			playback_chn, stream_id);

		plock4param->Unlock();
		return -EPARAM;
	}

	//�Ƴ�
	map_pcplayback.erase(stream_id);
	map_pbchn_sid.erase(playback_chn);
	
	delete pcplayback;
	pcplayback = NULL;

	plock4param->Unlock();

	DBG_PRINT("playback_chn: %u, stream_id: %u\n", playback_chn, stream_id);

	ret = BizStreamReqStop(stream_id);
	if (ret)
	{
		ERR_PRINT("BizStreamReqStop failed, ret: %d, playback_chn: %d, stream_id: %d\n",
			ret, playback_chn, stream_id);
	}

	DBG_PRINT("end\n");
	return ret;
}

int CBizPlaybackManager::PlaybackPause(u32 playback_chn)
{
	if (!b_inited)
	{
		ERR_PRINT("module not inited\n");
		return -FAILURE;
	}
	
	int ret = SUCCESS;
	u32 dev_ip = INADDR_NONE;
	u32 stream_id = INVALID_VALUE;
	CBizPlayback *pcplayback = NULL;
	MAP_PBChn_SID::iterator map_id_iter;
	MAP_ID_PCPLAYBACK::iterator map_ppb_iter;

	DBG_PRINT("start\n");

	//DBG_PRINT("manager lock\n");
	plock4param->Lock();

	map_id_iter = map_pbchn_sid.find(playback_chn);
	if (map_id_iter == map_pbchn_sid.end())//������
	{
		ERR_PRINT("MAP_PBChn_SID find failed, playback_chn: %d\n", playback_chn);

		plock4param->Unlock();
		return -EPARAM;
	}

	stream_id = map_id_iter->second;
	map_ppb_iter = map_pcplayback.find(stream_id);
	if (map_ppb_iter == map_pcplayback.end())
	{
		ERR_PRINT("MAP_PBChn_SID find success, but MAP_ID_PCPLAYBACK find failed, playback_chn: %d, stream_id: %d\n",
			playback_chn, stream_id);

		plock4param->Unlock();
		return -EPARAM;
	}
	
	pcplayback = map_ppb_iter->second;
	if (NULL == pcplayback)
	{
		ERR_PRINT("NULL == pcplayback, playback_chn: %d, stream_id: %d\n",
			playback_chn, stream_id);

		plock4param->Unlock();
		return -EPARAM;
	}

	pcplayback->plock4param->Lock();
	
	dev_ip = pcplayback->dev_ip;
	stream_id = pcplayback->stream_id;

	pcplayback->plock4param->Unlock();

	plock4param->Unlock();

	DBG_PRINT("playback_chn: %u, stream_id: %u\n", playback_chn, stream_id);


	ret = BizDevStreamPause(EM_NVR, dev_ip, stream_id);
	if (ret)
	{
		ERR_PRINT("BizDevStreamPause failed, ret: %d, playback_chn: %d, stream_id: %d\n",
			ret, playback_chn, stream_id);
	}

	DBG_PRINT("end\n");
	return ret;
}

int CBizPlaybackManager::PlaybackResume(u32 playback_chn)
{
	if (!b_inited)
	{
		ERR_PRINT("module not inited\n");
		return -FAILURE;
	}
	
	int ret = SUCCESS;
	u32 dev_ip = INADDR_NONE;
	u32 stream_id = INVALID_VALUE;
	CBizPlayback *pcplayback = NULL;
	MAP_PBChn_SID::iterator map_id_iter;
	MAP_ID_PCPLAYBACK::iterator map_ppb_iter;

	DBG_PRINT("start\n");

	//DBG_PRINT("manager lock\n");
	plock4param->Lock();

	map_id_iter = map_pbchn_sid.find(playback_chn);
	if (map_id_iter == map_pbchn_sid.end())//������
	{
		ERR_PRINT("MAP_PBChn_SID find failed, playback_chn: %d\n", playback_chn);

		plock4param->Unlock();
		return -EPARAM;
	}

	stream_id = map_id_iter->second;
	map_ppb_iter = map_pcplayback.find(stream_id);
	if (map_ppb_iter == map_pcplayback.end())
	{
		ERR_PRINT("MAP_PBChn_SID find success, but MAP_ID_PCPLAYBACK find failed, playback_chn: %d, stream_id: %d\n",
			playback_chn, stream_id);

		plock4param->Unlock();
		return -EPARAM;
	}
	
	pcplayback = map_ppb_iter->second;
	if (NULL == pcplayback)
	{
		ERR_PRINT("NULL == pcplayback, playback_chn: %d, stream_id: %d\n",
			playback_chn, stream_id);

		plock4param->Unlock();
		return -EPARAM;
	}

	pcplayback->plock4param->Lock();
	
	dev_ip = pcplayback->dev_ip;
	stream_id = pcplayback->stream_id;

	pcplayback->plock4param->Unlock();

	plock4param->Unlock();

	DBG_PRINT("playback_chn: %u, stream_id: %u\n", playback_chn, stream_id);


	ret = BizDevStreamResume(EM_NVR, dev_ip, stream_id);
	if (ret)
	{
		ERR_PRINT("BizDevStreamResume failed, ret: %d, playback_chn: %d, stream_id: %d\n",
			ret, playback_chn, stream_id);
	}

	DBG_PRINT("end\n");
	return ret;
}

int CBizPlaybackManager::PlaybackStep(u32 playback_chn)//֡��
{
	if (!b_inited)
	{
		ERR_PRINT("module not inited\n");
		return -FAILURE;
	}
	
	int ret = SUCCESS;
	u32 dev_ip = INADDR_NONE;
	u32 stream_id = INVALID_VALUE;
	CBizPlayback *pcplayback = NULL;
	MAP_PBChn_SID::iterator map_id_iter;
	MAP_ID_PCPLAYBACK::iterator map_ppb_iter;

	DBG_PRINT("start\n");

	//DBG_PRINT("manager lock\n");
	plock4param->Lock();

	map_id_iter = map_pbchn_sid.find(playback_chn);
	if (map_id_iter == map_pbchn_sid.end())//������
	{
		ERR_PRINT("MAP_PBChn_SID find failed, playback_chn: %d\n", playback_chn);

		plock4param->Unlock();
		return -EPARAM;
	}

	stream_id = map_id_iter->second;
	map_ppb_iter = map_pcplayback.find(stream_id);
	if (map_ppb_iter == map_pcplayback.end())
	{
		ERR_PRINT("MAP_PBChn_SID find success, but MAP_ID_PCPLAYBACK find failed, playback_chn: %d, stream_id: %d\n",
			playback_chn, stream_id);

		plock4param->Unlock();
		return -EPARAM;
	}
	
	pcplayback = map_ppb_iter->second;
	if (NULL == pcplayback)
	{
		ERR_PRINT("NULL == pcplayback, playback_chn: %d, stream_id: %d\n",
			playback_chn, stream_id);

		plock4param->Unlock();
		return -EPARAM;
	}

	pcplayback->plock4param->Lock();
	
	dev_ip = pcplayback->dev_ip;
	stream_id = pcplayback->stream_id;

	pcplayback->plock4param->Unlock();

	plock4param->Unlock();

	DBG_PRINT("playback_chn: %u, stream_id: %u\n", playback_chn, stream_id);


	ret = BizDevStreamStep(EM_NVR, dev_ip, stream_id);
	if (ret)
	{
		ERR_PRINT("BizDevStreamResume failed, ret: %d, playback_chn: %d, stream_id: %d\n",
			ret, playback_chn, stream_id);
	}

	DBG_PRINT("end\n");
	return ret;
}

int CBizPlaybackManager::PlaybackRate(u32 playback_chn, s32 rate)//{-8, -4, -2, 1, 2, 4, 8}
{
	if (!b_inited)
	{
		ERR_PRINT("module not inited\n");
		return -FAILURE;
	}
	
	int ret = SUCCESS;
	u32 dev_ip = INADDR_NONE;
	u32 stream_id = INVALID_VALUE;
	CBizPlayback *pcplayback = NULL;
	MAP_PBChn_SID::iterator map_id_iter;
	MAP_ID_PCPLAYBACK::iterator map_ppb_iter;

	DBG_PRINT("start\n");

	//DBG_PRINT("manager lock\n");
	plock4param->Lock();

	map_id_iter = map_pbchn_sid.find(playback_chn);
	if (map_id_iter == map_pbchn_sid.end())//������
	{
		ERR_PRINT("MAP_PBChn_SID find failed, playback_chn: %d\n", playback_chn);

		plock4param->Unlock();
		return -EPARAM;
	}

	stream_id = map_id_iter->second;
	map_ppb_iter = map_pcplayback.find(stream_id);
	if (map_ppb_iter == map_pcplayback.end())
	{
		ERR_PRINT("MAP_PBChn_SID find success, but MAP_ID_PCPLAYBACK find failed, playback_chn: %d, stream_id: %d\n",
			playback_chn, stream_id);

		plock4param->Unlock();
		return -EPARAM;
	}
	
	pcplayback = map_ppb_iter->second;
	if (NULL == pcplayback)
	{
		ERR_PRINT("NULL == pcplayback, playback_chn: %d, stream_id: %d\n",
			playback_chn, stream_id);

		plock4param->Unlock();
		return -EPARAM;
	}

	pcplayback->plock4param->Lock();
	
	dev_ip = pcplayback->dev_ip;
	stream_id = pcplayback->stream_id;

	pcplayback->plock4param->Unlock();

	plock4param->Unlock();

	DBG_PRINT("playback_chn: %u, stream_id: %u\n", playback_chn, stream_id);

	ret = BizDevStreamRate(EM_NVR, dev_ip, stream_id, rate);
	if (ret)
	{
		ERR_PRINT("BizDevStreamRate failed, ret: %d, playback_chn: %d, stream_id: %d\n",
			ret, playback_chn, stream_id);
	}

	DBG_PRINT("end\n");
	return ret;
}

int CBizPlaybackManager::PlaybackSeek(u32 playback_chn, u32 time)
{
	if (!b_inited)
	{
		ERR_PRINT("module not inited\n");
		return -FAILURE;
	}
	
	int ret = SUCCESS;
	u32 dev_ip = INADDR_NONE;
	u32 stream_id = INVALID_VALUE;
	CBizPlayback *pcplayback = NULL;
	MAP_PBChn_SID::iterator map_id_iter;
	MAP_ID_PCPLAYBACK::iterator map_ppb_iter;

	DBG_PRINT("start\n");

	//DBG_PRINT("manager lock\n");
	plock4param->Lock();

	map_id_iter = map_pbchn_sid.find(playback_chn);
	if (map_id_iter == map_pbchn_sid.end())//������
	{
		ERR_PRINT("MAP_PBChn_SID find failed, playback_chn: %d\n", playback_chn);

		plock4param->Unlock();
		return -EPARAM;
	}

	stream_id = map_id_iter->second;
	map_ppb_iter = map_pcplayback.find(stream_id);
	if (map_ppb_iter == map_pcplayback.end())
	{
		ERR_PRINT("MAP_PBChn_SID find success, but MAP_ID_PCPLAYBACK find failed, playback_chn: %d, stream_id: %d\n",
			playback_chn, stream_id);

		plock4param->Unlock();
		return -EPARAM;
	}
	
	pcplayback = map_ppb_iter->second;
	if (NULL == pcplayback)
	{
		ERR_PRINT("NULL == pcplayback, playback_chn: %d, stream_id: %d\n",
			playback_chn, stream_id);

		plock4param->Unlock();
		return -EPARAM;
	}

	pcplayback->plock4param->Lock();
	
	dev_ip = pcplayback->dev_ip;
	stream_id = pcplayback->stream_id;

	pcplayback->plock4param->Unlock();

	plock4param->Unlock();

	DBG_PRINT("playback_chn: %u, stream_id: %u\n", playback_chn, stream_id);

	ret = BizDevStreamSeek(EM_NVR, dev_ip, stream_id, time);
	if (ret)
	{
		ERR_PRINT("BizDevStreamSeek failed, ret: %d, playback_chn: %d, stream_id: %d\n",
			ret, playback_chn, stream_id);
	}

	DBG_PRINT("end\n");
	return ret;
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


//
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


//�طſ���
int BizModulePlaybackPause(u32 playback_chn)
{
	return g_biz_playback_manager.PlaybackPause(playback_chn);
}

int BizModulePlaybackResume(u32 playback_chn)
{
	return g_biz_playback_manager.PlaybackResume(playback_chn);
}

int BizModulePlaybackStep(u32 playback_chn)//֡��
{
	return g_biz_playback_manager.PlaybackStep(playback_chn);
}

int BizModulePlaybackRate(u32 playback_chn, s32 rate)//{-8, -4, -2, 1, 2, 4, 8}
{
	return g_biz_playback_manager.PlaybackRate(playback_chn, rate);
}

int BizModulePlaybackSeek(u32 playback_chn, u32 time)
{
	return g_biz_playback_manager.PlaybackSeek(playback_chn, time);
}






