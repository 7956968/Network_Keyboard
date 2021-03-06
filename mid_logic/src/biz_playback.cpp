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
#include "custommp4.h"
#include "avilib.h"

#include "biz.h"
#include "biz_net.h"
#include "biz_msg_type.h"
#include "biz_types.h"
#include "biz_config.h"
#include "biz_remote_stream.h"
#include "biz_device.h"


#include "biz_playback.h"
#include "biz_system_complex.h"


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
#include <sys/stat.h>
#include <fcntl.h>



#include <map>
#include <set>
//#include <bitset>
#include <algorithm>
#include <utility>

//本模块::playback_id 就是来自biz_remote_stream::stream_id
#ifndef USE_AUDIO_PCMU
#define USE_AUDIO_PCMU
#endif

class CBizPlaybackManager;


/*******************************************************************
CBizPlayback 声明
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
	void FreeSrc();//释放资源
	
	
private:
	VD_BOOL b_inited;
	C_Lock *plock4param;	//mutex
	int playback_type;
	ifly_recfileinfo_t file_info;
	char save_file_name[128];//下载文件名
	SPlayback_Time_Info_t time_info;

	u32 playback_chn;	//回放通道
	u32 dev_ip;//服务器IP
	u32 stream_id;	//来自biz_remote_stream //关键，系统唯一
	EM_STREAM_STATUS_TYPE status;	//流状态
	s32 stream_errno;	//错误码
	s32 rate;		//当前播放速度[-8 -4 -2 1 2 4 8]  <==   1<<[-3, 3]
	u32 cur_pos;	//当前播放时间
	u32 total_size;	//总时间长度
	int fd;//下载
	u32 wr_cnt;		//写入数据量累计
};


/*******************************************************************
CBizPlayback 定义
*******************************************************************/
CBizPlayback::CBizPlayback()
: b_inited(FALSE)
, plock4param(NULL)
, playback_type(EM_PLAYBACK_NONOE)
, playback_chn(INVALID_VALUE)	//回放通道
, dev_ip(INADDR_NONE)			//服务器IP
, stream_id(INVALID_VALUE)	//关键，系统唯一
, status(EM_STREAM_STATUS_INIT) //流状态
, stream_errno(SUCCESS) 	//错误码
, rate(1)					//正常播放
, cur_pos(INVALID_VALUE)	//当前播放时间
, total_size(INVALID_VALUE) //总时间长度
, fd(INVALID_FD)
, wr_cnt(0)
{
	memset(&file_info, 0, sizeof(ifly_recfileinfo_t));
	memset(&time_info, 0, sizeof(SPlayback_Time_Info_t));
	memset(&save_file_name, 0, sizeof(save_file_name));
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

void CBizPlayback::FreeSrc()//释放资源
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
	if (plock4param)
		plock4param->Lock();

	if (playback_chn >= 0x10)//下载
	{
		if (INVALID_FD != fd)
		{
			close(fd);
			fd = INVALID_FD;
		}
	}

	if (plock4param)
		plock4param->Unlock();
	
	FreeSrc();
}
















/*******************************************************************
CBizPlaybackManager 声明
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
	// 写消息to stream manager
	int WriteMsg(SBizMsg_t *pmsg, u32 msg_len);
	int dealFrameFunc(u32 stream_id, FRAMEHDR *pframe_hdr);//to BizDataCB
	int dealStateFunc(SBizMsg_t *pmsg, u32 len);//WriteMsg
	int PlaybackStartByFile(u32 playback_chn, u32 dev_ip, ifly_recfileinfo_t *pfile_info, char *psave_file_name=NULL);
	int PlaybackStartByTime(u32 playback_chn, u32 dev_ip, u8 chn, u32 start_time, u32 end_time);
	//预览是否已经处于进行中
	VD_BOOL PlaybackIsStarted(u32 playback_chn);
	int PlaybackStop(u32 playback_chn);
	int PlaybackCancel(u32 playback_chn);
	//回放控制
	int PlaybackPause(u32 playback_chn);
	int PlaybackResume(u32 playback_chn);
	int PlaybackStep(u32 playback_chn);//帧进
	int PlaybackRate(u32 playback_chn, s32 rate);//{-8, -4, -2, 1, 2, 4, 8}
	int PlaybackSeek(u32 playback_chn, u32 time);
	
private:
	CBizPlaybackManager();
	CBizPlaybackManager(CBizPlaybackManager &)
	{

	}
	void FreeSrc();//释放资源
	void threadMsg(uint param);//消息线程执行函数
	int dealStreamMsgConnectSuccess(u32 stream_id);
	int dealStreamMsgConnectFail(u32 stream_id, s32 stream_errno);
	int dealStreamMsgStop(u32 stream_id, s32 stream_errno);
	int dealStreamMsgProgess(u32 stream_id,u32 cur_pos, u32 total_size);
	int dealStreamMsgFinish(u32 stream_id);
	int dealStreamMsgSvrShutdown(u32 stream_id);
	
private:
	VD_BOOL b_inited;
	C_Lock *plock4param;//mutex
	MAP_PBChn_SID map_pbchn_sid;
	MAP_ID_PCPLAYBACK map_pcplayback;
	u32 cplayback_cnt;
	
	//消息队列数据
	pthread_cond_t mq_cond;		//写flash 的条件变量
	pthread_mutex_t mq_mutex;	//条件变量互斥锁
	u32 mq_msg_count;			//条件，pmsg_queue 中消息数目
	CcircularBuffer *pmq_ccbuf;	//环形缓冲区
	Threadlet mq_msg_threadlet;	//消息线程
	
};

PATTERN_SINGLETON_IMPLEMENT(CBizPlaybackManager);


/*******************************************************************
CBizPlaybackManager 定义
*******************************************************************/
CBizPlaybackManager::CBizPlaybackManager()
: b_inited(FALSE)
, plock4param(NULL)
, cplayback_cnt(0)

//消息队列数据
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
	
	//创建消息队列线程及相关数据
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

	//启动消息队列读取线程
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

// 写消息to Playback manager
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

	//内部
	if (EM_STREAM_STATUS_INIT != pcplayback->status)
	{
		ERR_PRINT("EM_STREAM_STATUS_INIT != pcplayback->status(%d)\n", pcplayback->status);
	}
	
	pcplayback->status = EM_STREAM_STATUS_RUNNING;
	pcplayback->stream_errno = SUCCESS;


	//其他
	dev_ip = pcplayback->dev_ip;
	playback_chn = pcplayback->playback_chn;

	pcplayback->plock4param->Unlock();
	plock4param->Unlock();//非删除操作可以先释放该锁

	if (playback_chn < 0x10)//回放
	{
		ret = BizDevStreamProgress(EM_NVR, dev_ip, stream_id, TRUE);//接收回放进度信息
		if (ret)
		{
			ERR_PRINT("BizDevStreamProgress, playback_chn: %d failed, ret: %d\n",
				playback_chn, ret);
		}
	}
	else	//下载
	{
		DBG_PRINT("download file connect success, playback_chn: %d\n", playback_chn);
	}

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
	pcplayback->status = EM_STREAM_STATUS_STOP;
	
	if (SUCCESS == pcplayback->stream_errno
			&& SUCCESS != stream_errno)
	{
		 pcplayback->stream_errno = stream_errno;
	}
	
	playback_chn = pcplayback->playback_chn;

	pcplayback->plock4param->Unlock();

#if 0
	//删除
	map_pcplayback.erase(stream_id);
	map_pbchn_sid.erase(playback_chn);

	delete pcplayback;
	pcplayback = NULL;
#endif	
	plock4param->Unlock();

#if 0
	//上传msg 2 biz manager
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
	s32 stop_reason = SUCCESS;
	
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

	//内部数据
	if (EM_STREAM_STATUS_STOP == pcplayback->status)
	{
		pcplayback->plock4param->Unlock();
		plock4param->Unlock();

		DBG_PRINT("status == EM_STREAM_STATUS_STOP, return\n");
		return SUCCESS;
	}
	
	pcplayback->status = EM_STREAM_STATUS_STOP;
	
	if (SUCCESS == pcplayback->stream_errno
			&& SUCCESS != stream_errno)
	{
		 pcplayback->stream_errno = stream_errno;
	}

	//其他
	playback_chn = pcplayback->playback_chn;

	pcplayback->plock4param->Unlock();
#if 0
	//删除
	map_pcplayback.erase(stream_id);
	map_pbchn_sid.erase(playback_chn);

	delete pcplayback;
	pcplayback = NULL;
#endif	
	plock4param->Unlock();

	//上传msg 2 biz manager
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
		stop_reason = stream_errno;
	}
	else//无错
	{
		para.type = EM_BIZ_EVENT_PLAYBACK_DONE;
		para.un_part_chn.playback_chn = playback_chn;
	}
#if 0
	ret = BizStreamReqStop(stream_id, stop_reason);
	if (ret)
	{
		ERR_PRINT("BizStreamReqStop failed, ret: %d, playback_chn: %d, stream_id: %d\n",
			ret, playback_chn, stream_id);
	}
#endif
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

	//对应的回放结构如果存在就上传消息
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

	//上传msg 2 biz manager
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

//NVR端在文件回放时只上报进度，不上报结束
int CBizPlaybackManager::dealStreamMsgFinish(u32 stream_id)
{
	int ret = SUCCESS;
	u32 playback_chn = INVALID_VALUE;
	CBizPlayback *pcplayback = NULL;
	MAP_ID_PCPLAYBACK::iterator map_ppb_iter;

	//stream_id  to  playback_chn
	plock4param->Lock();

	//对应的回放结构如果存在就上传消息
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

	//上传msg 2 biz manager
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

#if 0
int CBizPlaybackManager::dealStreamMsgSvrShutdown(u32 stream_id)
{
	int ret = SUCCESS;
	u32 playback_chn = INVALID_VALUE;
	CBizPlayback *pcplayback = NULL;
	MAP_ID_PCPLAYBACK::iterator map_ppb_iter;
	s32 stop_reason = SUCCESS;

	DBG_PRINT("1\n");
	
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
	if (playback_chn < 0x10)//回放
	{
		
	}
	else
	{
		//下载未完成，对端关闭
		if (pcplayback->wr_cnt != pcplayback->file_info.size)
		{
			stop_reason = -ERECV;
		}
	}

	pcplayback->plock4param->Unlock();

	//删除
	map_pcplayback.erase(stream_id);
	map_pbchn_sid.erase(playback_chn);

	delete pcplayback;
	pcplayback = NULL;
	
	plock4param->Unlock();

	//上传msg 2 biz manager
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
	
	if (stop_reason)
	{
		para.type = EM_BIZ_EVENT_PLAYBACK_NETWORK_ERR;
		para.un_part_chn.playback_chn = playback_chn;
		para.un_part_data.stream_errno = stop_reason;
	}
	else//无错
	{
		para.type = EM_BIZ_EVENT_PLAYBACK_DONE;
		para.un_part_chn.playback_chn = playback_chn;
	}

	ret = BizStreamReqStop(stream_id, stop_reason);
	if (ret)
	{
		ERR_PRINT("BizStreamReqStop failed, ret: %d, playback_chn: %d, stream_id: %d\n",
			ret, playback_chn, stream_id);
	}

	DBG_PRINT("BizEventCB, playback_chn: %d, msg_type: %d, stop_reason: %d\n",
			playback_chn, para.type, stop_reason);
	
	ret = BizEventCB(&para);
	if (ret)
	{
		ERR_PRINT("BizEventCB failed, ret: %d, playback_chn: %d, msg_type: %d\n",
			ret, playback_chn, para.type);
	}
				
#endif


	return ret;
}
#endif

void CBizPlaybackManager::threadMsg(uint param)//读消息
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
		
		if (mq_msg_count)	//有消息
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
		else	//无消息
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
				//消息
				case EM_STREAM_MSG_CONNECT_SUCCESS:	//连接成功
				{
					//biz_remote_stream 上传
					u32 stream_id = msg.un_part_chn.stream_id;
					
					ret = dealStreamMsgConnectSuccess(stream_id);
				} break;
				
				case EM_STREAM_MSG_CONNECT_FALIURE:	//连接失败
				{
					//biz_remote_stream 上传
					u32 stream_id = msg.un_part_chn.stream_id;
					s32 stream_errno = msg.un_part_data.stream_errno;
					
					ret = dealStreamMsgConnectFail(stream_id, stream_errno);
				} break;
				
				case EM_STREAM_MSG_STOP:	//biz_dev 层上传的流关闭，可能设备掉线
				{
					u32 stream_id = msg.un_part_chn.stream_id;
					s32 stream_errno = msg.un_part_data.stream_errno;

					ret = dealStreamMsgStop(stream_id, stream_errno);
					
				} break;
				
				case EM_STREAM_MSG_PROGRESS:		//文件回放/下载进度
				{
					ret = dealStreamMsgProgess(msg.un_part_chn.stream_id,
												msg.un_part_data.stream_progress.cur_pos,
												msg.un_part_data.stream_progress.total_size);
				} break;
				
				case EM_STREAM_MSG_FINISH:		//文件下载完成
				{
					ret = dealStreamMsgFinish(msg.un_part_chn.stream_id);
				} break;
			#if 0
				case EM_STREAM_MSG_SVR_SHUTDOWM:		//连接对端关闭
				{
					ret = dealStreamMsgSvrShutdown(msg.un_part_chn.stream_id);
				} break;
			#endif
				

				default:
					ERR_PRINT("msg_type: %d, not support\n", msg_type);
			}
		}
	}

thread_exit:
	
	ERR_PRINT("CBizPlaybackManager::threadMsg exit, inconceivable\n");
}

#define USB_NOERROR 0
#define USB_ERROR_WRITE_ACCESS 1
#define USB_ERROR_WRITE_FULL 2
#define USB_ERROR_WRITE_CANCEL 3

int CBizPlaybackManager::dealFrameFunc(u32 stream_id, FRAMEHDR *pframe_hdr)
{
	int fd = INVALID_FD;
	int ret = SUCCESS;
	ifly_recfileinfo_t file_info;
	
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

	//对应的回放结构如果存在就上传消息
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
	file_info = pcplayback->file_info;
		
	if (playback_chn < 0x10)//回放
	{
		pcplayback->plock4param->Unlock();
		plock4param->Unlock();

		SBizDataPara para;
		memset(&para, 0, sizeof(SBizDataPara));
		para.type = EM_BIZ_DATA_PLAYBACK;
		para.un_part_chn.playback_chn = playback_chn;
		para.un_part_data.pframe_hdr = pframe_hdr;
		
		return BizDataCB(&para);
	}

	//下载
	fd = pcplayback->fd;
	if (INVALID_FD == fd)
	{
		pcplayback->plock4param->Unlock();
		plock4param->Unlock();
		
		ERR_PRINT("playback_chn: %d, fd == INVALID_FD, inconceivable\n", playback_chn);

		SBizMsg_t msg;//发消息给自己
		memset(&msg, 0, sizeof(SBizMsg_t));
		msg.msg_type = EM_STREAM_MSG_STOP;
		msg.un_part_chn.stream_id = stream_id;
		msg.un_part_data.stream_errno = -EUDISK_WR;

		return WriteMsg(&msg, sizeof(SBizMsg_t));
	}

	//写udisk
	u32 file_size = pcplayback->file_info.size;

	//error	pcplayback->wr_cnt > file_size
	if (pcplayback->wr_cnt + pframe_hdr->m_dwDataSize > file_size)
	{
		pcplayback->plock4param->Unlock();
		plock4param->Unlock();
		
		ERR_PRINT("playback_chn: %d, wr_cnt(%u) + rcv_size(%u) > file_size(%u), inconceivable\n",
			playback_chn, pcplayback->wr_cnt, pframe_hdr->m_dwDataSize, file_size);

		SBizMsg_t msg;//发消息给自己
		memset(&msg, 0, sizeof(SBizMsg_t));
		msg.msg_type = EM_STREAM_MSG_STOP;
		msg.un_part_chn.stream_id = stream_id;
		msg.un_part_data.stream_errno = -ERECV;

		return WriteMsg(&msg, sizeof(SBizMsg_t));
	}
	
	ret = write(fd, pframe_hdr->m_pData, pframe_hdr->m_dwDataSize);
	if (ret != (s32)pframe_hdr->m_dwDataSize)
	{
		pcplayback->plock4param->Unlock();
		plock4param->Unlock();
		
		ERR_PRINT("playback_chn: %d, udisk write failed, ret: %d, data len: %d, err(%d, %s)\n", 
			playback_chn, ret, pframe_hdr->m_dwDataSize, errno, strerror(errno));

		SBizMsg_t msg;//发消息给自己
		memset(&msg, 0, sizeof(SBizMsg_t));
		msg.msg_type = EM_STREAM_MSG_STOP;
		msg.un_part_chn.stream_id = stream_id;
		msg.un_part_data.stream_errno = -EUDISK_WR;

		return WriteMsg(&msg, sizeof(SBizMsg_t));
	}
	
	pcplayback->wr_cnt += ret;
	
	//DBG_PRINT("wr_cnt: %d, cur_pos: %d\n", pcplayback->wr_cnt, cur_pos);
	//进度构成: 下载占80%，转换占20%
	u32 cur_pos = 0;
	if(pcplayback->wr_cnt < file_size) //下载中
	{		
		cur_pos = (double)80.0 * pcplayback->wr_cnt / file_size; //百分比

		//上传进度
		if (cur_pos != pcplayback->cur_pos)
		{
			pcplayback->cur_pos = cur_pos;

			pcplayback->plock4param->Unlock();
			plock4param->Unlock();
			
			SBizEventPara para;//上传进度
			memset(&para, 0, sizeof(SBizEventPara));		
			para.type = EM_BIZ_EVENT_PLAYBACK_RUN;
			para.un_part_chn.playback_chn = playback_chn;
			para.un_part_data.stream_progress.cur_pos = cur_pos;
			para.un_part_data.stream_progress.total_size = 100;

			//DBG_PRINT("BizEventCB, playback_chn: %d, msg_type: %d, cur_pos: %d\n",
					//playback_chn, para.type, cur_pos);

			BizEventCB(&para);	
		}
		else
		{
			pcplayback->plock4param->Unlock();
			plock4param->Unlock();
		}

		return SUCCESS;
	}

	//下载完成转换成AVI格式	
	fsync(fd);
	close(fd);
	pcplayback->fd = INVALID_FD;

	int inside_err = SUCCESS;
	char avi_file_path[128];
	
	if (0 == strlen(pcplayback->save_file_name))
	{
		ERR_PRINT("save_file_name invalid\n");
		
		inside_err = -EPARAM;
	}
	else
	{
		sprintf(avi_file_path, "./udisk/%s", pcplayback->save_file_name);	
	}
	
	pcplayback->plock4param->Unlock();
	plock4param->Unlock();

	//convert ifv 2 avi
	custommp4_t* pFile1 = NULL;
	avi_t* pAviHandle = NULL;
	char fly_file_path[128];
	s8 *buf = NULL;
	u32 update_pos = 79;//进度构成: 下载占80%，转换占20%
	u32 start_time = 0;
	//struct tm tm_time;
	u32 file_totaltime = 0;
	u32 size = 0;//文件内视频帧数量
	u32 total_frames = 0;
	u32 wr_frames = 0;//已经写入的视频帧
	u8 Iframe_cnt = 1;
	u8 key = 0;
	int readlen = 0;
	int ret_avi = 0;
	SBizEventPara para;
	SBizMsg_t msg;
	char *pos = NULL;	
	
	#ifdef USE_AUDIO_PCMU
	u8 media_type = 0;
	u64 pts = 0;
	#endif

	if (-EPARAM == inside_err)
	{
		goto END;
	}
	
	buf = new s8[MAX_FRAME_SIZE];
	if (NULL == buf)
	{
		ERR_PRINT("buf new failed\n");
		
		inside_err = -ESPACE;
		goto END;
	}

	pos = strstr(file_info.filename, "fly");
	if (NULL == pos)
	{
		ERR_PRINT("fly_file_name %s, invalid\n", file_info.filename);
		
		inside_err = -EPARAM;
		goto END;
	}
	sprintf(fly_file_path, "./udisk/%s", pos);
	
	DBG_PRINT("fly_file_path: %s, avi_file_path %s, file offset: %u\n", fly_file_path, avi_file_path, file_info.offset);
	
	if (0 == access(avi_file_path, F_OK))//存在，先删除
	{
		unlink(avi_file_path);
	}

/////////////////////////////////
	pFile1 = custommp4_open(fly_file_path, O_R, 0);//file_info.offset); 下载的文件已经由NVR端处理过偏移
	if(pFile1 == NULL)
	{
		ERR_PRINT("avi custommp4_open recfile error\n");
		//errcode = USB_NOERROR;
		inside_err = -EUDISK_WR;
		goto END;
	}
	
	pAviHandle = AVI_open_output_file(avi_file_path);
	if (!pAviHandle)
	{
		ERR_PRINT("avi file open failed \n");
		//errcode = USB_ERROR_WRITE_ACCESS;
		inside_err = -EUDISK_WR;
		goto END;
	}
	
	//USE_AUDIO_PCMU begin
	size = custommp4_video_length(pFile1);//文件内视频帧数量
	file_totaltime = file_info.end_time - file_info.start_time;
	if (0 == file_totaltime) file_totaltime = 1;

	DBG_PRINT("total_frames: %d, total_time: %d, fps: %f\n", size, file_totaltime, (double)size/(float)file_totaltime);
#if 0	
	AVI_set_video(pAviHandle,
			 custommp4_video_width(pFile1),
			 custommp4_video_height(pFile1),
			 (double)size/(float)file_totaltime/*custommp4_video_frame_rate(pFile1)*/,
			 "H264" );
#else
	AVI_set_video2(pAviHandle,
			 custommp4_video_width(pFile1),
			 custommp4_video_height(pFile1),
			 (double)size/(float)file_totaltime/*custommp4_video_frame_rate(pFile1)*/,
			 file_totaltime,
			 "H264" );
#endif	
	AVI_set_audio(pAviHandle,
			custommp4_audio_channels(pFile1),
			custommp4_audio_sample_rate(pFile1),
			custommp4_audio_bits(pFile1),
			WAVE_FORMAT_PCM, 0 );
	//USE_AUDIO_PCMU end
	
	total_frames = size;
	wr_frames = 0;//已经写入的视频帧
	key = 0;
	
	#ifdef USE_AUDIO_PCMU
	pts = 0;
	#endif
	
	while(size > 0)
	{
	#if 0
		// check if need break backup
		if(modsysCmplx_QueryBreakBackup())
		{
			bBreakBackup = TRUE;
			break;
		}
		
		if(tCopy.cancel)
		{
			haveerror = TRUE;
			errcode = USB_ERROR_WRITE_CANCEL;
			tCopy.cancel = 0;
			break;
		}
	#endif	
		#ifdef USE_AUDIO_PCMU
		Iframe_cnt = 1;
		//readlen = custommp4_read_one_media_frame(pFile1, (u8 *)buf, MAX_FRAME_SIZE, &start_time, &key, &pts,&media_type);
		//组合sps pps I帧版本，解决AVI播放卡顿
		readlen = custommp4_read_one_media_frame2(pFile1, (u8 *)buf, MAX_FRAME_SIZE, &start_time, &key, &pts,&media_type, &Iframe_cnt);
		#else
		readlen = custommp4_read_one_video_frame(pFile1, buf, MAX_FRAME_SIZE, &start_time, &duration, &key);
		#endif
		if (readlen <= 0)
		{
			ERR_PRINT( "avi Read error 10,errcode=%d,errstr=%s\n", errno, strerror(errno));
			//errcode = USB_NOERROR;
			inside_err = -EUDISK_WR;			
			break;
		}
		//DBG_PRINT("media_type: %d, key: %d, readlen: %d\n", media_type, key, readlen);
		
		#ifdef USE_AUDIO_PCMU
		if(0 == media_type){
		#endif
			ret_avi = AVI_write_frame(pAviHandle, buf, readlen, key);
			if (ret_avi)
			{
				ERR_PRINT( "avi Write error 1,errcode=%d,errstr=%s\n", errno, strerror(errno) );
				//errcode = USB_ERROR_WRITE_FULL;
				inside_err = -EUDISK_WR;
				if(errno == 5)//Input/output error
				{
					//errcode = USB_ERROR_WRITE_ACCESS;
					inside_err = -EUDISK_WR;
				}
				
				break;
			}

			wr_frames += Iframe_cnt;
			size -= Iframe_cnt;
		#ifdef USE_AUDIO_PCMU
		}
		else
		{
			ret_avi = AVI_write_audio(pAviHandle, buf, readlen);
			if (ret_avi)
			{
				ERR_PRINT( "avi Write error 1,errcode=%d,errstr=%s\n", errno, strerror(errno) );
				//errcode = USB_ERROR_WRITE_FULL;
				inside_err = -EUDISK_WR;
				if(errno == 5)//Input/output error
				{
					//errcode = USB_ERROR_WRITE_ACCESS;
					inside_err = -EUDISK_WR;
				}
				
				break;
			}
		}
		#endif

		//进度构成: 下载占80%，转换占20%
		cur_pos = 80 + (double)20.0 * wr_frames / total_frames;

		//上传进度
		if (cur_pos != update_pos)
		{
			update_pos = cur_pos;
			
			//上传进度
			memset(&para, 0, sizeof(SBizEventPara));		
			para.type = EM_BIZ_EVENT_PLAYBACK_RUN;
			para.un_part_chn.playback_chn = playback_chn;
			para.un_part_data.stream_progress.cur_pos = cur_pos;
			para.un_part_data.stream_progress.total_size = 100;

			//DBG_PRINT("BizEventCB, playback_chn: %d, msg_type: %d, convert avi cur_pos: %d\n",
			//		playback_chn, para.type, cur_pos);

			BizEventCB(&para);	
		}
	}
	
END:
	if (buf)
	{
		delete[] buf;
		buf = NULL;
	}
	
	if (pFile1)
	{
		custommp4_close(pFile1);
		pFile1 = NULL;
	}
	
	if (pAviHandle)
	{
		AVI_close(pAviHandle);
		pAviHandle = NULL;
	}

	//发消息给自己
	memset(&msg, 0, sizeof(SBizMsg_t));
	msg.msg_type = EM_STREAM_MSG_STOP;
	msg.un_part_chn.stream_id = stream_id;
	
	if(inside_err == SUCCESS) //成功
	{
		DBG_PRINT("file convert success\n");		
	}
	else //失败
	{
		ERR_PRINT("file convert failed, err: %d\n", inside_err);
		msg.un_part_data.stream_errno = inside_err;

		//删除AVI 文件
		DBG_PRINT("rm avi file: %s\n", avi_file_path);
		unlink(avi_file_path);
	}

	//删除IFV 文件
	DBG_PRINT("rm ifv file: %s\n", fly_file_path);
	unlink(fly_file_path);
	
	BizUnmountUdisk();

	return WriteMsg(&msg, sizeof(SBizMsg_t));
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



int CBizPlaybackManager::PlaybackStartByFile(u32 playback_chn, u32 dev_ip, ifly_recfileinfo_t *pfile_info, char *psave_file_name)
{
	int ret = SUCCESS;
	u32 stream_id = INVALID_VALUE;
	int connect_type = 0;
	struct in_addr in;
	in.s_addr = dev_ip;
	int fd = INVALID_FD;//下载

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

	if (playback_chn < 0x10) //回放
	{
		connect_type = 0;
	}
	else	//下载
	{
		connect_type = 1;

		if (NULL == psave_file_name)
		{
			ERR_PRINT("NULL == psave_file_name\n");
			
			return -EPARAM;
		}

		ret = BizMountUdisk();
		if (ret)
		{
			return ret;
		}

		//rec/c2/fly00592.ifv 去除路径rec/c2/
		char fly_file_path[128] = {0};
		char *pos = strstr(pfile_info->filename, "fly");
		if (NULL == pos)
		{
			ERR_PRINT("fly_file_name %s, invalid\n", pfile_info->filename);
			
			return -EPARAM;
		}
		sprintf(fly_file_path, "./udisk/%s", pos);
		DBG_PRINT("fly_file_path %s\n", fly_file_path);
		
		if (0 == access(fly_file_path, F_OK))//存在，先删除
		{
			unlink(fly_file_path);
		}
		
		fd = open(fly_file_path, O_WRONLY|O_CREAT, 0666);
		if (fd < 0)
		{
			ERR_PRINT("open file %s failed\n", fly_file_path);
			return -FAILURE;
		}
	}

	CBizPlayback *pcplayback = NULL;

	pcplayback = new CBizPlayback;
	if (NULL == pcplayback)
	{
		ERR_PRINT("dev_ip: %s, playback_chn: %d, new CBizPlayback failed\n", inet_ntoa(in), playback_chn);
		
		ret = -ESPACE;
		goto fail;
	}

	ret = pcplayback->Init();
	if (ret)
	{
		ERR_PRINT("dev_ip: %s, playback_chn: %d, CBizPlayback init failed, ret: %d\n", inet_ntoa(in), playback_chn, ret);
		
		ret = -ESPACE;
		goto fail;
	}
	
	//成功返回并不表示连接成功，只是写入了消息列表，之后在消息线程连接
	//连接成功后会有消息上传，那时接收进度信息
	ret = BizStreamReqPlaybackByFile (
			EM_NVR,
			connect_type,/* 0 回放 1 下载*/
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
	pcplayback->status = EM_STREAM_STATUS_INIT;
	pcplayback->stream_errno = SUCCESS;
	

	if (connect_type == 1) //下载
	{
		pcplayback->fd = fd;
		strcpy(pcplayback->save_file_name, psave_file_name);
	}

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

	if (INVALID_FD != fd)
	{
		close(fd);
		fd = INVALID_FD;
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

	if (playback_chn < 0x10) //回放
	{
		connect_type = 0;
	}
	else	//下载
	{
		connect_type = 1;
	}

	//成功返回并不表示连接成功，只是写入了消息列表，之后在消息线程连接
	//连接成功后会有消息上传，那时接收进度信息
	ret = BizStreamReqPlaybackByTime (
			EM_NVR,
			connect_type,/* 0 回放 1 下载*/
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
	pcplayback->status = EM_STREAM_STATUS_INIT;
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


//是否已经处于进行中
VD_BOOL CBizPlaybackManager::PlaybackIsStarted(u32 playback_chn)
{	
	VD_BOOL b = FALSE;
	
	if (!b_inited)
	{
		ERR_PRINT("module not inited\n");
		return FALSE;
	}
	
	plock4param->Lock();

	if (map_pbchn_sid.end() != map_pbchn_sid.find(playback_chn))//存在
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
	ifly_recfileinfo_t file_info;

	//DBG_PRINT("1 playback_chn: %d\n", playback_chn);

	//DBG_PRINT("2 playback_chn: %d\n", playback_chn);
	plock4param->Lock();
	//DBG_PRINT("3 playback_chn: %d\n", playback_chn);
	map_id_iter = map_pbchn_sid.find(playback_chn);
	if (map_id_iter == map_pbchn_sid.end())//不存在
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

	file_info = pcplayback->file_info;

	pcplayback->plock4param->Unlock();
	
	//移除
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

	if (playback_chn >= 0x10) //下载
	{
		//rec/c2/fly00592.ifv 去除路径rec/c2/
		char fly_file_path[128] = {0};
		char *pos = strstr(file_info.filename, "fly");
		if (NULL == pos)
		{
			ERR_PRINT("fly_file_name %s, invalid\n", file_info.filename);
		}
		else
		{
			sprintf(fly_file_path, "./udisk/%s", pos);
			DBG_PRINT("fly_file_path %s\n", fly_file_path);
			
			if (0 == access(fly_file_path, F_OK))//存在，删除
			{
				unlink(fly_file_path);
			}
		}

		BizUnmountUdisk();
	}
	//DBG_PRINT("end\n");
	return ret;
}
//用于下载
int CBizPlaybackManager::PlaybackCancel(u32 playback_chn)
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
	ifly_recfileinfo_t file_info;
	char save_file_name[128];

	memset(save_file_name, 0, sizeof(save_file_name));
	
	//DBG_PRINT("1 playback_chn: %d\n", playback_chn);

	//DBG_PRINT("2 playback_chn: %d\n", playback_chn);
	plock4param->Lock();
	//DBG_PRINT("3 playback_chn: %d\n", playback_chn);
	map_id_iter = map_pbchn_sid.find(playback_chn);
	if (map_id_iter == map_pbchn_sid.end())//不存在
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

	if (pcplayback->playback_chn < 0x10) //非下载
	{
		pcplayback->plock4param->Unlock();
		plock4param->Unlock();

		ERR_PRINT("playback_chn < 0x10, mode != download\n");
		return -EPARAM;
	}

	file_info = pcplayback->file_info;
	strcpy(save_file_name, pcplayback->save_file_name);

	pcplayback->plock4param->Unlock();
	
	//移除
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

	if (playback_chn >= 0x10) //下载
	{
		//rec/c2/fly00592.ifv 去除路径rec/c2/
		char fly_file_path[128] = {0};
		char *pos = strstr(file_info.filename, "fly");
		if (NULL == pos)
		{
			ERR_PRINT("fly_file_name %s, invalid\n", file_info.filename);
		}
		else
		{
			sprintf(fly_file_path, "./udisk/%s", pos);
			DBG_PRINT("fly_file_path %s\n", fly_file_path);
			
			if (0 == access(fly_file_path, F_OK))//存在，删除
			{
				//删除IFV 文件
				unlink(fly_file_path);
			}
		}
		
		//删除AVI 文件
		if (strlen(save_file_name))
		{
			char file_name[128];
			sprintf(file_name, "./udisk/%s", save_file_name);
			
			if (0 == access(file_name, F_OK))//存在，删除
			{
				unlink(file_name);
			}
		}
		
		BizUnmountUdisk();
	}
	//DBG_PRINT("end\n");
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
	if (map_id_iter == map_pbchn_sid.end())//不存在
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
	if (map_id_iter == map_pbchn_sid.end())//不存在
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

int CBizPlaybackManager::PlaybackStep(u32 playback_chn)//帧进
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
	if (map_id_iter == map_pbchn_sid.end())//不存在
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
	if (map_id_iter == map_pbchn_sid.end())//不存在
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
	if (map_id_iter == map_pbchn_sid.end())//不存在
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
			外部接口实现
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
int BizModulePlaybackStartByFile(u32 playback_chn, u32 dev_ip, ifly_recfileinfo_t *pfile_info, char *psave_file_name)
{
	return g_biz_playback_manager.PlaybackStartByFile(playback_chn, dev_ip, pfile_info, psave_file_name);
}

int BizModulePlaybackStartByTime(u32 playback_chn, u32 dev_ip, u8 chn, u32 start_time, u32 end_time)
{
	return g_biz_playback_manager.PlaybackStartByTime(playback_chn, dev_ip, chn, start_time, end_time);
}

//是否已经处于进行中
VD_BOOL BizModulePlaybackIsStarted(u32 playback_chn)
{
	return g_biz_playback_manager.PlaybackIsStarted(playback_chn);
}

int BizModulePlaybackStop(u32 playback_chn)
{
	return g_biz_playback_manager.PlaybackStop(playback_chn);
}

int BizModulePlaybackCancel(u32 playback_chn)
{
	return g_biz_playback_manager.PlaybackCancel(playback_chn);
}


//回放控制
int BizModulePlaybackPause(u32 playback_chn)
{
	return g_biz_playback_manager.PlaybackPause(playback_chn);
}

int BizModulePlaybackResume(u32 playback_chn)
{
	return g_biz_playback_manager.PlaybackResume(playback_chn);
}

int BizModulePlaybackStep(u32 playback_chn)//帧进
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






