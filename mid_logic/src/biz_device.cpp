#include "api.h"
#include "singleton.h"
#include "object.h"
#include "atomic.h"
#include "c_lock.h"
#include "crwlock.h"
#include "ccircular_buffer.h"
#include "public.h"
#include "flash.h"
#include "cthread.h"
#include "ctimer.h"
#include "net.h"
#include "biz.h"
#include "biz_net.h"
#include "biz_msg_type.h"
#include "biz_types.h"
#include "biz_config.h"
#include "biz_remote_stream.h"
#include "biz_device.h"


#include "bond.h"

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

#define SendBuf (4096)
#define RcvBuf (4096)
#define DIALOGUE_TIMEOUT	(5) //һ������ػ���ʱ
#define RECONNECT_TIMEOUT	(5) //��
#define KEEP_ALIVE_INTERVAL	(15) //��
#define NETDIALOGUE_ERR_MAX	(2) //����ͨ�Ŵ����ۼ�ֵ��>=��ֵʱ��Ϊ�豸�Ͽ�


#ifdef __cplusplus
extern "C" {
#endif

//�����ص�
void SearchDevice_CB(ifly_DeviceInfo_t dev, void* pContext);

#ifdef __cplusplus
}
#endif

/********************************************************************
							Device
********************************************************************/
#define MaxMediaLinks (5)
#define MaxIdleNum (60)
	
typedef struct DevStream_t
{
	u32 link_id;//biz_device �ڲ�ʹ��
	u32 stream_id;//�ؼ���ϵͳΨһ
	EM_STREAM_STATUS_TYPE status;
	s32 stream_errno;//������
	ifly_TCP_Stream_Req req;

	DevStream_t()
	: link_id(INVALID_VALUE)
	, stream_id(INVALID_VALUE)
	, status(EM_STREAM_STATUS_DISCONNECT)
	, stream_errno(SUCCESS)
	{
		
	}

	~DevStream_t()
	{
		
	}
} SDevStream_t;


//u32 stream_id u32 linkid
typedef std::map<u32, u32> MAP_SID_LID;
//u32 linkid
typedef std::map<u32, SDevStream_t*> MAP_LID_PSTREAM;


class CBizDevice : public CObject {
	friend class CBizDeviceManager;
public:
	CBizDevice();
	~CBizDevice();
	VD_BOOL Init(void);
	void CleanSock();

	int GetDeviceInfo(ifly_DeviceInfo_t *pDeviceInfo);
	//keepalive����ʹ��
	int GetDevSysTime(ifly_sysTime_t *psys_time);
	//���ӡ���¼������
	int DevConnect();
	//�Ͽ���������
	int DevDisconnect();

	//stream******************************************
	//biz_remote_stream �������ã�����ֵ��֪�ɹ����
	//�����ϴ���Ϣ��biz_stream_manager
	int ReqStreamStart(u32 stream_id, ifly_TCP_Stream_Req *preq, s32 *psock_stream);
	//�ϲ���ùرգ��������ϴ���Ϣ
	int ReqStreamStopByStreamID(u32 stream_id, s32 stop_reason = SUCCESS);//GLB_ERROR_NUM �ر�ԭ��Ĭ�������ر�
	//ģ���ڲ����õĹرգ�
	// 1���豸���߻�����ͨ�ų������
	// 2���ϲ�ɾ���豸����
	//��Ҫ�ϴ���Ϣ��biz_stream_manager
	int StreamStopByLinkID(u32 link_id, s32 stop_reason = SUCCESS);//GLB_ERROR_NUM �ر�ԭ��Ĭ�������ر�
	int StreamProgress(u32 stream_id, VD_BOOL b);//���ջطŽ�����Ϣ
	//�طſ���
	int StreamPause(u32 stream_id);
	int StreamResume(u32 stream_id);
	int StreamStep(u32 stream_id);//֡��
	int StreamRate(u32 stream_id, s32 rate);//{-8, -4, -2, 1, 2, 4, 8}
	int StreamSeek(u32 stream_id, u32 time);
	//void _CleanStream(int stream_idx);

	//ģ���ڲ����õĹرգ�
	// 1���豸���߻�����ͨ�ų������
	// 2���ϲ�ɾ���豸����
	int ShutdownStreamAll(s32 stop_reason = SUCCESS);//GLB_ERROR_NUM
	
	//������������������
	int CheckAndReconnectStream();

	
	
	//��ȡ����ͨ����IPC��Ϣ
	int GetChnIPCInfo(ifly_ipc_info_t * pipc_info, u32 size);
	//ֻ֧��NVR���õ�NVRͨ����(osd info)
	int GetChnName(u8 chn, char *pbuf, u32 size);
	
	//���ý�����ͨ����Ӧ��NVR ͨ��
	int SetChnIpc(u8 dec_chn, u32 nvr_ip, u8 nvr_chn);
	//ɾ��ͨ��IPC
	int DelChnIpc(u8 dec_chn);	
	//NVR ¼������
	int RecFilesSearch(ifly_recsearch_param_t *psearch_para, ifly_search_file_result_t *psearch_result);
	//��ȡ�豸��Ѳ����
	int GetPatrolPara(ifly_patrol_para_t *para, u32 *pbuf_size);

	
	
private:
    CBizDevice(CBizDevice &)
	{
		
	}

	void FreeSrc();//�ͷ���Դ
	int _DevLogin(ifly_loginpara_t *plogin);
	int _DevLogout(ifly_loginpara_t *plogin);
	int _DevSetAlarmUpload(u8 upload_enable);
	int _StreamStop(u32 link_id, s32 stop_reason = SUCCESS);//GLB_ERROR_NUM �ر�ԭ��Ĭ�������ر�
	//�ײ����ݽ���
	int DevNetCom(u16 event, const void *content, int length, void* ackbuf, int ackbuflen);
	//���ڴ�����
	int DevNetComAfter(int net_ret);
	//int getStreamFromLinkID(u32 link_id);//ͨ��link_id�õ���Ӧtream_rcv[MaxMediaLinks] �±�
	
private:
	C_Lock *plock4param;//mutex
	SBiz_DeviceInfo_t dev_info;
	ifly_loginpara_t login;
	s32 dev_idx; //dev pool index
	int sock_cmd;		//����sock����¼�����ձ���
	VD_BOOL b_alive;	//�Ƿ�����
	int err_cnt;		//��ʱδ�á�����������ۼ�2�Σ����������豸
						//_Add2SetFromMap ���cnt_err ��close dev sock
	//stream					
	C_Lock *plock4stream;//mutex
#if 1
	MAP_SID_LID map_sid_lid;
	MAP_LID_PSTREAM map_lid_pstream;

#else
	VD_BOOL bthread_stream_running;
	VD_BOOL bthread_stream_exit;//�ⲿ�����߳��˳�
	int stream_cnt;//�ͻ�����������������stearm_rcv ������Ч��Ա��
	int idle_cnt;//���м������߳̿���1���Ӻ��˳�
	CSemaphore sem_exit;//�ȴ�threadStreamRcv�˳��ź���
	SDev_StearmRcv_t stream_rcv[MaxMediaLinks]; //�������ṹMaxMediaLinks
	void threadStreamRcv(uint param);
	Threadlet m_threadlet_stream_rcv;
#endif
};


/********************************************************************
							Device Manager
********************************************************************/

#define g_biz_device_manager (*CBizDeviceManager::instance())

//�豸IP��ַ���豸��ָ��
//MAP_IP_IDX ��IP��ַ��Ҫntohl  ������������
//<u32 IP_le, s32 pool_index> �豸���е�����
typedef std::map<u32, s32> MAP_IP_IDX;

//<s32 socket fd, u32 pool_index>
typedef std::map<s32, s32> MAP_FD_IDX;


class CBizDeviceManager : public CObject
{
	friend class CBizDevice;
	
public:
	PATTERN_SINGLETON_DECLARE(CBizDeviceManager);
	~CBizDeviceManager();
	int GetDevIPList(EM_DEV_TYPE dev_type, std::list<u32> &dev_ip_list);//�����ֽ���
    int GetDevIdx(EM_DEV_TYPE dev_type, u32 dev_ip);
	int GetDevInfo(EM_DEV_TYPE dev_type, u32 dev_ip, SGuiDev *pdev);
	//��ȡ����ͨ����IPC��Ϣ
	int GetDevChnIPCInfo(EM_DEV_TYPE dev_type, u32 dev_ip, ifly_ipc_info_t * pipc_info, u32 size);
	//ֻ֧��NVR���õ�NVRͨ����(osd info)
	int GetDevChnName(EM_DEV_TYPE dev_type, u32 dev_ip, u8 chn, char *pbuf, u32 size);
	//���ý�����ͨ����Ӧ��NVR ͨ��
	int SetDevChnIpc(EM_DEV_TYPE dev_type, u32 dec_ip , u8 dec_chn, u32 nvr_ip, u8 nvr_chn);
	//ɾ��ͨ��IPC
	int DelDevChnIpc(EM_DEV_TYPE dev_type, u32 dec_ip , u8 dec_chn);	
	//NVR ¼������
	int DevRecFilesSearch(u32 nvr_ip, ifly_recsearch_param_t *psearch_para, ifly_search_file_result_t *psearch_result);




	//��ȡ�豸��Ѳ����
	int DevGetPatrolPara(EM_DEV_TYPE dev_type, u32 dec_ip,ifly_patrol_para_t *para, u32 *pbuf_size);





	
	int StartNotifyDevInfo();//ʹ��֪ͨ���豸�㽫��Ϣ֪ͨ���ϲ�
	int AddDev(EM_DEV_TYPE dev_type, u32 dev_ip);
	int DelDev(EM_DEV_TYPE dev_type, u32 dev_ip);	
	//�ɹ�����stearm_rcv[MaxMediaLinks] �±�stream_idx
	int DevReqStreamStart(EM_DEV_TYPE dev_type, u32 dev_ip, u32 stream_id, ifly_TCP_Stream_Req *preq, s32 *psock_stream);
	int DevReqStreamStopByStreamID(EM_DEV_TYPE dev_type, u32 dev_ip, u32 stream_id, s32 stop_reason=SUCCESS);
	int DevStreamProgress(EM_DEV_TYPE dev_type, u32 dev_ip, u32 stream_id, VD_BOOL b);//���ջطŽ�����Ϣ
	int DevStreamPause(EM_DEV_TYPE dev_type, u32 dev_ip, u32 stream_id);
	int DevStreamResume(EM_DEV_TYPE dev_type, u32 dev_ip, u32 stream_id);
	int DevStreamStep(EM_DEV_TYPE dev_type, u32 dev_ip, u32 stream_id);//֡��
	int DevStreamRate(EM_DEV_TYPE dev_type, u32 dev_ip, u32 stream_id, s32 rate);//{-8, -4, -2, 1, 2, 4, 8}
	int DevStreamSeek(EM_DEV_TYPE dev_type, u32 dev_ip, u32 stream_id, u32 time);
	//�ײ����ݽ���
	int NetCom(int sock, u16 event, const void *content, int length, void* ackbuf, int ackbuflen, int *realacklen);
	int FirstInit();
	int SecondInit();
	VD_BOOL isInited();
	//add and del  map_fd_idx ��Ҫ����������Ϣ�ı�
	int AddMapRcv(s32 sock_fd, s32 dev_idx);
	int DelMapRcv(s32 sock_fd);
	//��ѯdev: sock_cmd�Ƿ���map_fd_idx
	//�������˵����thread_Rcv ���Ѿ��������մ���
	VD_BOOL isInMapRcv(s32 sock_fd);
	// д��Ϣto dev manager
	int WriteMsg(SBizMsg_t *pmsg, u32 msg_len);
	
private:
	CBizDeviceManager();
	CBizDeviceManager(CBizDeviceManager &)
	{

	}
	void FreeSrc();//�ͷ���Դ
	int _AddDev2Map(EM_DEV_TYPE dev_type, u32 dev_ip);//�ⲿ����
	void threadMsg(uint param);//����Ϣ
	void threadKeepAlive(uint param);//����������
	void timerFuncReconnect(uint param);//����������
	void threadRcv(uint param);// 1.���շ�������Ӧ---2. ���շ���������
	void dealDevNotify(s32 dev_idx, u16 event, s8 *pbyMsgBuf, int msgLen); //�����豸֪ͨ��Ϣ
	//���Ƿ�����, ������ӵ��豸
	void _SplitDevFromMap(EM_DEV_TYPE dev_type, 
								std::list<s32> &list_devs_online, 
								std::list<s32> &list_devs_offline);
	int _UnlinkAllDevs();
	
	//device
	
	
private:
	VD_BOOL b_inited;
	//VD_BOOL b_notify_devinfo;

	//�豸����
	//�ڲ�
	C_Lock *plock_dev_pool;//�豸��
	C_Lock **pplock_dev;
	CBizDevice **ppcdev;
	std::vector<bool> pool_bits;//ָʾ�豸��������idxλ����û�б�ռ��
	u32 pool_cur_idx;
	s32 pool_size;// max_link_devs *2 Բ��Ϊ2�Ĵ���	
	u32 pool_dev_count;

	//���
	C_Lock *plock4param;//rwlock	
	u32 max_link_devs; //���������豸��Ŀ(nvr + dec)
	std::set<u32> set_dev_ip; //�����豸IP��ntohl���ֵ����ѯ�豸�Ƿ����
	//MAP_IP_IDX map_nvr; //�����NVR
	//MAP_IP_IDX map_patrol_dec; //�������Ѳ��DEC
	//MAP_IP_IDX map_switch_dec; //������л���DEC
	MAP_IP_IDX map_dev[EM_DEV_TYPE_MAX - EM_NVR];
	std::vector<std::string> str_dev_list;
	
	CTimer *m_reconnect_timer; //�����豸
	CSemaphore reconnect_sem; //threadKeepAlive ��timerFuncReconnect ͬ��
	Threadlet m_threadlet_keep_alive; // 1 �����ɾ���豸-- 2 ����������豸
	Threadlet m_threadlet_rcv; // 1 ���շ�������Ӧ-- 2 ���շ���������-- 3 ����ɾ���豸
	
	//��ɾ���豸
	C_Lock *plock_list_del;//Mutex
	std::list<s32> list_dev_del;//��ɾ�����豸�б�	dev_idx

	//���н����豸��Ϣ
	C_Lock *plock_map_fd_idx;
	MAP_FD_IDX map_fd_idx;//m_threadlet_rcv ������յ��豸socket ��index

	//C_Lock *plock4sync_num;
	u16 sync_num;//����ػ����к�
	C_Lock *plock4sock;//Mutex ͬʱֻ�����һ��������ͨ��
	CSemaphore sync_sem; //socket ���ͷ�ͬ���ȴ����շ�������Ӧ
	char *send_buf;
	char *sync_buf; //�洢��������Ӧ����
	u32 sync_ack_len; // ��������Ӧ���ݳ���

	//��Ϣ��������
	pthread_cond_t mq_cond;		//дflash ����������
	pthread_mutex_t mq_mutex;	//��������������
	u32 mq_msg_count;			//������pmsg_queue ����Ϣ��Ŀ
	CcircularBuffer *pmq_ccbuf;	//���λ�����
	Threadlet mq_msg_threadlet;	//��Ϣ�߳�
};


/********************************************************************
							Device
********************************************************************/

CBizDevice::CBizDevice()
: plock4param(NULL)
, dev_idx(INVALID_VALUE)
, sock_cmd(INVALID_SOCKET)
, b_alive(FALSE)
, err_cnt(0)
//stream
, plock4stream(NULL)
#if 0
, bthread_stream_running(FALSE)
, bthread_stream_exit(FALSE)
, stream_cnt(0)
, idle_cnt(0)
, sem_exit(0)
#endif
{
	
}

CBizDevice::~CBizDevice()
{
	FreeSrc();
}

int CBizDevice::Init()
{
	//VD_BOOL b_ret = FALSE;
	
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

	plock4stream = new CMutex(MUTEX_RECURSIVE);
	if (NULL == plock4stream)
	{
		ERR_PRINT("new plock4stream failed\n");
		goto fail;
	}
	
	if (plock4stream->Create())//FALSE
	{
		ERR_PRINT("create plock4stream failed\n");
		goto fail;
	}
	return SUCCESS;

fail:
	FreeSrc();
	return -FAILURE;
}

void CBizDevice::FreeSrc()//�ͷ���Դ
{	
	if (plock4param)
	{
		delete plock4param;
		plock4param = NULL;
	}

	if (plock4stream)
	{
		delete plock4stream;
		plock4stream = NULL;
	}
}

//�ⲿ����
void CBizDevice::CleanSock()
{
	if (INVALID_SOCKET != sock_cmd)
	{
		//�Ƴ�����map_fd_idx
		g_biz_device_manager.DelMapRcv(sock_cmd);
		
		close(sock_cmd);
		sock_cmd = INVALID_SOCKET;
	}
	
	b_alive = FALSE;
	err_cnt = 0;
}

int CBizDevice::_DevLogin(ifly_loginpara_t *plogin)
{
	int realacklen = 0;
	int ret = SUCCESS;
	u8 buf[128];
    
	ret = g_biz_device_manager.NetCom(sock_cmd, CTRL_CMD_LOGIN, plogin, sizeof(ifly_loginpara_t), buf, sizeof(buf), &realacklen);
	if (SUCCESS != ret)
	{
		ERR_PRINT("NetCom CTRL_CMD_LOGIN invalid\n");
		return -FAILURE;
	}

	return SUCCESS;
}

int CBizDevice::_DevLogout(ifly_loginpara_t *plogin)
{
	//int realacklen = 0;
	int ret = SUCCESS;
	u8 buf[128];

#if 0   
	ret = g_biz_device_manager.NetCom(sock_cmd, CTRL_CMD_LOGOFF, plogin, sizeof(ifly_loginpara_t), buf, sizeof(buf), &realacklen);
	if (SUCCESS != ret)
	{
		ERR_PRINT("NetCom CTRL_CMD_LOGOFF invalid\n");
		return -FAILURE;
	}
#else
	ret = DevNetCom(CTRL_CMD_LOGOFF, plogin, sizeof(ifly_loginpara_t), buf, sizeof(buf));
	if (SUCCESS != ret)
	{
		ERR_PRINT("DevNetCom CTRL_CMD_LOGOFF failed\n");
		return ret;
	}
#endif
	return SUCCESS;
}

int CBizDevice::_DevSetAlarmUpload(u8 upload_enable)
{
	//int realacklen = 0;
	int ret = SUCCESS;
	u8 buf[128];

	if (0 != upload_enable && 1 != upload_enable)
    {
        DBG_PRINT("upload_enable: %d, invalid\n", upload_enable);
        return -EPARAM;
    }

#if 0	
	ret = g_biz_device_manager.NetCom(sock_cmd, CTRL_CMD_ALARMUPLOADCENTER, &upload_enable,sizeof(upload_enable), buf, sizeof(buf), &realacklen);
	if (SUCCESS != ret)
	{
		ERR_PRINT("NetCom CTRL_CMD_ALARMUPLOADCENTER invalid\n");
		return -FAILURE;
	}
#else
	ret = DevNetCom(CTRL_CMD_ALARMUPLOADCENTER, &upload_enable, sizeof(upload_enable), buf, sizeof(buf));
	if (SUCCESS != ret)
	{
		ERR_PRINT("DevNetCom CTRL_CMD_LOGOFF failed\n");
		return ret;
	}
#endif

	return SUCCESS;
}

int CBizDevice::GetDeviceInfo(ifly_DeviceInfo_t *pDeviceInfo)
{
	int realacklen = 0;
	int ret = SUCCESS;

	if (NULL == pDeviceInfo)
	{
		DBG_PRINT("param invalid\n");
        return -EPARAM;
	}
	
    if (INVALID_SOCKET == sock_cmd)
    {
        DBG_PRINT("dev offline\n");
        return -EDEV_OFFLINE;
    }
	
	ret = g_biz_device_manager.NetCom(sock_cmd, CTRL_CMD_GETDEVICEINFO, NULL, 0, pDeviceInfo, sizeof(ifly_DeviceInfo_t), &realacklen);
	if (ret)
	{
		ERR_PRINT("NetCom invalid\n");
		return -FAILURE;
	}

	//���ݸı��ֽ���
	pDeviceInfo->devicePort = ntohs(pDeviceInfo->devicePort);//ֻ��һ��

#ifdef NDEBUG
	struct in_addr in;
	in.s_addr = pDeviceInfo->deviceIP;
	
	printf("get device info deviceIP:%s, devicePort:%d, device_name:%s, device_mode:%s, MaxChn=%d, nNVROrDecoder:%d\n", \
		inet_ntoa(in),
		pDeviceInfo->devicePort,
		pDeviceInfo->device_name,
		pDeviceInfo->device_mode,
		pDeviceInfo->maxChnNum,
		pDeviceInfo->nNVROrDecoder);
#endif

	return ret;
}

//�ײ����ݽ���
int CBizDevice::DevNetCom(u16 event, const void *content, int length, void* ackbuf, int ackbuflen)
{
	int realacklen = 0;
	int ret = SUCCESS;
	struct in_addr in;
	in.s_addr = dev_info.deviceIP;

	do //���ڳ�ʱ�ط�
	{
		//DBG_PRINT("dev(%s) cmd(%d) send\n", inet_ntoa(in), event);	
		
		if (!b_alive || INVALID_SOCKET == sock_cmd)
		{
			DBG_PRINT("dev(%s) offline\n", inet_ntoa(in));
			return -EDEV_OFFLINE;
		}

		if (err_cnt > NETDIALOGUE_ERR_MAX)
		{
			DBG_PRINT("dev(%s) err_cnt > NETDIALOGUE_ERR_MAX\n", inet_ntoa(in));
			return -EDEV_OFFLINE;
		}

		if (isSockIOErr(sock_cmd))
		{
			ERR_PRINT("dev(%s) isSockIOErr true\n", inet_ntoa(in));

			ret = -ERECV;
			goto fail;
		}		

		//��ѯdev: sock_cmd�Ƿ���map_fd_idx
		//�������˵����thread_Rcv ���Ѿ��������մ���
		if (!g_biz_device_manager.isInMapRcv(sock_cmd))
		{
			ERR_PRINT("dev(%s) sock_cmd not in MapRcv\n", inet_ntoa(in));

			ret = -ERECV;
			goto fail;
		}
		
		ret = g_biz_device_manager.NetCom(sock_cmd, event, content, length, ackbuf, ackbuflen, &realacklen);

fail:
		//�ɹ����Ҫ���к��ڴ���
		ret = DevNetComAfter(ret);
		if (SUCCESS == ret)
		{
			break;
		}
		else if (-ETIMEOUT == ret)//һ�γ�ʱ���ܷ�������һ�����������
		{
			ERR_PRINT("dev(%s) cmd(%d) timeout, try again!!!\n", inet_ntoa(in), event);			
		}
	} while (err_cnt < NETDIALOGUE_ERR_MAX);
	
	return ret;
}

int CBizDevice::DevNetComAfter(int net_ret)//���ڴ�����
{
	struct in_addr in;
	in.s_addr = dev_info.deviceIP;
	
	if (SUCCESS == net_ret)
	{
		err_cnt = 0;//����ͨ�ųɹ�������
		return SUCCESS;
	}

	//DBG_PRINT("dev(%s) net_ret: %d\n", inet_ntoa(in), net_ret);

	if (-ETIMEOUT == net_ret)//һ�γ�ʱ���ܷ�������һ�����������
	{
		++err_cnt;
	}

	if (net_ret == -ESEND|| net_ret == -ERECV)//-ESEND/-ERECV �϶�����
	{
		err_cnt = NETDIALOGUE_ERR_MAX+1;
	}

	if (err_cnt >= NETDIALOGUE_ERR_MAX)
	{
		net_ret = -EDEV_OFFLINE;//�豸���ߣ��������������keepalive �߳�_SplitDevFromMap �д���
		
		DBG_PRINT("dev(%s) disconnect\n", inet_ntoa(in));
#if 0
		//�ر�cmd socket
		CleanSock();
		
		//�Ͽ�������
		ShutdownStreamAll(EM_STREAM_RCV_ERR);
		
		//ע��logout
		DevDisconnect();
#endif
	}

	return net_ret;
}
#if 0
int CBizDevice::getStreamFromLinkID(u32 link_id)//ͨ��link_id�õ���Ӧtream_rcv[MaxMediaLinks] �±�
{
	int stream_idx = INVALID_VALUE;
	int i = 0;
	
	plock4stream->Lock();

	for (i=0; i<MaxMediaLinks; ++i)
	{
		if (stream_rcv[i].b_connect
			&& stream_rcv[i].linkid == link_id)
		{
			stream_idx = i;
			break;
		}
	}
	
	plock4stream->Unlock();

	return stream_idx;
}
#endif

//keepalive����ʹ��
int CBizDevice::GetDevSysTime(ifly_sysTime_t *psys_time)
{
	int ret = SUCCESS;

	if (NULL == psys_time)
	{
		DBG_PRINT("param invalid\n");
        return -EPARAM;
	}

	ret = DevNetCom(CTRL_CMD_GETSYSTIME, NULL, 0, psys_time, sizeof(ifly_sysTime_t));
	if (ret)
	{
		ERR_PRINT("DevNetCom CTRL_CMD_GETSYSTIME failed, ret: %d\n", ret);
		return ret;
	}

	//���ݸı��ֽ���
	psys_time->systemtime = ntohl(psys_time->systemtime);

#ifdef NDEBUG
	struct in_addr in;
	in.s_addr = dev_info.deviceIP;
	time_t time = (time_t)psys_time->systemtime + 3600*8;
	struct tm tm_time;
	
	gmtime_r(&time, &tm_time);

	DBG_PRINT("Dev(%s) system time: %d/%d/%d-%02d/%02d/%02d\n", inet_ntoa(in),
		tm_time.tm_year+1900, tm_time.tm_mon+1, tm_time.tm_mday,
		tm_time.tm_hour, tm_time.tm_min, tm_time.tm_sec);
	
#endif

	return SUCCESS;
}


//��ȡ����ͨ����IPC��Ϣ
int CBizDevice::GetChnIPCInfo(ifly_ipc_info_t * pipc_info, u32 size)
{
	int ret = SUCCESS;
	char buf[RcvBuf]={0};
	ifly_search_desc_t req_desc, ret_desc;
	ifly_ipc_info_t *pipc = NULL;
	u32 cp_nums = 0;
	u32 ret_nums = 0; 
	
	
 	if (NULL == pipc_info)
	{
		DBG_PRINT("param invalid\n");
        return -EPARAM;
	}

	if (size < dev_info.maxChnNum * sizeof(ifly_ipc_info_t))
	{
		DBG_PRINT("size(%d) invalid, dev_info.maxChnNum(%d) * sizeof(ifly_ipc_info_t)(%d) = %d\n",
			size, dev_info.maxChnNum, sizeof(ifly_ipc_info_t), dev_info.maxChnNum * sizeof(ifly_ipc_info_t));
        return -EPARAM;
	}	

	if (INVALID_SOCKET == sock_cmd)
    {
        DBG_PRINT("dev offline\n");
        return -EDEV_OFFLINE;
    }
	
	memset(&req_desc, 0, sizeof(ifly_search_desc_t));
	memset(&ret_desc, 0, sizeof(ifly_search_desc_t));

	do{	
		req_desc.startID = ret_desc.endID+1;//�ӵ�һ����ʼ
		req_desc.startID = htons(req_desc.startID);
		
		ret = DevNetCom(CTRL_CMD_GETADDIPCLIST, &req_desc, sizeof(ifly_search_desc_t), buf, sizeof(buf));
		if (ret)
		{
			ERR_PRINT("DevNetCom CTRL_CMD_GETADDIPCLIST failed, ret: %d\n", ret);
			
			return ret;
		}

		memcpy(&ret_desc, buf, sizeof(ifly_search_desc_t));
		ret_desc.startID = ntohs(ret_desc.startID);
		ret_desc.endID = ntohs(ret_desc.endID);
		ret_desc.sum = ntohs(ret_desc.sum);

		DBG_PRINT("ret sum: %d, startID: %d, endID: %d\n", 
					ret_desc.sum, ret_desc.startID, ret_desc.endID);

		if (ret_desc.sum != dev_info.maxChnNum)
		{
			ERR_PRINT("ret_desc.sum(%d) != dev_info.maxChnNum(%d)\n", 
				ret_desc.sum, dev_info.maxChnNum);
			
			return -EPARAM;
		}
		
		ret_nums = ret_desc.endID - ret_desc.startID + 1;
		pipc = (ifly_ipc_info_t *)(buf + sizeof(ifly_search_desc_t));			
		memcpy(pipc_info+cp_nums, pipc, sizeof(ifly_ipc_info_t)*ret_nums);
		cp_nums += ret_nums;
		
	} while(ret_desc.endID < ret_desc.sum);
	
	return SUCCESS;
}

//ֻ֧��NVR���õ�NVRͨ����(osd info)
int CBizDevice::GetChnName(u8 chn, char *pbuf, u32 size)
{
	int ret = SUCCESS;

	if (dev_info.devicetype != EM_NVR)
	{
		ERR_PRINT("dev_info.devicetype(%d) != EM_NVR\n", dev_info.devicetype);
		return -EPARAM;
	}

	if (chn >= dev_info.maxChnNum)
	{
		ERR_PRINT("chn(%d) >= dev_info.maxChnNum(%d)\n", chn, dev_info.maxChnNum);
		return -EPARAM;
	}
	
	if (NULL == pbuf)
	{
		ERR_PRINT("Pbuf == NULL\n");
		return -EPARAM;
	}

	if (size == 0)
	{
		ERR_PRINT("size == 0\n");
		return -EPARAM;
	}

	ifly_ImgParam_t para_info;
	memset(&para_info, 0, sizeof(ifly_ImgParam_t));
	
	ret = DevNetCom(CTRL_CMD_GETIMGPARAM, &chn, sizeof(chn), &para_info, sizeof(ifly_ImgParam_t));
	if (ret)
	{
		ERR_PRINT("DevNetCom CTRL_CMD_GETIMGPARAM failed, ret: %d\n", ret);
		
		return ret;
	}

	if (strlen(para_info.channelname)+1 > size)
	{
		ERR_PRINT("size(%d) short for chn name strlen(%d)\n", ret, strlen(para_info.channelname));
		
		return -ESPACE;
	}

	strcpy(pbuf, para_info.channelname);

	return SUCCESS;
}

//���ý�����ͨ����Ӧ��NVR ͨ��
int CBizDevice::SetChnIpc(u8 dec_chn, u32 nvr_ip, u8 nvr_chn)
{
	int ret = SUCCESS;
	ifly_ipc_info_t ipc_info;

	struct in_addr in;
	in.s_addr = nvr_ip;
	
	//DBG_PRINT("dec_chn: %d, nvr ip: %s, nvr_chn: %d\n",
	//	dec_chn, inet_ntoa(in), nvr_chn);

	//ֻ֧�ֽ�����
	if (dev_info.devicetype != EM_SWITCH_DEC
		&& dev_info.devicetype != EM_PATROL_DEC)
	{
		ERR_PRINT("dev_info.devicetype(%d) not support\n", dev_info.devicetype);
		return -EPARAM;
	}

	if (dec_chn >= dev_info.maxChnNum)
	{
		ERR_PRINT("chn(%d) >= dev_info.maxChnNum(%d)\n", dec_chn, dev_info.maxChnNum);
		return -EPARAM;
	}	
	
	memset(&ipc_info, 0, sizeof(ifly_ipc_info_t));
	ipc_info.enable = 1;
	ipc_info.channel_no = dec_chn;
	ipc_info.protocol_type = htonl(0x100);//protocol nvr
	ipc_info.dwIp = htonl(nvr_ip);
	ipc_info.wPort = htons(8670);
	strcpy(ipc_info.user, "admin");
	strcpy(ipc_info.pwd, "");
	ipc_info.max_nvr_chn = 16;
	ipc_info.req_nvr_chn = nvr_chn;

	ret = DevNetCom(CTRL_CMD_SETIPC, &ipc_info, sizeof(ifly_ipc_info_t), NULL, 0);
	if (ret)
	{
		ERR_PRINT("DevNetCom CTRL_CMD_SETIPC failed, ret: %d\n", ret);
		
		return ret;
	}

	return SUCCESS;
}

//ɾ��ͨ��IPC
int CBizDevice::DelChnIpc(u8 dec_chn)
{
	int ret = SUCCESS;
	ifly_ipc_info_t ipc_info;

	//ֻ֧�ֽ�����
	if (dev_info.devicetype != EM_SWITCH_DEC
		&& dev_info.devicetype != EM_PATROL_DEC)
	{
		ERR_PRINT("dev_info.devicetype(%d) not support\n", dev_info.devicetype);
		return -EPARAM;
	}

	if (dec_chn >= dev_info.maxChnNum)
	{
		ERR_PRINT("chn(%d) >= dev_info.maxChnNum(%d)\n", dec_chn, dev_info.maxChnNum);
		return -EPARAM;
	}	
	
	memset(&ipc_info, 0, sizeof(ifly_ipc_info_t));
	ipc_info.enable = 0;
	ipc_info.channel_no = dec_chn;
	#if 0
	ipc_info.protocol_type = htonl(0x100);//protocol nvr
	ipc_info.dwIp = htonl(nvr_ip);
	ipc_info.wPort = htons(8670);
	strcpy(ipc_info.user, "admin");
	strcpy(ipc_info.pwd, "");
	ipc_info.max_nvr_chn = 16;
	ipc_info.req_nvr_chn = nvr_chn;
	#endif

	ret = DevNetCom(CTRL_CMD_DELETEIPC, &ipc_info, sizeof(ifly_ipc_info_t), NULL, 0);
	if (ret)
	{
		ERR_PRINT("DevNetCom CTRL_CMD_DELETEIPC failed, ret: %d\n", ret);
		
		return ret;
	}

	return SUCCESS;
}

//NVR ¼������
int CBizDevice::RecFilesSearch(ifly_recsearch_param_t *psearch_para, ifly_search_file_result_t *psearch_result)
{
	int ret = SUCCESS;
	ifly_recsearch_param_t search_para;//Ϊ�˱���psearch_para ���ݲ���htons htonl Ӱ��
	char buf[4096]={0};

	//ֻ֧��NVR
	if (dev_info.devicetype != EM_NVR)
	{
		ERR_PRINT("dev_info.devicetype(%d) != EM_NVR\n", dev_info.devicetype);
		return -EPARAM;
	}

	if (NULL == psearch_para || NULL == psearch_result)
	{
		ERR_PRINT("para NULL\n");
		return -EPARAM;
	}
	
    
#if 0
enum NETDVR_REC_INDEX_MASK
{
    NETDVR_REC_INDEX_TIMER = 0x1,
    NETDVR_REC_INDEX_MD = 0x2,
    NETDVR_REC_INDEX_ALARM = 0x4,
    NETDVR_REC_INDEX_HAND = 0x8,
    NETDVR_REC_INDEX_ALL = 0x10,
};
#endif
	memcpy(&search_para, psearch_para, sizeof(ifly_recsearch_param_t));

	search_para.channel_mask = htons(search_para.channel_mask);
    search_para.type_mask = htons(search_para.type_mask);
    search_para.start_time = htonl(search_para.start_time);
    search_para.end_time = htonl(search_para.end_time);
    search_para.startID = htons(search_para.startID);  //must >= 1
    search_para.max_return = htons(search_para.max_return); //must <= 24
    
	ret = DevNetCom(CTRL_CMD_RECFILESEARCH, 
				&search_para, sizeof(ifly_recsearch_param_t), 
				buf, sizeof(buf));
	if (ret)
	{
		ERR_PRINT("DevNetCom CTRL_CMD_RECFILESEARCH failed, ret: %d\n", ret);
		
		return ret;
	}

	ifly_search_desc_t desc;
	memcpy(&desc, buf, sizeof(ifly_search_desc_t));	
	desc.sum = ntohs(desc.sum);
	desc.startID = ntohs(desc.startID);
	desc.endID = ntohs(desc.endID);

	DBG_PRINT("sum: %d, startID: %d, endID: %d\n", 
		desc.sum, desc.startID, desc.endID);
	
	memcpy(&psearch_result->result_desc, &desc, sizeof(ifly_search_desc_t));

	if (desc.startID == 0)//û���������ļ����޷����ļ���Ϣ
	{
		DBG_PRINT("desc.startID == 0, not found files\n");
		return SUCCESS;
	}

	u16 file_nums = MIN(psearch_para->max_return, desc.endID-desc.startID+1);
	
	DBG_PRINT("result file nums: %d\n", file_nums);
		
	memcpy(psearch_result->pfile_info,
		buf + sizeof(ifly_search_desc_t),
		file_nums * sizeof(ifly_recfileinfo_t));

	u32 i = 0;
	for (i=0; i<file_nums; ++i)
	{
		psearch_result->pfile_info[i].start_time	= ntohl(psearch_result->pfile_info[i].start_time);
		psearch_result->pfile_info[i].end_time		= ntohl(psearch_result->pfile_info[i].end_time);
		psearch_result->pfile_info[i].size			= ntohl(psearch_result->pfile_info[i].size);
		psearch_result->pfile_info[i].offset		= ntohl(psearch_result->pfile_info[i].offset);
	}

	return SUCCESS;
}

//��ȡ�豸��Ѳ����
int CBizDevice::GetPatrolPara(ifly_patrol_para_t *para, u32 *pbuf_size)
{
	int ret = SUCCESS;
	char buf[1024]={0};

	//ֻ֧�ֽ�����
	if (dev_info.devicetype != EM_SWITCH_DEC
		&& dev_info.devicetype != EM_PATROL_DEC)
	{
		ERR_PRINT("dev_info.devicetype(%d) not support\n", dev_info.devicetype);
		return -EPARAM;
	}

	if (NULL == para || NULL == pbuf_size)
	{
		ERR_PRINT("para NULL\n");
		return -EPARAM;
	}

	if (sizeof(ifly_patrol_para_t) >= *pbuf_size)
	{
		ERR_PRINT("sizeof(ifly_patrol_para_t)(%d) >= buf_size(%d)\n",
			sizeof(ifly_patrol_para_t), *pbuf_size);
		return -EPARAM;
	}
	
	memset(para, 0, sizeof(ifly_patrol_para_t));
	ret = DevNetCom(CTRL_CMD_GET_PATROL_PARA, NULL, 0, buf, sizeof(buf));
	if (ret)
	{
		ERR_PRINT("DevNetCom CTRL_CMD_GET_PATROL_PARA failed, ret: %d\n", ret);
		
		return ret;
	}

	memcpy(para, buf, sizeof(ifly_patrol_para_t)); 
	
	u32 num = sizeof(ifly_patrol_para_t)+para->nInterval_num \
			+ para->nPatrolMode_num - 1;//nInterval_num+nPatrolMode_num Ϊvalue[]�����С

	//printf("para size: %d, num: %d\n", *psize, num);
	if (num > *pbuf_size)
	{
		*pbuf_size = num;
		ret =  -ESPACE;
	}
	
	memcpy(para, buf, num);

	return SUCCESS;
}


















//���ӡ���¼������
int CBizDevice::DevConnect()
{
	int ret = 0;
	struct in_addr in;
	ifly_DeviceInfo_t DeviceInfo;
	SGuiDev gdev;

	in.s_addr = dev_info.deviceIP;

	//�������ͣ�0x1���������0x2���������䣻0x3���㲥������0x4  ��Ѳͬ��(ֻ���Ѿ�ʹ����Ѳ���豸��Ч)
	ret = SendTcpConnHead(sock_cmd, 0x1);
	if (ret < 0)
	{
		DBG_PRINT("svr IP: %s, SendTcpConnHead failed\n", inet_ntoa(in));
		
		goto fail;
	}
	DBG_PRINT("svr IP: %s, SendTcpConnHead success\n", inet_ntoa(in));

	//�õ��豸��Ϣ
	memset(&DeviceInfo, 0, sizeof(ifly_DeviceInfo_t));
	ret = GetDeviceInfo(&DeviceInfo);
	if (ret < 0)
	{
		DBG_PRINT("svr IP: %s, GetDeviceInfo failed\n", inet_ntoa(in));

		goto fail;
	}
	DBG_PRINT("svr IP: %s, GetDeviceInfo success\n", inet_ntoa(in));

	//���Ͳ�һ��
#if 0	
	if (dev_info.devicetype != DeviceInfo.nNVROrDecoder+(u8)EM_DEV_TYPE_NONE)
	{
		DBG_PRINT("device type conflict, devicetype: %d, nNVROrDecoder: %d\n", 
			dev_info.devicetype, DeviceInfo.nNVROrDecoder);

		goto fail;
	}
#endif

	dev_info.maxChnNum = DeviceInfo.maxChnNum;

	//��¼�豸	
	memset(&login, 0, sizeof(ifly_loginpara_t));
	
	login.ipAddr = GetLocalIp();
	if (login.ipAddr == 0)
	{
		DBG_PRINT("GetLocalIp failed\n");
		goto fail;
	}
	
	strcpy(login.username, "admin");
	//strcpy(login.loginpass, pwd);
	strcpy(login.macAddr, "00:00:00:00:00:00");
	ret = _DevLogin(&login);
	if (ret < 0)
	{
		DBG_PRINT("svr IP: %s, DeviceLogin failed\n", inet_ntoa(in));

		goto fail;
	}
	DBG_PRINT("svr IP: %s, DeviceLogin success\n", inet_ntoa(in));

	b_alive = TRUE;
	err_cnt = 0;//����

#if 0
	//���ý��ձ�����Ϣ
	ret = _DevSetAlarmUpload(1);
	if (ret < 0)
	{
		DBG_PRINT("svr IP: %s, Device SetAlarmUpload failed\n", inet_ntoa(in));

		goto fail2;
	}
	DBG_PRINT("svr IP: %s, Device SetAlarmUpload success\n", inet_ntoa(in));
#endif	

	//֪ͨ�豸����
	
	memset(&gdev, 0, sizeof(gdev));
	gdev.b_alive = b_alive;
	gdev.devicetype = dev_info.devicetype;
	gdev.deviceIP= dev_info.deviceIP;
	gdev.maxChnNum= dev_info.maxChnNum;
	gdev.dev_idx= dev_idx;
	
	notifyDevInfo(&gdev);
	
	return SUCCESS;

fail2:
	_DevLogout(&login);

fail:
	return -FAILURE;
}

//�Ͽ���������
int CBizDevice::DevDisconnect()
{
	int ret = 0;
	struct in_addr in;
	SGuiDev gdev;

	in.s_addr = dev_info.deviceIP;
	
	ret = _DevLogout(&login);
	if (ret < 0)
	{
		DBG_PRINT("svr IP: %s, DeviceLogout failed\n", inet_ntoa(in));
	}
	//DBG_PRINT("svr IP: %s, DeviceLogout success\n", inet_ntoa(in));

#if 0
	//���ý��ձ�����Ϣ
	ret = _DevSetAlarmUpload(0);
	if (ret < 0)
	{
		DBG_PRINT("svr IP: %s, Device SetAlarmUpload failed\n", inet_ntoa(in));
	}
	DBG_PRINT("svr IP: %s, Device SetAlarmUpload success\n", inet_ntoa(in));
#endif

	//֪ͨ�豸����
	memset(&gdev, 0, sizeof(gdev));
	gdev.b_alive = b_alive;
	gdev.devicetype = dev_info.devicetype;
	gdev.deviceIP= dev_info.deviceIP;
	gdev.maxChnNum= dev_info.maxChnNum;
	gdev.dev_idx= dev_idx;
	notifyDevInfo(&gdev);
	
	return SUCCESS;
}

//stream
//�ɹ�����stearm_rcv[MaxMediaLinks] �±�stream_idx
// ifly_TCP_Stream_Req  �ڴ˱任�������ֽ���
//biz_remote_stream �������ã�����ֵ��֪�ɹ����
//�����ϴ���Ϣ��biz_stream_manager
int CBizDevice::ReqStreamStart(u32 stream_id, ifly_TCP_Stream_Req *preq, s32 *psock_stream)
{
	int ret = -FAILURE;
	int fd_tmp = INVALID_SOCKET;
	u32 link_id = INVALID_VALUE;
	u8 req_cmd = INVALID_VALUE;
	u16 net_cmd = INVALID_VALUE;
	SDevStream_t *pstream = NULL;
	ifly_TCP_Stream_Req send_req;//��Ҫ�任�������ֽ���
	ifly_TCP_Stream_Ack stream_ack;
	struct sockaddr_in server;
	struct in_addr in;
	in.s_addr = dev_info.deviceIP;
	struct timeval timeout = {3,0};
	struct timeval timeout0 = {0,0};
	socklen_t optlen = 0; 

	if (NULL == preq || NULL == psock_stream)
	{
		ERR_PRINT("NULL == preq || NULL == psock_stream, svr IP: %s\n", inet_ntoa(in));
		return -EPARAM;
	}

	//0��Ԥ�� 1���ļ��ط� 2��ʱ��ط� 3���ļ����� 4������ 
	//5 VOIP 6 �ļ���֡���� 7 ʱ�䰴֡���� 8 ͸��ͨ��
	//9 Զ�̸�ʽ��Ӳ�� 10 ������ץͼ 11 ��·��ʱ��ط� 12 ��ʱ�������ļ�

	req_cmd = preq->command;
	//memset(&send_req, 0, sizeof(ifly_TCP_Stream_Req));
	send_req = *preq;

	//send_req �ṹת��Ϊ�����ֽ���
	if (req_cmd == 0)// 0��Ԥ��
	{
		
	}
	else if (req_cmd == 1)// 1���ļ��ط�
	{
		send_req.FilePlayBack_t.offset = htonl(send_req.FilePlayBack_t.offset);
	}
	else if (req_cmd == 2)// 2��ʱ��ط�
	{
		send_req.TimePlayBack_t.type = htons(send_req.TimePlayBack_t.type);
		send_req.TimePlayBack_t.start_time = htonl(send_req.TimePlayBack_t.start_time);
		send_req.TimePlayBack_t.end_time = htonl(send_req.TimePlayBack_t.end_time);
	}
	else if (req_cmd == 3)// 3���ļ�����
	{
		send_req.FileDownLoad_t.offset = htonl(send_req.FileDownLoad_t.offset);
		send_req.FileDownLoad_t.size = htonl(send_req.FileDownLoad_t.size);
	}
	else
	{
		ERR_PRINT("stream req cmd: %d not support, svr IP: %s\n", req_cmd, inet_ntoa(in));

		return -EPARAM;
	}
	
	if (!b_alive || INVALID_SOCKET == sock_cmd)
	{
		ERR_PRINT("svr IP: %s, offline\n", inet_ntoa(in));
		return -EDEV_OFFLINE;
	}

	fd_tmp = socket(AF_INET, SOCK_STREAM, 0);
	if (fd_tmp < 0)
	{
		ERR_PRINT("svr IP: %s, stream req cmd: %d, create socket failed, %s\n", inet_ntoa(in), preq->command, strerror(errno));

		return -FAILURE;
	}
	
	optlen = sizeof(struct timeval);
	ret = getsockopt(fd_tmp, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout0, &optlen);//Ϊ֮��ָ�
	if (ret < 0)
	{
		ERR_PRINT("svr IP: %s, stream req cmd: %d, getsockopt failed\n", inet_ntoa(in), preq->command);
		
		goto fail;
	}
	
	ret = setsockopt(fd_tmp, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(struct timeval));//���ó�ʱʱ��
	if (ret < 0)
	{
		ERR_PRINT("svr IP: %s, stream req cmd: %d, setsockopt failed\n", inet_ntoa(in), preq->command);
		
		goto fail;
	}

	server.sin_family = AF_INET;
	server.sin_addr.s_addr = dev_info.deviceIP;
	server.sin_port = htons(dev_info.devicePort);

	ret = connect(fd_tmp, (struct sockaddr*)&server, sizeof(server));
	if (ret < 0)
	{
		ERR_PRINT("svr IP: %s, stream req cmd: %d, socket connect failed, %s\n", inet_ntoa(in), preq->command, strerror(errno));

		goto fail;
	}

	//�������ͣ�0x1���������0x2���������䣻0x3���㲥������0x4  ��Ѳͬ��(ֻ���Ѿ�ʹ����Ѳ���豸��Ч)
	ret = SendTcpConnHead(fd_tmp, 0x2);
	if (ret < 0)
	{
		ERR_PRINT("svr IP: %s, stream req cmd: %d, SendTcpConnHead 0x2 failed\n", inet_ntoa(in), preq->command);
		
		goto fail;
	}

	ret = loopsend(fd_tmp, (char *)preq, sizeof(ifly_TCP_Stream_Req));
	if (ret < 0)
	{
		ERR_PRINT("svr IP: %s, stream req cmd: %d, Send ifly_TCP_Stream_Req failed\n", inet_ntoa(in), preq->command);
		
		goto fail;
	}

	ret = looprecv(fd_tmp, (char *)&stream_ack, sizeof(ifly_TCP_Stream_Ack));
	if (ret < 0)
	{
		ERR_PRINT ("svr IP: %s, stream req cmd: %d, recv ifly_TCP_Stream_Ack failed\n", inet_ntoa(in), preq->command);
		
		goto fail;
	}

	stream_ack.ackid = ntohl(stream_ack.ackid);
	stream_ack.errcode = ntohl(stream_ack.errcode);

	if (stream_ack.errcode != 0)
	{
		ERR_PRINT("svr IP: %s, stream req cmd: %d, ack errcode: %d\n", 
			inet_ntoa(in), preq->command, stream_ack.errcode);
		
		goto fail;
	}

	//success
	link_id = stream_ack.ackid;

	pstream = new SDevStream_t;
	if (NULL == pstream)
	{
		ERR_PRINT ("svr IP: %s, new SDevStream_t failed\n", inet_ntoa(in));

		ret = -ESPACE;
		goto fail2;
	}

	plock4stream->Lock();

	if (!map_sid_lid.insert(std::make_pair(stream_id, link_id)).second)
	{
		ERR_PRINT("svr IP: %s, stream req cmd: %d, map_lid_pstream insert failed\n", inet_ntoa(in), preq->command);

		ret = -ESTREAM_ID_CONFLICT;
		goto fail3;
	}
	
	if (!map_lid_pstream.insert(std::make_pair(link_id, pstream)).second)
	{
		ERR_PRINT("svr IP: %s, stream req cmd: %d, map_lid_pstream insert failed\n", inet_ntoa(in), preq->command);

		ret = -ESTREAM_ID_CONFLICT;
		goto fail4;
	}
	
	pstream->link_id = link_id;
	pstream->stream_id = stream_id;
	pstream->status = EM_STREAM_STATUS_RUNNING;
	pstream->req = *preq;//�����ֽ���
	
	plock4stream->Unlock();

	setsockopt(fd_tmp, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout0, sizeof(struct timeval));//�ָ�

	if (psock_stream)
	{
		*psock_stream = fd_tmp;
	}
	
	DBG_PRINT("svr IP: %s, stream req cmd: %d success, link_id: %u\n", 
			inet_ntoa(in), preq->command, link_id);
	
	//send msg to stream manager  ����Ҫ�ˣ��ϲ���������

	
	return SUCCESS;

fail4:
	map_sid_lid.erase(stream_id);
	
fail3:
	plock4stream->Unlock();
	
	if (pstream)
	{
		delete pstream;
		pstream = NULL;
	}

fail2:

	//�ر���
	if ((u32)INVALID_VALUE != link_id)
	{
		link_id = htonl(link_id);
		
		if (req_cmd == 0)
		{
			net_cmd = CTRL_CMD_STOPVIDEOMONITOR;
		}
		else if (req_cmd == 1)
		{
			net_cmd = CTRL_CMD_STOPFILEPLAY;
		}
		else if (req_cmd == 2)
		{
			net_cmd = CTRL_CMD_STOPTIMEPLAY;
		}
		else if (req_cmd == 3)
		{
			net_cmd = CTRL_CMD_STOPDOWNLOAD;
		}
		else
		{
			ERR_PRINT("stream req cmd: %d not support\n", req_cmd);
	
			goto fail;
		}
		
		DevNetCom(net_cmd, &link_id, sizeof(link_id), NULL, 0);
	}
	
fail:
	close(fd_tmp);	

	return -FAILURE;
}

//�ϲ���ùرգ��������ϴ���Ϣ
int CBizDevice::ReqStreamStopByStreamID(u32 stream_id, s32 stop_reason)//GLB_ERROR_NUM
{
	int ret = -FAILURE;
	struct in_addr in;
	in.s_addr = dev_info.deviceIP;
	u8 req_cmd = INVALID_VALUE;
	u16 net_cmd = INVALID_VALUE;
	u32 stream_link_id = INVALID_VALUE;
	u32 link_id = INVALID_VALUE;
	//s32 stream_errno = SUCCESS;
	EM_STREAM_STATUS_TYPE status;
	char buf[128];
	SDevStream_t* pstream = NULL;
	MAP_SID_LID::iterator map_sid_lid_iter;
	MAP_LID_PSTREAM::iterator map_iter;
	
	plock4stream->Lock();

	map_sid_lid_iter = map_sid_lid.find(stream_id);
	if (map_sid_lid_iter == map_sid_lid.end())//�п��ܣ����֮ǰ����ʧ��û��insert map����֮���ϲ�ر���ʱ
	{
		ERR_PRINT("svr IP: %s, stream_id(%u) map_sid_lid not find\n", 
			inet_ntoa(in), stream_id);
		
		plock4stream->Unlock();
		return -EPARAM;
	}
	link_id = map_sid_lid_iter->second;
	
	map_sid_lid.erase(map_sid_lid_iter);
	
	map_iter = map_lid_pstream.find(link_id);
	if (map_iter == map_lid_pstream.end())
	{
		ERR_PRINT("svr IP: %s, link_id(%u), map_lid_pstream not find\n", 
			inet_ntoa(in), link_id);
		
		plock4stream->Unlock();
		return -EPARAM;
	}
	pstream = map_iter->second;
	
	map_lid_pstream.erase(map_iter);
	
	if (NULL == pstream)
	{
		ERR_PRINT("svr IP: %s, link_id(%u), NULL == pstream\n", 
			inet_ntoa(in), link_id);
		
		plock4stream->Unlock();
		return -EPARAM;
	}

	stream_link_id = pstream->link_id;
	stream_id = pstream->stream_id;
	status = pstream->status;
	req_cmd = pstream->req.command;
	//stream_errno = pstream->stream_errno;

	DBG_PRINT("svr IP: %s, link_id(%u), stream errno: %d, stop_reason: %d\n",
		inet_ntoa(in), link_id, pstream->stream_errno, stop_reason);

	if ( SUCCESS == pstream->stream_errno
		&& SUCCESS != stop_reason)
	{
		pstream->stream_errno = stop_reason;
	}

	delete pstream;//free
	pstream = NULL;

	map_sid_lid.erase(stream_id);

	if (link_id != stream_link_id)
	{
		ERR_PRINT("svr IP: %s, link_id(%u) != stream_link_id(%u), inconceivable\n", 
			inet_ntoa(in), link_id, stream_link_id);
		
		plock4stream->Unlock();
		return -EPARAM;
	}
	
	if (status != EM_STREAM_STATUS_RUNNING)
	{
		ERR_PRINT("svr IP: %s, link_id(%u), stream status(%d) not connected\n", 
			inet_ntoa(in), link_id, status);

		plock4stream->Unlock();
		return SUCCESS;
	}
	
	//0��Ԥ�� 1���ļ��ط� 2��ʱ��ط� 3���ļ����� 4������ 
	//5 VOIP 6 �ļ���֡���� 7 ʱ�䰴֡���� 8 ͸��ͨ��
	//9 Զ�̸�ʽ��Ӳ�� 10 ������ץͼ 11 ��·��ʱ��ط� 12 ��ʱ�������ļ�
	if (req_cmd == 0)
	{
		net_cmd = CTRL_CMD_STOPVIDEOMONITOR;
	}
	else if (req_cmd == 1)
	{
		net_cmd = CTRL_CMD_STOPFILEPLAY;
	}
	else if (req_cmd == 2)
	{
		net_cmd = CTRL_CMD_STOPTIMEPLAY;
	}
	else if (req_cmd == 3)
	{
		net_cmd = CTRL_CMD_STOPDOWNLOAD;
	}
	else
	{
		ERR_PRINT("svr IP: %s, link_id(%u), stream req cmd: %d not support\n",
			inet_ntoa(in), link_id, req_cmd);
		
		plock4stream->Unlock();

		return -EPARAM;
	}

	plock4stream->Unlock();

	link_id = htonl(link_id);
	ret = DevNetCom(net_cmd, &link_id, sizeof(link_id), buf, sizeof(buf));
	if (ret)
	{
		ERR_PRINT("svr IP: %s, link_id(%u), DevNetCom cmd(%d) failed, ret: %d\n",
			inet_ntoa(in), link_id, net_cmd, ret);
	}

#if 0 
 	//send msg to stream manager
 	memset(&msg, 0, sizeof(SBizMsg_t));
	msg.msg_type = EM_STREAM_MSG_STOP;
	msg.stream_err.stream_id = pstream->stream_id;
	msg.stream_err.stream_errno = stream_errno;
	
 	ret = BizSendMsg2StreamManager(&msg, sizeof(SBizMsg_t));
	if (ret)
	{
		ERR_PRINT("svr IP: %s, link_id(%u), msg_type: %d, BizSendMsg2StreamManager failed, ret: %d\n",
			inet_ntoa(in), link_id, msg.msg_type, ret);
 	}
#endif
	return SUCCESS;
}

//ģ���ڲ����õĹرգ����ó���:
// 1���豸���߻�����ͨ�ų�����ã�
// 2���ϲ�ɾ���豸����
//��Ҫ�ϴ���Ϣ��biz_stream_manager
int CBizDevice::StreamStopByLinkID(u32 link_id, s32 stop_reason)//GLB_ERROR_NUM
{
	int ret = -FAILURE;
	struct in_addr in;
	in.s_addr = dev_info.deviceIP;
	u8 req_cmd = INVALID_VALUE;
	u16 net_cmd = INVALID_VALUE;
	u32 stream_link_id = INVALID_VALUE;
	u32 stream_id = INVALID_VALUE;
	s32 stream_errno = SUCCESS;
	EM_STREAM_STATUS_TYPE status;
	char buf[128];
	SDevStream_t* pstream = NULL;
	MAP_LID_PSTREAM::iterator map_iter;
	SBizMsg_t msg;
	
	plock4stream->Lock();
	
	map_iter = map_lid_pstream.find(link_id);
	if (map_iter == map_lid_pstream.end())
	{
		ERR_PRINT("svr IP: %s, link_id(%u), map_lid_pstream not find\n", 
			inet_ntoa(in), link_id);//���ܷ�����StreamStart ��map_lid_pstream.insert ��֮ǰʧ��
		
		plock4stream->Unlock();
		return -EPARAM;
	}
	pstream = map_iter->second;
	
	map_lid_pstream.erase(map_iter);
	
	if (NULL == pstream)
	{
		ERR_PRINT("svr IP: %s, link_id(%u), NULL == pstream\n", 
			inet_ntoa(in), link_id);
		
		plock4stream->Unlock();
		return -EPARAM;
	}

	stream_link_id = pstream->link_id;
	stream_id = pstream->stream_id;
	status = pstream->status;
	req_cmd = pstream->req.command;

	DBG_PRINT("svr IP: %s, link_id(%u), stream errno: %d, stop_reason: %d\n",
			inet_ntoa(in), link_id, pstream->stream_errno, stop_reason);

	if (SUCCESS == pstream->stream_errno
		&& SUCCESS != stop_reason)
	{
		pstream->stream_errno = stop_reason;
	}
	
	stream_errno = pstream->stream_errno;

	delete pstream;//�ͷ�
	pstream = NULL;

	map_sid_lid.erase(stream_id);//�Ƴ�

	if (link_id != stream_link_id)
	{
		ERR_PRINT("svr IP: %s, link_id(%u) != stream_link_id(%u), inconceivable\n", 
			inet_ntoa(in), link_id, stream_link_id);
		
		plock4stream->Unlock();
		return -EPARAM;
	}
	
	if (EM_STREAM_STATUS_RUNNING != status)
	{
		ERR_PRINT("svr IP: %s, link_id(%u), EM_STREAM_STATUS_RUNNING != status(%d)\n", 
			inet_ntoa(in), link_id, status);

		plock4stream->Unlock();
		return SUCCESS;
	}
	
	//0��Ԥ�� 1���ļ��ط� 2��ʱ��ط� 3���ļ����� 4������ 
	//5 VOIP 6 �ļ���֡���� 7 ʱ�䰴֡���� 8 ͸��ͨ��
	//9 Զ�̸�ʽ��Ӳ�� 10 ������ץͼ 11 ��·��ʱ��ط� 12 ��ʱ�������ļ�
	if (req_cmd == 0)
	{
		net_cmd = CTRL_CMD_STOPVIDEOMONITOR;
	}
	else if (req_cmd == 1)
	{
		net_cmd = CTRL_CMD_STOPFILEPLAY;
	}
	else if (req_cmd == 2)
	{
		net_cmd = CTRL_CMD_STOPTIMEPLAY;
	}
	else if (req_cmd == 3)
	{
		net_cmd = CTRL_CMD_STOPDOWNLOAD;
	}
	else
	{
		ERR_PRINT("svr IP: %s, link_id(%u), stream req cmd: %d not support\n",
			inet_ntoa(in), link_id, req_cmd);
		
		plock4stream->Unlock();

		return -EPARAM;
	}

	link_id = htonl(link_id);
	ret = DevNetCom(net_cmd, &link_id, sizeof(link_id), buf, sizeof(buf));
	if (ret)
	{
		ERR_PRINT("svr IP: %s, link_id(%u), DevNetCom cmd(%d) failed, ret: %d\n",
			inet_ntoa(in), link_id, net_cmd, ret);
	}

	plock4stream->Unlock();

 	//send msg to stream manager
 	memset(&msg, 0, sizeof(SBizMsg_t));
	msg.msg_type = EM_STREAM_MSG_STOP;
	msg.un_part_chn.stream_id = pstream->stream_id;
	msg.un_part_data.stream_errno = stream_errno;
	
 	ret = BizSendMsg2StreamManager(&msg, sizeof(SBizMsg_t));
	if (ret)
	{
		ERR_PRINT("svr IP: %s, link_id(%u), msg_type: %d, BizSendMsg2StreamManager failed, ret: %d\n",
			inet_ntoa(in), link_id, msg.msg_type, ret);
 	}

	return SUCCESS;
}

int CBizDevice::StreamProgress(u32 stream_id, VD_BOOL b) //���ջطŽ�����Ϣ
{
	int ret = -FAILURE;
	struct in_addr in;
	in.s_addr = dev_info.deviceIP;
	u32 sndlen = 0;
	char sndbuf[128];
	//char rcvbuf[128];
	SDevStream_t* pstream = NULL;
	u32 link_id = INVALID_VALUE;//need convert htonl
	MAP_SID_LID::iterator map_sid_lid_iter;
	MAP_LID_PSTREAM::iterator map_iter;
	u8 req_cmd = 0;

	plock4stream->Lock();

	map_sid_lid_iter = map_sid_lid.find(stream_id);
	if (map_sid_lid_iter == map_sid_lid.end())
	{
		ERR_PRINT("svr IP: %s, stream_id(%u) map_sid_lid not find\n", 
			inet_ntoa(in), stream_id);
		
		plock4stream->Unlock();
		return -EPARAM;
	}
	link_id = map_sid_lid_iter->second;
	
	map_iter = map_lid_pstream.find(link_id);
	if (map_iter == map_lid_pstream.end())
	{
		ERR_PRINT("svr IP: %s, link_id(%u) not find\n", 
			inet_ntoa(in), link_id);
		
		plock4stream->Unlock();
		return -EPARAM;
	}

	pstream = map_iter->second;
	if (NULL == pstream)
	{
		ERR_PRINT("svr IP: %s, link_id(%u), NULL == pstream\n", 
			inet_ntoa(in), link_id);
		
		plock4stream->Unlock();
		return -EPARAM;
	}
	
	//0��Ԥ�� 1���ļ��ط� 2��ʱ��ط� 3���ļ����� 4������ 
	//5 VOIP 6 �ļ���֡���� 7 ʱ�䰴֡���� 8 ͸��ͨ��
	//9 Զ�̸�ʽ��Ӳ�� 10 ������ץͼ 11 ��·��ʱ��ط� 12 ��ʱ�������ļ�
	req_cmd = pstream->req.command;
	
	plock4stream->Unlock();
	
	if (req_cmd < 1 || req_cmd >3)
	{
		ERR_PRINT("svr IP: %s, link_id(%u), req_cmd(%d) not support progress\n", 
			inet_ntoa(in), link_id, req_cmd);
		
		return -EPARAM;
	}
	
	char flagSend = b?1:0;
	u32 send_link_id = htonl(link_id);
	
	memcpy(sndbuf, &send_link_id, sizeof(send_link_id));
	memcpy(sndbuf + sizeof(send_link_id), &flagSend, sizeof(flagSend));
	sndlen = sizeof(send_link_id) + sizeof(flagSend);
	
	//DBG_PRINT("sndlen: %d\n", sndlen);
	
	ret = DevNetCom(CTRL_CMD_PLAYPROGRESS, sndbuf, sndlen, NULL, 0);
	if (ret)
	{
		ERR_PRINT("DevNetCom CTRL_CMD_PLAYPROGRESS failed, ret: %d\n", ret);

		return ret;
	}

	return SUCCESS;
}

#if 0
void CBizDevice::_CleanStream(int stream_idx)
{
	//plock4stream->Lock();
	if (INVALID_SOCKET != stream_rcv[stream_idx].sockfd)
	{
		close(stream_rcv[stream_idx].sockfd);
		stream_rcv[stream_idx].sockfd = INVALID_SOCKET;
	}

	stream_rcv[stream_idx].b_connect = FALSE;
	stream_rcv[stream_idx].linkid = INVALID_VALUE;
	stream_rcv[stream_idx].pstream = NULL;
	memset(&stream_rcv[stream_idx].req, 0, sizeof(ifly_TCP_Stream_Req));	
	
	//plock4stream->Unlock();
}
#endif

//�طſ���
int CBizDevice::StreamPause(u32 stream_id)
{
	int ret = -FAILURE;
	struct in_addr in;
	in.s_addr = dev_info.deviceIP;
	u32 sndlen = 0;
	char sndbuf[128];
	//char rcvbuf[128];
	SDevStream_t* pstream = NULL;
	u32 link_id = INVALID_VALUE;//need convert htonl
	MAP_SID_LID::iterator map_sid_lid_iter;
	MAP_LID_PSTREAM::iterator map_iter;
	u8 req_cmd = 0;

	plock4stream->Lock();

	map_sid_lid_iter = map_sid_lid.find(stream_id);
	if (map_sid_lid_iter == map_sid_lid.end())
	{
		ERR_PRINT("svr IP: %s, stream_id(%u) map_sid_lid not find\n", 
			inet_ntoa(in), stream_id);
		
		plock4stream->Unlock();
		return -EPARAM;
	}
	link_id = map_sid_lid_iter->second;
	
	map_iter = map_lid_pstream.find(link_id);
	if (map_iter == map_lid_pstream.end())
	{
		ERR_PRINT("svr IP: %s, link_id(%u) not find\n", 
			inet_ntoa(in), link_id);
		
		plock4stream->Unlock();
		return -EPARAM;
	}

	pstream = map_iter->second;
	if (NULL == pstream)
	{
		ERR_PRINT("svr IP: %s, link_id(%u), NULL == pstream\n", 
			inet_ntoa(in), link_id);
		
		plock4stream->Unlock();
		return -EPARAM;
	}
	
	//0��Ԥ�� 1���ļ��ط� 2��ʱ��ط� 3���ļ����� 4������ 
	//5 VOIP 6 �ļ���֡���� 7 ʱ�䰴֡���� 8 ͸��ͨ��
	//9 Զ�̸�ʽ��Ӳ�� 10 ������ץͼ 11 ��·��ʱ��ط� 12 ��ʱ�������ļ�
	req_cmd = pstream->req.command;
	
	plock4stream->Unlock();
	
	if (req_cmd != 1 && req_cmd != 2)//�����ǻط�
	{
		ERR_PRINT("svr IP: %s, link_id(%u), req_cmd(%d) not playback\n", 
			inet_ntoa(in), link_id, req_cmd);
		
		return -EPARAM;
	}
	
	u32 send_link_id = htonl(link_id);
	
	memcpy(sndbuf, &send_link_id, sizeof(send_link_id));
	sndlen = sizeof(send_link_id);
	
	//DBG_PRINT("sndlen: %d\n", sndlen);
	
	ret = DevNetCom(CTRL_CMD_PAUSEPLAY, sndbuf, sndlen, NULL, 0);
	if (ret)
	{
		ERR_PRINT("DevNetCom CTRL_CMD_PAUSEPLAY failed, ret: %d\n", ret);

		return ret;
	}

	return SUCCESS;
}

int CBizDevice::StreamResume(u32 stream_id)
{
	int ret = -FAILURE;
	struct in_addr in;
	in.s_addr = dev_info.deviceIP;
	u32 sndlen = 0;
	char sndbuf[128];
	//char rcvbuf[128];
	SDevStream_t* pstream = NULL;
	u32 link_id = INVALID_VALUE;//need convert htonl
	MAP_SID_LID::iterator map_sid_lid_iter;
	MAP_LID_PSTREAM::iterator map_iter;
	u8 req_cmd = 0;

	plock4stream->Lock();

	map_sid_lid_iter = map_sid_lid.find(stream_id);
	if (map_sid_lid_iter == map_sid_lid.end())
	{
		ERR_PRINT("svr IP: %s, stream_id(%u) map_sid_lid not find\n", 
			inet_ntoa(in), stream_id);
		
		plock4stream->Unlock();
		return -EPARAM;
	}
	link_id = map_sid_lid_iter->second;
	
	map_iter = map_lid_pstream.find(link_id);
	if (map_iter == map_lid_pstream.end())
	{
		ERR_PRINT("svr IP: %s, link_id(%u) not find\n", 
			inet_ntoa(in), link_id);
		
		plock4stream->Unlock();
		return -EPARAM;
	}

	pstream = map_iter->second;
	if (NULL == pstream)
	{
		ERR_PRINT("svr IP: %s, link_id(%u), NULL == pstream\n", 
			inet_ntoa(in), link_id);
		
		plock4stream->Unlock();
		return -EPARAM;
	}
	
	//0��Ԥ�� 1���ļ��ط� 2��ʱ��ط� 3���ļ����� 4������ 
	//5 VOIP 6 �ļ���֡���� 7 ʱ�䰴֡���� 8 ͸��ͨ��
	//9 Զ�̸�ʽ��Ӳ�� 10 ������ץͼ 11 ��·��ʱ��ط� 12 ��ʱ�������ļ�
	req_cmd = pstream->req.command;
	
	plock4stream->Unlock();
	
	if (req_cmd != 1 && req_cmd != 2)//�����ǻط�
	{
		ERR_PRINT("svr IP: %s, link_id(%u), req_cmd(%d) not playback\n", 
			inet_ntoa(in), link_id, req_cmd);
		
		return -EPARAM;
	}
	
	u32 send_link_id = htonl(link_id);
	
	memcpy(sndbuf, &send_link_id, sizeof(send_link_id));
	sndlen = sizeof(send_link_id);
	
	//DBG_PRINT("sndlen: %d\n", sndlen);
	
	ret = DevNetCom(CTRL_CMD_RESUMEPLAY, sndbuf, sndlen, NULL, 0);
	if (ret)
	{
		ERR_PRINT("DevNetCom CTRL_CMD_RESUMEPLAY failed, ret: %d\n", ret);

		return ret;
	}

	return SUCCESS;
}

int CBizDevice::StreamStep(u32 stream_id)//֡��
{
	int ret = -FAILURE;
	struct in_addr in;
	in.s_addr = dev_info.deviceIP;
	u32 sndlen = 0;
	char sndbuf[128];
	//char rcvbuf[128];
	SDevStream_t* pstream = NULL;
	u32 link_id = INVALID_VALUE;//need convert htonl
	MAP_SID_LID::iterator map_sid_lid_iter;
	MAP_LID_PSTREAM::iterator map_iter;
	u8 req_cmd = 0;

	plock4stream->Lock();

	map_sid_lid_iter = map_sid_lid.find(stream_id);
	if (map_sid_lid_iter == map_sid_lid.end())
	{
		ERR_PRINT("svr IP: %s, stream_id(%u) map_sid_lid not find\n", 
			inet_ntoa(in), stream_id);
		
		plock4stream->Unlock();
		return -EPARAM;
	}
	link_id = map_sid_lid_iter->second;
	
	map_iter = map_lid_pstream.find(link_id);
	if (map_iter == map_lid_pstream.end())
	{
		ERR_PRINT("svr IP: %s, link_id(%u) not find\n", 
			inet_ntoa(in), link_id);
		
		plock4stream->Unlock();
		return -EPARAM;
	}

	pstream = map_iter->second;
	if (NULL == pstream)
	{
		ERR_PRINT("svr IP: %s, link_id(%u), NULL == pstream\n", 
			inet_ntoa(in), link_id);
		
		plock4stream->Unlock();
		return -EPARAM;
	}
	
	//0��Ԥ�� 1���ļ��ط� 2��ʱ��ط� 3���ļ����� 4������ 
	//5 VOIP 6 �ļ���֡���� 7 ʱ�䰴֡���� 8 ͸��ͨ��
	//9 Զ�̸�ʽ��Ӳ�� 10 ������ץͼ 11 ��·��ʱ��ط� 12 ��ʱ�������ļ�
	req_cmd = pstream->req.command;
	
	plock4stream->Unlock();
	
	if (req_cmd != 1 && req_cmd != 2)//�����ǻط�
	{
		ERR_PRINT("svr IP: %s, link_id(%u), req_cmd(%d) not playback\n", 
			inet_ntoa(in), link_id, req_cmd);
		
		return -EPARAM;
	}
	
	u32 send_link_id = htonl(link_id);
	
	memcpy(sndbuf, &send_link_id, sizeof(send_link_id));
	sndlen = sizeof(send_link_id);
	
	//DBG_PRINT("sndlen: %d\n", sndlen);
	
	ret = DevNetCom(CTRL_CMD_SINGLEPLAY, sndbuf, sndlen, NULL, 0);
	if (ret)
	{
		ERR_PRINT("DevNetCom CTRL_CMD_SINGLEPLAY failed, ret: %d\n", ret);

		return ret;
	}

	return SUCCESS;
}

int CBizDevice::StreamRate(u32 stream_id, s32 rate)//{-8, -4, -2, 1, 2, 4, 8}
{
	int ret = -FAILURE;
	struct in_addr in;
	in.s_addr = dev_info.deviceIP;
	u32 sndlen = 0;
	char sndbuf[128];
	//char rcvbuf[128];
	SDevStream_t* pstream = NULL;
	u32 link_id = INVALID_VALUE;//need convert htonl
	MAP_SID_LID::iterator map_sid_lid_iter;
	MAP_LID_PSTREAM::iterator map_iter;
	u8 req_cmd = 0;

	plock4stream->Lock();

	map_sid_lid_iter = map_sid_lid.find(stream_id);
	if (map_sid_lid_iter == map_sid_lid.end())
	{
		ERR_PRINT("svr IP: %s, stream_id(%u) map_sid_lid not find\n", 
			inet_ntoa(in), stream_id);
		
		plock4stream->Unlock();
		return -EPARAM;
	}
	link_id = map_sid_lid_iter->second;
	
	map_iter = map_lid_pstream.find(link_id);
	if (map_iter == map_lid_pstream.end())
	{
		ERR_PRINT("svr IP: %s, link_id(%u) not find\n", 
			inet_ntoa(in), link_id);
		
		plock4stream->Unlock();
		return -EPARAM;
	}

	pstream = map_iter->second;
	if (NULL == pstream)
	{
		ERR_PRINT("svr IP: %s, link_id(%u), NULL == pstream\n", 
			inet_ntoa(in), link_id);
		
		plock4stream->Unlock();
		return -EPARAM;
	}
	
	//0��Ԥ�� 1���ļ��ط� 2��ʱ��ط� 3���ļ����� 4������ 
	//5 VOIP 6 �ļ���֡���� 7 ʱ�䰴֡���� 8 ͸��ͨ��
	//9 Զ�̸�ʽ��Ӳ�� 10 ������ץͼ 11 ��·��ʱ��ط� 12 ��ʱ�������ļ�
	req_cmd = pstream->req.command;
	
	plock4stream->Unlock();
	
	if (req_cmd != 1 && req_cmd != 2)//�����ǻط�
	{
		ERR_PRINT("svr IP: %s, link_id(%u), req_cmd(%d) not playback\n", 
			inet_ntoa(in), link_id, req_cmd);
		
		return -EPARAM;
	}
	
	int nRate = htonl(rate);
	u32 send_link_id = htonl(link_id);
	
	memcpy(sndbuf, &send_link_id, sizeof(send_link_id));
	memcpy(sndbuf + sizeof(send_link_id), &nRate, sizeof(nRate));
	sndlen = sizeof(send_link_id) + sizeof(nRate);
	
	//DBG_PRINT("sndlen: %d\n", sndlen);
	
	ret = DevNetCom(CTRL_CMD_SETPLAYRATE, sndbuf, sndlen, NULL, 0);
	if (ret)
	{
		ERR_PRINT("DevNetCom CTRL_CMD_SETPLAYRATE failed, ret: %d\n", ret);

		return ret;
	}

	return SUCCESS;
}

int CBizDevice::StreamSeek(u32 stream_id, u32 time)
{
	int ret = -FAILURE;
	struct in_addr in;
	in.s_addr = dev_info.deviceIP;
	u32 sndlen = 0;
	char sndbuf[128];
	//char rcvbuf[128];
	SDevStream_t* pstream = NULL;
	u32 link_id = INVALID_VALUE;//need convert htonl
	MAP_SID_LID::iterator map_sid_lid_iter;
	MAP_LID_PSTREAM::iterator map_iter;
	u8 req_cmd = 0;

	plock4stream->Lock();

	map_sid_lid_iter = map_sid_lid.find(stream_id);
	if (map_sid_lid_iter == map_sid_lid.end())
	{
		ERR_PRINT("svr IP: %s, stream_id(%u) map_sid_lid not find\n", 
			inet_ntoa(in), stream_id);
		
		plock4stream->Unlock();
		return -EPARAM;
	}
	link_id = map_sid_lid_iter->second;
	
	map_iter = map_lid_pstream.find(link_id);
	if (map_iter == map_lid_pstream.end())
	{
		ERR_PRINT("svr IP: %s, link_id(%u) not find\n", 
			inet_ntoa(in), link_id);
		
		plock4stream->Unlock();
		return -EPARAM;
	}

	pstream = map_iter->second;
	if (NULL == pstream)
	{
		ERR_PRINT("svr IP: %s, link_id(%u), NULL == pstream\n", 
			inet_ntoa(in), link_id);
		
		plock4stream->Unlock();
		return -EPARAM;
	}
	
	//0��Ԥ�� 1���ļ��ط� 2��ʱ��ط� 3���ļ����� 4������ 
	//5 VOIP 6 �ļ���֡���� 7 ʱ�䰴֡���� 8 ͸��ͨ��
	//9 Զ�̸�ʽ��Ӳ�� 10 ������ץͼ 11 ��·��ʱ��ط� 12 ��ʱ�������ļ�
	req_cmd = pstream->req.command;
	
	plock4stream->Unlock();
	
	if (req_cmd != 1 && req_cmd != 2)//�����ǻط�
	{
		ERR_PRINT("svr IP: %s, link_id(%u), req_cmd(%d) not playback\n", 
			inet_ntoa(in), link_id, req_cmd);
		
		return -EPARAM;
	}
	
	u32 seek_time = htonl(time);
	u32 send_link_id = htonl(link_id);
	
	memcpy(sndbuf, &send_link_id, sizeof(send_link_id));
	memcpy(sndbuf + sizeof(send_link_id), &seek_time, sizeof(seek_time));
	sndlen = sizeof(send_link_id) + sizeof(seek_time);
	
	//DBG_PRINT("sndlen: %d\n", sndlen);
	
	ret = DevNetCom(CTRL_CMD_PLAYSEEK, sndbuf, sndlen, NULL, 0);
	if (ret)
	{
		ERR_PRINT("DevNetCom CTRL_CMD_PLAYSEEK failed, ret: %d\n", ret);

		return ret;
	}

	return SUCCESS;
}





//ģ���ڲ����õĹرգ�
// 1���豸���߻�����ͨ�ų������
// 2���ϲ�ɾ���豸����
int CBizDevice::ShutdownStreamAll(s32 stop_reason)
{
	MAP_LID_PSTREAM::iterator map_iter;
	
	plock4stream->Lock();
	
	while (1)
	{
		map_iter = map_lid_pstream.begin();
		
		if (map_iter == map_lid_pstream.end())
		{
			break;
		}
		
		StreamStopByLinkID(map_iter->first, stop_reason);//�����erase map iterator
	}

	plock4stream->Unlock();	
	
	return SUCCESS;
}

//������������������
int CBizDevice::CheckAndReconnectStream()
{
	return SUCCESS;
}

#if 0
//֧���ⲿ�����˳��Ϳ���һ��ʱ��������˳�
void CBizDevice::threadStreamRcv(uint param)
{	
	int i = 0;	
	fd_set rset;
	struct timeval timeout;
	int fd_max = INVALID_SOCKET;
	int fd_tmp = INVALID_SOCKET;
	int ret = SUCCESS;
	FRAMEHDR frame_hdr;
	ifly_MediaFRAMEHDR_t hdr;
	char *pframe_data = NULL;
	u8 req_command = 0;
	CMediaStream* pstream = NULL;
	struct in_addr in;
	
	in.s_addr = dev_info.deviceIP;

	pframe_data = new char[MAX_FRAME_SIZE];
	if (NULL == pframe_data)
	{
		DBG_PRINT("svr IP: %s, CBizDevice::threadStreamRcv malloc frame buffer failed\n", inet_ntoa(in));
		return ;
	}

	DBG_PRINT("svr IP: %s, CBizDevice::threadStreamRcv running\n", inet_ntoa(in));

	while (1)
	{
		FD_ZERO(&rset);
		fd_max = INVALID_SOCKET;
		timeout.tv_sec = 1;
		timeout.tv_usec = 0;
		
		plock4stream->Lock();//plock4stream->Unlock();

		if (bthread_stream_exit)//�ⲿ�����˳�
		{
			DBG_PRINT("bthread_stream_exit: %d\n", bthread_stream_exit);
			bthread_stream_running = FALSE;
			
			plock4stream->Unlock();			
			break;
		}
	
		for (i = 0; i < MaxMediaLinks; ++i)
		{
			fd_tmp = stream_rcv[i].sockfd;
			if (stream_rcv[i].b_connect && (INVALID_SOCKET != fd_tmp))
			{
				FD_SET(fd_tmp, &rset);
				fd_max = MAX(fd_tmp, fd_max);
			}
		}

		if (INVALID_SOCKET == fd_max)
		{	
			if (++idle_cnt < MaxIdleNum)
			{
				plock4stream->Unlock();
				
				sleep(1);
				continue;
			}
			else	//����һ��ʱ�������˳�
			{
				DBG_PRINT("idle exit\n");
				bthread_stream_running = FALSE;
				plock4stream->Unlock();
				break;				
			}
		}
		
		idle_cnt = 0;
		plock4stream->Unlock();

		ret = select(fd_max+1, &rset, NULL, NULL, &timeout);
		if (ret < 0)
		{
			ERR_PRINT("select failed\n");
			sleep(1);
			continue;
		}
		else if (ret == 0)//timeout
		{
			continue;
		}

		plock4stream->Lock();  //plock4stream->Unlock();
		for (i = 0; i < MaxMediaLinks; ++i)
		{
			//plock4stream->Lock();
			
			fd_tmp = stream_rcv[i].sockfd;
			//0��Ԥ�� 1���ļ��ط� 2��ʱ��ط� 3���ļ����� 4������ 
			//5 VOIP 6 �ļ���֡���� 7 ʱ�䰴֡���� 8 ͸��ͨ��
			//9 Զ�̸�ʽ��Ӳ�� 10 ������ץͼ 11 ��·��ʱ��ط� 12 ��ʱ�������ļ�
			req_command = stream_rcv[i].req.command;
			pstream = stream_rcv[i].pstream;
			
			//plock4stream->Unlock();
			
			if ((INVALID_SOCKET != fd_tmp) && FD_ISSET(fd_tmp, &rset))
			{				
				memset(&frame_hdr, 0, sizeof(FRAMEHDR));
				
				if (req_command < 3)
				{
					ret = looprecv(fd_tmp, (char *)&hdr, sizeof(ifly_MediaFRAMEHDR_t));
					if (ret)
					{
						ERR_PRINT("svr IP: %s, recv ifly_MediaFRAMEHDR_t failed\n", inet_ntoa(in));
						goto fail;
					}
					
					hdr.m_dwDataSize = ntohl(hdr.m_dwDataSize);
					hdr.m_dwFrameID = ntohl(hdr.m_dwFrameID);
					hdr.m_dwTimeStamp = ntohl(hdr.m_dwTimeStamp);
					hdr.m_nVideoHeight = ntohl(hdr.m_nVideoHeight);
					hdr.m_nVideoWidth = ntohl(hdr.m_nVideoWidth);

					if (hdr.m_dwDataSize > MAX_FRAME_SIZE)
					{
						ERR_PRINT("svr IP: %s, recv DataSize(%d) > MAX_FRAME_SIZE(%d) failed\n",
							inet_ntoa(in), hdr.m_dwDataSize, MAX_FRAME_SIZE);
						goto fail;
					}
					
					ret = looprecv(fd_tmp, pframe_data, hdr.m_dwDataSize);
					if (ret)
					{
						ERR_PRINT("svr IP: %s, recv one frame failed\n", inet_ntoa(in));
						goto fail;
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
					if (hdr.m_bKeyFrame)
					{
						DBG_PRINT("svr IP: %s, recv one frame, command: %d, m_byMediaType: %d, m_bKeyFrame: %d, m_dwFrameID: %d, m_dwDataSize: %d\n", 
							inet_ntoa(in), req_command, hdr.m_byMediaType, hdr.m_bKeyFrame, hdr.m_dwFrameID, hdr.m_dwDataSize);
					}
#endif					
					if (pstream != NULL)
					{
						pstream->dealFrameFunc(&frame_hdr);
					}
				}
				else if (req_command == 3)	//�ļ�����
				{
					ret = recv(fd_tmp, pframe_data, MAX_FRAME_SIZE, 0);
					if (ret <= 0)
					{
						DBG_PRINT("svr IP: %s, recv one frame failed\n", inet_ntoa(in));
						goto fail;
					}
					frame_hdr.m_pData = (u8 *)pframe_data;
					frame_hdr.m_dwDataSize = ret;

					if (pstream != NULL)
					{
						pstream->dealFrameFunc(&frame_hdr);
					}
				}
				else
				{
					DBG_PRINT("svr IP: %s, stream req command(%d) not supported\n", inet_ntoa(in), req_command);
				}

				continue;//��������ڴ�
				
fail: //���ճ���
				
				StreamStop(i, EM_STREAM_RCV_ERR);
			#if 0
				//plock4stream->Lock();
				_CleanStream(i);
				if (pstream != NULL)
				{
					pstream->dealStateFunc(EM_STREAM_RCV_ERR);//������մ���	
				}
				
				//plock4stream->Unlock();
			#endif
			}
		}
		plock4stream->Unlock();
	}

	if (NULL != pframe_data)
	{
		delete [] pframe_data;
		pframe_data = NULL;
	}

	sem_exit.Post();
	
	DBG_PRINT("svr IP: %s, CBizDevice::threadStreamRcv exit\n", inet_ntoa(in));
}
#endif






/********************************************************************
							Device Manager
********************************************************************/

PATTERN_SINGLETON_IMPLEMENT(CBizDeviceManager);

/********************************************************************
							Device  Manager
********************************************************************/
CBizDeviceManager::CBizDeviceManager()
: b_inited(FALSE)
//, b_notify_devinfo(FALSE)
, plock_dev_pool(NULL)
, pplock_dev(NULL)
, ppcdev(NULL)
, pool_cur_idx(0)
, pool_size(0)
, pool_dev_count(0)
, plock4param(NULL)
, max_link_devs(0)
, m_reconnect_timer(NULL)
, reconnect_sem(0)
, plock_list_del(NULL)
, plock_map_fd_idx(NULL)
//, plock4sync_num(NULL)
, sync_num(0)
, plock4sock(NULL)
, sync_sem(0)
, send_buf(NULL)
, sync_buf(NULL)
, sync_ack_len(0)
//��Ϣ��������
, mq_msg_count(0)
, pmq_ccbuf(NULL)
{
	
}

CBizDeviceManager::~CBizDeviceManager()
{
	FreeSrc();
}

int CBizDeviceManager::FirstInit(void)
{
	int ret;
	std::vector<u32> dev_list;
	std::vector<u32>::iterator iter;
	SConfigDvrProperty sdev_property;
	u32 size = 0;
	u32 maxuint = 0;
	u32 andv = 0;
	s32 i;

	str_dev_list.clear();
	str_dev_list.push_back("EM_NVR");
	str_dev_list.push_back("EM_PATROL_DEC");
	str_dev_list.push_back("EM_SWITCH_DEC");
	
	//���������豸��Ŀ����(nvr + dec)
	ret = BizConfigGetDvrProperty(sdev_property);
	if (ret)
	{
		ERR_PRINT("BizConfigGetDvrProperty failed\n");
		goto fail;
	}
	
	max_link_devs = sdev_property.nNvr_nums + sdev_property.nDec_nums;
	
#if 0 //debug
	struct in_addr in;
	MAP_IP_IDX::iterator map_iter;
#endif

	//pool_size = max_link_devs *2 Բ��Ϊ2�Ĵ���
	size = max_link_devs * 2;
	if((size & (size-1)) == 0)//�Ƿ�Ϊ2�Ĵ���
	{
		pool_size = size;
	}
	else
	{
		maxuint = (u32)((u32)~0);
		andv = ~(maxuint&(maxuint>>1));
		
		while((andv & size) == 0)
			andv = andv>>1;
		
		pool_size = (int)andv<<1;
	}

	plock_dev_pool = new CMutex;
	if (NULL == plock_dev_pool)
	{
		ERR_PRINT("new plock_dev_pool failed\n");
		goto fail;
	}

	if (plock_dev_pool->Create())
	{
		ERR_PRINT("create plock_dev_pool failed\n");
		goto fail;
	}
	
	pplock_dev = new C_Lock*[pool_size];
	if (NULL == pplock_dev)
	{
		ERR_PRINT("new pplock_dev failed\n");
		goto fail;
	}
	memset((void *)pplock_dev, 0, pool_size*sizeof(C_Lock *));

	pool_bits.resize(pool_size);
	
	for(i=0; i < pool_size; i++)
	{
		pplock_dev[i] = new CMutex;
		if (NULL == pplock_dev[i])
		{
			ERR_PRINT("new pplock_dev[%d] failed\n", i);
			goto fail;
		}
		
		if (pplock_dev[i]->Create())
		{
			ERR_PRINT("create pplock_dev[%d] failed\n", i);
			goto fail;
		}

		pool_bits[i] = 0;//set all bits to 0
	}

	ppcdev = new CBizDevice*[pool_size];
	if (NULL == ppcdev)
	{
		ERR_PRINT("new ppcdev failed\n");
		goto fail;
	}
	memset((void *)ppcdev, 0, pool_size*sizeof(CBizDevice *));	
	
	//rw lock
	plock4param = new CRWlock;
	if (NULL == plock4param)
	{
		ERR_PRINT("new plock4param failed\n");
		goto fail;
	}

	if (plock4param->Create())
	{
		ERR_PRINT("create plock4param failed\n");
		goto fail;
	}
	
	//���DEV_MAP NVR
	dev_list.clear();
	ret = BizConfigGetNvrList(dev_list);
	if (ret)
	{
		ERR_PRINT("BizConfigGetNvrList failed\n");
		goto fail;
	}

	for (iter = dev_list.begin(); 
		iter != dev_list.end();
		++iter)
	{
		if (_AddDev2Map(EM_NVR, *iter))
		{
			ERR_PRINT("fillDevMap EM_NVR failed\n");
			goto fail;
		}
	}

#if 0 //debug
	if (!map_nvr.empty())
	{
		for (map_iter = map_nvr.begin();	\
		 	 map_iter != map_nvr.end();	\
			 ++map_iter)
		{
			in.s_addr = htonl(map_iter->first);
			printf("map nvr : %s\n", inet_ntoa(in));
		}
	}
#endif	

	//���DEV_MAP PATROL_DEC
	dev_list.clear();
	ret = BizConfigGetPatrolDecList(dev_list);
	if (ret)
	{
		ERR_PRINT("BizConfigGetNvrList failed\n");
		goto fail;
	}

	for (iter = dev_list.begin();
		iter != dev_list.end();
		++iter)
	{
		if (_AddDev2Map(EM_PATROL_DEC, *iter))
		{
			ERR_PRINT("fillDevMap EM_PATROL_DEC failed\n");
			goto fail;
		}
	}

	//���DEV_MAP SWITCH_DEC
	dev_list.clear();
	ret = BizConfigGetSwitchDecList(dev_list);
	if (ret)
	{
		ERR_PRINT("BizConfigGetNvrList failed\n");
		goto fail;
	}

	for (iter = dev_list.begin();
		iter != dev_list.end();
		++iter)
	{
		if (_AddDev2Map(EM_SWITCH_DEC, *iter))
		{
			ERR_PRINT("fillDevMap EM_SWITCH_DEC failed\n");
			goto fail;
		}
	}

	//m_reconnect_timer = new CTimer("CBizDeviceManager-reconnect_timer");
	//if (NULL == m_reconnect_timer)
	//{
	//	ERR_PRINT("BizConfigGetNvrList failed\n");
	//	goto fail;
	//}

	//��ɾ���豸
	plock_list_del= new CMutex;
	if (NULL == plock_list_del)
	{
		ERR_PRINT("new plock_list_del failed\n");
		goto fail;
	}

	if (plock_list_del->Create())
	{
		ERR_PRINT("create plock_list_del failed\n");
		goto fail;
	}
	
	//���н����豸��Ϣ
	plock_map_fd_idx = new CMutex;
	if (NULL == plock_map_fd_idx)
	{
		ERR_PRINT("new plock_map_fd_idx failed\n");
		goto fail;
	}

	if (plock_map_fd_idx->Create())
	{
		ERR_PRINT("create plock_map_fd_idx failed\n");
		goto fail;
	}
#if 0	
	plock4sync_num = new CMutex;
	if (NULL == plock4sync_num)
	{
		ERR_PRINT("new plock4sync_num failed\n");
		goto fail;
	}

	if (plock4sync_num->Create())
	{
		ERR_PRINT("create plock4sync_num failed\n");
		goto fail;
	}
#endif	
	plock4sock = new CMutex;
	if (NULL == plock4sock)
	{
		ERR_PRINT("new plock4sock failed\n");
		goto fail;
	}

	if (plock4sock->Create())
	{
		ERR_PRINT("create plock4sock failed\n");
		goto fail;
	}

	send_buf = new char[SendBuf];
	if (NULL == send_buf)
	{
		ERR_PRINT("new send_buf failed\n");
		goto fail;
	}
	
	sync_buf = new char[RcvBuf];
	if (NULL == sync_buf)
	{
		ERR_PRINT("new sync_buf failed\n");
		goto fail;
	}

	m_reconnect_timer = new CTimer("CBizDeviceManager-reconnect_timer");
	if (NULL == m_reconnect_timer)
	{
		ERR_PRINT("new CTimer failed\n");
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

	b_inited = TRUE;
	return SUCCESS;
	
fail:

	FreeSrc();
	return -FAILURE;
}

int CBizDeviceManager::SecondInit(void)
{
	//������Ϣ���ж�ȡ�߳�
	mq_msg_threadlet.run("BizDeviceManager-mq", this, (ASYNPROC)&CBizDeviceManager::threadMsg, 0, 0);
	
	//���������������߳�	
	m_threadlet_rcv.run("BizDeviceManager-rcv", this, (ASYNPROC)&CBizDeviceManager::threadRcv, 0, 0);

	//�����߳�: ����
	m_threadlet_keep_alive.run("BizDeviceManager-keep_alive", this, (ASYNPROC)&CBizDeviceManager::threadKeepAlive, 0, 0);
	
	return SUCCESS;
}

void CBizDeviceManager::FreeSrc()//�ͷ���Դ
{	
	MAP_IP_IDX::iterator map_iter;
	std::vector<CBizDevice *>::iterator iter;
	std::list<ifly_cp_t *>list_iter;
	s32 i = 0;
	
	b_inited = FALSE;

	if (!str_dev_list.empty())
	{
		str_dev_list.clear();
	}
	
	if (plock4param)
	{
		delete plock4param;
		plock4param = NULL;
	}

	//�豸����	
	//�ڲ�
	if (ppcdev)
	{
		delete []ppcdev;
		ppcdev = NULL;
	}

	if (pplock_dev)
	{
		for(i=0; i < pool_size; i++)
		{
			if (pplock_dev[i])
			{
				delete pplock_dev[i];
				pplock_dev[i] = NULL;
			}
		}

		delete []pplock_dev;
		pplock_dev = NULL;
	}
	
	if (plock_dev_pool)
	{
		delete plock_dev_pool;
		plock_dev_pool = NULL;
	}
	//���
	max_link_devs = 0;
	set_dev_ip.clear();

	int dev_type = 0;
	for (dev_type = EM_NVR; dev_type < EM_DEV_TYPE_MAX; ++dev_type)
    {
        map_dev[dev_type-EM_NVR].clear();
	}	

	if (m_reconnect_timer)
	{
		delete m_reconnect_timer;
		m_reconnect_timer = NULL;
	}

	//��ɾ���豸
	if (plock_list_del)
	{
		delete plock_list_del;
		plock_list_del = NULL;
	}

	list_dev_del.clear();

	//���н����豸��Ϣ
	if (plock_map_fd_idx)
	{
		delete plock_map_fd_idx;
		plock_map_fd_idx = NULL;
	}
#if 0	
	if (plock4sync_num)
	{
		delete plock4sync_num;
		plock4sync_num = NULL;
	}
#endif	
	if (plock4sock)
	{
		delete plock4sock;
		plock4sock = NULL;
	}

	if (send_buf)
	{
		delete []send_buf;
		send_buf = NULL;
	}
	
	if (sync_buf)
	{
		delete []sync_buf;
		sync_buf = NULL;
	}

	if (pmq_ccbuf)
	{
		delete pmq_ccbuf;
		pmq_ccbuf = NULL;
	}
}

VD_BOOL CBizDeviceManager::isInited()
{
	return b_inited;
}

int CBizDeviceManager::GetDevIPList(EM_DEV_TYPE dev_type, std::list<u32> &dev_ip_list)
{
	MAP_IP_IDX *pmap = NULL;
	MAP_IP_IDX::iterator map_iter;
	int ret = -FAILURE;

	if (!dev_ip_list.empty())//�ǿ�
	{
		ERR_PRINT("param vdev_list is not empty\n");
		return -FAILURE;
	}

	if (!b_inited)
	{
		ERR_PRINT("module not init\n");
		return -FAILURE;
	}
	
	if (plock4param->Rdlock())
	{
		ERR_PRINT("Rdlock failed\n");
		return -FAILURE;
	}

	if (dev_type <= EM_DEV_TYPE_NONE
            || dev_type >= EM_DEV_TYPE_MAX)
    {
        ERR_PRINT("dev_type(%d) not support\n", dev_type);
		
		ret = -EPARAM;		
        goto fail;
    }
    pmap = &map_dev[dev_type-EM_NVR];

	for (map_iter = pmap->begin();
	 	 map_iter != pmap->end();
		 ++map_iter)
	{				
		dev_ip_list.push_back(ntohl(map_iter->first));
	}

    plock4param->Unlock();
	return SUCCESS;

fail:
	
	plock4param->Unlock();
	return ret;
}

int CBizDeviceManager::GetDevIdx(EM_DEV_TYPE dev_type, u32 dev_ip)
{	
	MAP_IP_IDX *pmap = NULL;
	MAP_IP_IDX::iterator map_iter;
	std::string str_dev_type;
	int ret = -FAILURE;
	int dev_idx = -1;
	u32 ip_le = ntohl(dev_ip);
	struct in_addr in;
	in.s_addr = dev_ip;
	
	if (!b_inited)
	{
		ERR_PRINT("module not init\n");
		return -FAILURE;
	}

	if (dev_type <= EM_DEV_TYPE_NONE
            || dev_type >= EM_DEV_TYPE_MAX)
    {
        ERR_PRINT("dev_type(%d) not support\n", dev_type);
		
		return -EPARAM;
    }
	
	if (plock4param->Rdlock())
	{
		ERR_PRINT("Rdlock failed\n");
		return -FAILURE;
	}	
	
    pmap = &map_dev[dev_type-EM_NVR];
	str_dev_type = str_dev_list[dev_type-EM_NVR];	

	map_iter = pmap->find(ip_le);
	if (map_iter == pmap->end())
	{
		ERR_PRINT("%s map not found IP(%s) failed\n", str_dev_type.c_str(), inet_ntoa(in));
		
		ret = -EDEV_NOTFOUND;
		goto fail;
	}

	dev_idx = map_iter->second;
	if ((dev_idx < 0) || (dev_idx >= pool_size))
	{
		ERR_PRINT("%s dev(%s) idx(%d) out of range, pool_size: %d\n", 
			str_dev_type.c_str(), inet_ntoa(in), dev_idx, pool_size);
		
		ret = -EDEV_NOTFOUND;
		goto fail;
	}

	plock4param->Unlock();
	return dev_idx;
	
fail:
	
	plock4param->Unlock();
	return ret;
}

int CBizDeviceManager::GetDevInfo(EM_DEV_TYPE dev_type, u32 dev_ip, SGuiDev *pgdev)
{
	CBizDevice *pcdev = NULL;
	s32 dev_idx = -1;
	
	struct in_addr in;
	in.s_addr = dev_ip;

	dev_idx = GetDevIdx(dev_type, dev_ip);
	if (dev_idx < 0)
	{
		ERR_PRINT("IP(%s) GetDevIdx failed\n", inet_ntoa(in));
		
		return -EDEV_NOTFOUND;
	}
	
	pplock_dev[dev_idx]->Lock();
	
	pcdev = ppcdev[dev_idx];
	if (NULL == pcdev)
	{	
		ERR_PRINT("IP(%s) pcdev == NULL\n", inet_ntoa(in));
		
		pplock_dev[dev_idx]->Unlock();
		return -EDEV_NOTFOUND;		
	}

	pgdev->b_alive = pcdev->b_alive;
	pgdev->devicetype = pcdev->dev_info.devicetype;
	pgdev->deviceIP= pcdev->dev_info.deviceIP;
	pgdev->maxChnNum= pcdev->dev_info.maxChnNum;
	pgdev->dev_idx= pcdev->dev_idx;	

	pplock_dev[dev_idx]->Unlock();
	
	return SUCCESS;
}

//��ȡ����ͨ����IPC��Ϣ
int CBizDeviceManager::GetDevChnIPCInfo(EM_DEV_TYPE dev_type, u32 dev_ip, ifly_ipc_info_t * pipc_info, u32 size)
{	
	CBizDevice *pcdev = NULL;
	int dev_idx = -1;
	int ret = -FAILURE;
	struct in_addr in;
	in.s_addr = dev_ip;

	dev_idx = GetDevIdx(dev_type, dev_ip);
	if (dev_idx < 0)
	{
		ERR_PRINT("IP(%s) GetDevIdx failed\n", inet_ntoa(in));
		
		return -EDEV_NOTFOUND;
	}
	
	pplock_dev[dev_idx]->Lock();

	pcdev = ppcdev[dev_idx];
	if (NULL == pcdev)
	{	
		ERR_PRINT("IP(%s) pcdev == NULL\n", inet_ntoa(in));
		
		pplock_dev[dev_idx]->Unlock();
		return -EDEV_NOTFOUND;		
	}

	ret = pcdev->GetChnIPCInfo(pipc_info, size);
	if (ret)
	{
		ERR_PRINT("IP(%s) GetChnIPCInfo failed, ret: %d\n", inet_ntoa(in), ret);
		
		pplock_dev[dev_idx]->Unlock();		
		return ret;
	}	

	//success
	pplock_dev[dev_idx]->Unlock();
	return SUCCESS;
}

//ֻ֧��NVR���õ�NVRͨ����(osd info)
int CBizDeviceManager::GetDevChnName(EM_DEV_TYPE dev_type, u32 dev_ip, u8 chn, char *pbuf, u32 size)
{
	CBizDevice *pcdev = NULL;
	int dev_idx = -1;
	int ret = -FAILURE;
	struct in_addr in;
	in.s_addr = dev_ip;

	dev_idx = GetDevIdx(dev_type, dev_ip);
	if (dev_idx < 0)
	{
		ERR_PRINT("IP(%s) GetDevIdx failed\n", inet_ntoa(in));
		
		return -EDEV_NOTFOUND;
	}
	
	pplock_dev[dev_idx]->Lock();

	pcdev = ppcdev[dev_idx];
	if (NULL == pcdev)
	{	
		ERR_PRINT("IP(%s) pcdev == NULL\n", inet_ntoa(in));

		pplock_dev[dev_idx]->Unlock();
		return -EDEV_NOTFOUND;		
	}

	ret = pcdev->GetChnName(chn, pbuf, size);
	if (ret)
	{
		ERR_PRINT("IP(%s) GetChnName failed, ret: %d\n", inet_ntoa(in), ret);
		
		pplock_dev[dev_idx]->Unlock();	
		return ret;
	}	

	//success
	pplock_dev[dev_idx]->Unlock();
	return SUCCESS;
}

//���ý�����ͨ����Ӧ��NVR ͨ��
int CBizDeviceManager::SetDevChnIpc(EM_DEV_TYPE dev_type, u32 dec_ip , u8 dec_chn, u32 nvr_ip, u8 nvr_chn)
{
	CBizDevice *pcdev = NULL;
	int dev_idx = -1;
	int ret = -FAILURE;
	struct in_addr in;

	//check nvr
	in.s_addr = nvr_ip;
	dev_idx = GetDevIdx(EM_NVR, nvr_ip);
	if (dev_idx < 0)
	{
		ERR_PRINT("NVR IP(%s) GetDevIdx failed\n", inet_ntoa(in));
		
		return -EDEV_NOTFOUND;
	}
	
	pplock_dev[dev_idx]->Lock();

	pcdev = ppcdev[dev_idx];
	if (NULL == pcdev)
	{	
		ERR_PRINT("NVR IP(%s) pcdev == NULL\n", inet_ntoa(in));

		pplock_dev[dev_idx]->Unlock();
		return -EDEV_NOTFOUND;
	}

	if (nvr_chn >= pcdev->dev_info.maxChnNum)
	{
		ERR_PRINT("NVR IP(%s) nvr_chn(%d) >= pcdev->dev_info.maxChnNum(%d)\n", 
			inet_ntoa(in), nvr_chn, pcdev->dev_info.maxChnNum);

		pplock_dev[dev_idx]->Unlock();
		return -EPARAM;
	}

	pplock_dev[dev_idx]->Unlock();
	
	//check dec
	in.s_addr = dec_ip;
	dev_idx = GetDevIdx(dev_type, dec_ip);
	if (dev_idx < 0)
	{
		ERR_PRINT("IP(%s) GetDevIdx failed\n", inet_ntoa(in));
		
		return -EDEV_NOTFOUND;
	}
	
	pplock_dev[dev_idx]->Lock();

	pcdev = ppcdev[dev_idx];
	if (NULL == pcdev)
	{	
		ERR_PRINT("DEC IP(%s) pcdev == NULL\n", inet_ntoa(in));

		pplock_dev[dev_idx]->Unlock();
		return -EDEV_NOTFOUND;		
	}

	if (dec_chn >= pcdev->dev_info.maxChnNum)
	{
		ERR_PRINT("DEC IP(%s) dec_chn(%d) >= pcdev->dev_info.maxChnNum(%d)\n", 
			inet_ntoa(in), dec_chn, pcdev->dev_info.maxChnNum);

		pplock_dev[dev_idx]->Unlock();
		return -EPARAM;
	}

	ret = pcdev->SetChnIpc(dec_chn, nvr_ip, nvr_chn);
	if (ret)
	{
		ERR_PRINT("DEC IP(%s) SetChnIpc failed, ret: %d\n", inet_ntoa(in), ret);
		
		pplock_dev[dev_idx]->Unlock();		
		return ret;
	}	

	//success
	pplock_dev[dev_idx]->Unlock();
	return SUCCESS;
}

//ɾ��ͨ��IPC
int CBizDeviceManager::DelDevChnIpc(EM_DEV_TYPE dev_type, u32 dec_ip , u8 dec_chn)
{
	CBizDevice *pcdev = NULL;
	int dev_idx = -1;
	int ret = -FAILURE;
	struct in_addr in;
	
	//check dec
	in.s_addr = dec_ip;
	dev_idx = GetDevIdx(dev_type, dec_ip);
	if (dev_idx < 0)
	{
		ERR_PRINT("IP(%s) GetDevIdx failed\n", inet_ntoa(in));
		
		return -EDEV_NOTFOUND;
	}
	
	pplock_dev[dev_idx]->Lock();

	pcdev = ppcdev[dev_idx];
	if (NULL == pcdev)
	{	
		ERR_PRINT("DEC IP(%s) pcdev == NULL\n", inet_ntoa(in));

		pplock_dev[dev_idx]->Unlock();
		return -EDEV_NOTFOUND;		
	}

	if (dec_chn >= pcdev->dev_info.maxChnNum)
	{
		ERR_PRINT("DEC IP(%s) dec_chn(%d) >= pcdev->dev_info.maxChnNum(%d)\n", 
			inet_ntoa(in), dec_chn, pcdev->dev_info.maxChnNum);

		pplock_dev[dev_idx]->Unlock();
		return -EPARAM;
	}

	ret = pcdev->DelChnIpc(dec_chn);
	if (ret)
	{
		ERR_PRINT("DEC IP(%s) DelChnIpc failed, ret: %d\n", inet_ntoa(in), ret);
		
		pplock_dev[dev_idx]->Unlock();
		return ret;
	}	

	//success
	pplock_dev[dev_idx]->Unlock();
	return SUCCESS;
}

//NVR ¼������
int CBizDeviceManager::DevRecFilesSearch(u32 nvr_ip, ifly_recsearch_param_t *psearch_para, ifly_search_file_result_t *psearch_result)
{
	CBizDevice *pcdev = NULL;
	int dev_idx = -1;
	int ret = -FAILURE;
	struct in_addr in;
	
	//check dec
	in.s_addr = nvr_ip;
	dev_idx = GetDevIdx(EM_NVR, nvr_ip);
	if (dev_idx < 0)
	{
		ERR_PRINT("IP(%s) GetDevIdx failed\n", inet_ntoa(in));
		
		return -EDEV_NOTFOUND;
	}
	
	pplock_dev[dev_idx]->Lock();

	pcdev = ppcdev[dev_idx];
	if (NULL == pcdev)
	{	
		ERR_PRINT("DEC IP(%s) pcdev == NULL\n", inet_ntoa(in));

		pplock_dev[dev_idx]->Unlock();
		return -EDEV_NOTFOUND;		
	}

	ret = pcdev->RecFilesSearch(psearch_para, psearch_result);
	if (ret)
	{
		ERR_PRINT("DEC IP(%s) DelChnIpc failed, ret: %d\n", inet_ntoa(in), ret);
		
		pplock_dev[dev_idx]->Unlock();
		return ret;
	}	

	//success
	pplock_dev[dev_idx]->Unlock();
	return SUCCESS;
}

//��ȡ�豸��Ѳ����
int CBizDeviceManager::DevGetPatrolPara(EM_DEV_TYPE dev_type, u32 dec_ip, ifly_patrol_para_t *para, u32 *pbuf_size)
{
	CBizDevice *pcdev = NULL;
	int dev_idx = -1;
	int ret = -FAILURE;
	struct in_addr in;
	
	//check dec
	in.s_addr = dec_ip;
	dev_idx = GetDevIdx(dev_type, dec_ip);
	if (dev_idx < 0)
	{
		ERR_PRINT("IP(%s) GetDevIdx failed\n", inet_ntoa(in));
		
		return -EDEV_NOTFOUND;
	}
	
	pplock_dev[dev_idx]->Lock();

	pcdev = ppcdev[dev_idx];
	if (NULL == pcdev)
	{	
		ERR_PRINT("DEC IP(%s) pcdev == NULL\n", inet_ntoa(in));

		pplock_dev[dev_idx]->Unlock();
		return -EDEV_NOTFOUND;		
	}

	ret = pcdev->GetPatrolPara(para, pbuf_size);
	if (ret)
	{
		ERR_PRINT("DEC IP(%s) DelChnIpc failed, ret: %d\n", inet_ntoa(in), ret);
		
		pplock_dev[dev_idx]->Unlock();
		return ret;
	}	

	//success
	pplock_dev[dev_idx]->Unlock();
	return SUCCESS;
}






int CBizDeviceManager::StartNotifyDevInfo()//ʹ��֪ͨ���豸�㽫��Ϣ֪ͨ���ϲ�
{
	if (plock4param->Wrlock())
	{
		ERR_PRINT("Wrlock failed\n");
		return -FAILURE;
	}
	
	//b_notify_devinfo = TRUE;

	plock4param->Unlock();

	return SUCCESS;
}

int CBizDeviceManager::AddDev(EM_DEV_TYPE dev_type, u32 dev_ip)
{
	int ret = SUCCESS;
	u32 ip_le = ntohl(dev_ip);
	struct in_addr in;

	in.s_addr = dev_ip;
	
	if (!b_inited)
	{
		ERR_PRINT("module not init\n");
		return -FAILURE;
	}

	if (dev_type < EM_NVR || dev_type > EM_SWITCH_DEC)
    {
        ERR_PRINT("dev_type(%d) not support\n", dev_type);
		
		return -EPARAM;
    }
	
	if (plock4param->Wrlock())
	{
		ERR_PRINT("Wrlock Wrlock\n");
		return -FAILURE;
	}

	//���IP ��ͻ
	if (set_dev_ip.find(ip_le) != set_dev_ip.end())
	{
		ERR_PRINT("set IP(%s) conflict\n", inet_ntoa(in));
		
		ret = -EIP_CONFLICT;
		goto done;
	}

	//�豸��Ŀ����
	if (set_dev_ip.size() >= max_link_devs)
	{
		ERR_PRINT("devs too mang, cur(%d), limit(%d)\n", set_dev_ip.size(), max_link_devs);
		
		ret = -EDEVS_LIMIT;
		goto done;
	}

	ret = _AddDev2Map(dev_type, dev_ip);
	if (ret)
	{
		DBG_PRINT("_AddDev2Map IP(%s) failed\n", inet_ntoa(in));
		goto done;
	}

	switch (dev_type)
	{
		case EM_NVR:
			ret = BizConfigAddNvr(dev_ip);
			if (ret)
			{
				DBG_PRINT("BizConfigAddNvr IP(%s) failed\n", inet_ntoa(in));
			}
			break;
			
		case EM_PATROL_DEC:
			ret = BizConfigAddPatrolDec(dev_ip);
			if (ret)
			{
				DBG_PRINT("BizConfigAddPatrolDec IP(%s) failed\n", inet_ntoa(in));
			}
			break;
			
		case EM_SWITCH_DEC:
			ret = BizConfigAddSwitchDec(dev_ip);
			if (ret)
			{
				DBG_PRINT("BizConfigAddSwitchDec IP(%s) failed\n", inet_ntoa(in));
			}
			break;

		default:
			ERR_PRINT("dev_type%d exception\n", dev_type);
			ret = -EPARAM;
	}
	
done:
	
	plock4param->Unlock();
	return ret;
}

//��������ɾ����ֻ�Ƿ����ɾ���б�vdev_del_list
//������ɾ����threadKeepAlive �߳�
int CBizDeviceManager::DelDev(EM_DEV_TYPE dev_type, u32 dev_ip)
{
	MAP_IP_IDX *pmap = NULL;
	MAP_IP_IDX::iterator map_iter;
	u32 ip_le = ntohl(dev_ip);
	struct in_addr in;
	int ret = SUCCESS;
	std::string str_dev_type;

	in.s_addr = dev_ip;
	
	if (!b_inited)
	{
		ERR_PRINT("module not init\n");
		return -FAILURE;
	}

	if (dev_type < EM_NVR || dev_type > EM_SWITCH_DEC)
	{
		ERR_PRINT("dev ip: %s, dev_type(%d) not support\n", inet_ntoa(in), dev_type);
		return -EPARAM;
	}
	
	if (plock4param->Wrlock())
	{
		ERR_PRINT("Wrlock Wrlock\n");
		return -FAILURE;
	}

	pmap = &map_dev[dev_type-EM_NVR];
	str_dev_type = str_dev_list[dev_type-EM_NVR];

	//IP ����set
	if (set_dev_ip.find(ip_le) == set_dev_ip.end())
	{
		DBG_PRINT("set IP(%s) not found\n", inet_ntoa(in));
		ret = -EDEV_NOTFOUND;
		goto done;
	}

	if (set_dev_ip.erase(ip_le) != 1)
	{
		ERR_PRINT("set erase IP(%s) failed\n", inet_ntoa(in));
		ret = -FAILURE;
		goto fail;
	}   

	map_iter = pmap->find(ip_le);
	if (map_iter == pmap->end())
	{
		ERR_PRINT("%s map not found IP(%s) failed\n", str_dev_type.c_str(), inet_ntoa(in));
		ret = -EDEV_NOTFOUND;
		goto done;
	}

	//move dev to map_dev_del
	plock_list_del->Lock();
	list_dev_del.push_back(map_iter->second);
	plock_list_del->Unlock();
	
	pmap->erase(map_iter);
	
done:
	DBG_PRINT("%s map erase dev SUCCESS, IP(%s)\n", str_dev_type.c_str(), inet_ntoa(in));
	plock4param->Unlock();

	switch (dev_type)
	{
		case EM_NVR:
			ret = BizConfigDelNvr(dev_ip);
			if (ret)
			{
				DBG_PRINT("BizConfigDelNvr IP(%s) failed\n", inet_ntoa(in));
			}
			break;
			
		case EM_PATROL_DEC:
			ret = BizConfigDelPatrolDec(dev_ip);
			if (ret)
			{
				DBG_PRINT("BizConfigDelPatrolDec IP(%s) failed\n", inet_ntoa(in));
			}
			break;
			
		case EM_SWITCH_DEC:
			ret = BizConfigDelSwitchDec(dev_ip);
			if (ret)
			{
				DBG_PRINT("BizConfigDelSwitchDec IP(%s) failed\n", inet_ntoa(in));
			}
			break;

		default:
			ERR_PRINT("dev_type%d exception\n", dev_type);
	}
	
	return SUCCESS;

fail:
	plock4param->Unlock();
	return ret;
}

//�ɹ�����stearm_rcv[MaxMediaLinks] �±�stream_idx
int CBizDeviceManager::DevReqStreamStart(EM_DEV_TYPE dev_type, u32 dev_ip, u32 stream_id, ifly_TCP_Stream_Req *preq, s32 *psock_stream)
{
	CBizDevice *pcdev = NULL;
	int dev_idx = INVALID_VALUE;
	int ret = SUCCESS;
	struct in_addr in;
	in.s_addr = dev_ip;

	dev_idx = GetDevIdx(dev_type, dev_ip);
	if (dev_idx < 0)
	{
		ERR_PRINT("IP(%s) GetDevIdx failed\n", inet_ntoa(in));
		
		return -EDEV_NOTFOUND;
	}
	
	pplock_dev[dev_idx]->Lock();

	pcdev = ppcdev[dev_idx];
	if (NULL == pcdev)
	{	
		ERR_PRINT("IP(%s) pcdev == NULL\n", inet_ntoa(in));

		pplock_dev[dev_idx]->Unlock();
		return -EDEV_NOTFOUND;		
	}	

	ret = pcdev->ReqStreamStart(stream_id, preq, psock_stream);
	if (ret < 0)
	{
		ERR_PRINT("IP(%s) StreamStart failed: %d\n", inet_ntoa(in), ret);
		
		pplock_dev[dev_idx]->Unlock();
		return ret;
	}

	//success
	pplock_dev[dev_idx]->Unlock();
	return ret;
}

int CBizDeviceManager::DevReqStreamStopByStreamID(EM_DEV_TYPE dev_type, u32 dev_ip, u32 stream_id, s32 stop_reason)
{
	CBizDevice *pcdev = NULL;
	int dev_idx = INVALID_VALUE;
	int ret = SUCCESS;
	struct in_addr in;
	in.s_addr = dev_ip;

	dev_idx = GetDevIdx(dev_type, dev_ip);
	if (dev_idx < 0)
	{
		ERR_PRINT("IP(%s) GetDevIdx failed\n", inet_ntoa(in));
		
		return -EDEV_NOTFOUND;
	}
	
	pplock_dev[dev_idx]->Lock();

	pcdev = ppcdev[dev_idx];
	if (NULL == pcdev)
	{	
		ERR_PRINT("IP(%s) pcdev == NULL\n", inet_ntoa(in));

		pplock_dev[dev_idx]->Unlock();
		return -EDEV_NOTFOUND;		
	}

	ret = pcdev->ReqStreamStopByStreamID(stream_id, stop_reason);
	if (ret)
	{
		ERR_PRINT("IP(%s) StreamStop failed: %d\n", inet_ntoa(in), ret);
		
		pplock_dev[dev_idx]->Unlock();
		return ret;
	}
	
	pplock_dev[dev_idx]->Unlock();
	return SUCCESS;
}

int CBizDeviceManager::DevStreamProgress(EM_DEV_TYPE dev_type, u32 dev_ip, u32 stream_id, VD_BOOL b)//���ջطŽ�����Ϣ
{
	CBizDevice *pcdev = NULL;
	int dev_idx = INVALID_VALUE;
	int ret = SUCCESS;
	struct in_addr in;
	in.s_addr = dev_ip;

	dev_idx = GetDevIdx(dev_type, dev_ip);
	if (dev_idx < 0)
	{
		ERR_PRINT("IP(%s) GetDevIdx failed\n", inet_ntoa(in));
		
		return -EDEV_NOTFOUND;
	}
	
	pplock_dev[dev_idx]->Lock();

	pcdev = ppcdev[dev_idx];
	if (NULL == pcdev)
	{	
		ERR_PRINT("IP(%s) pcdev == NULL\n", inet_ntoa(in));

		pplock_dev[dev_idx]->Unlock();
		return -EDEV_NOTFOUND;		
	}

	ret = pcdev->StreamProgress(stream_id, b);
	if (ret)
	{
		ERR_PRINT("IP(%s) StreamProgress failed: %d\n", inet_ntoa(in), ret);
		
		pplock_dev[dev_idx]->Unlock();
		return ret;
	}
	
	pplock_dev[dev_idx]->Unlock();
	return SUCCESS;
}

int CBizDeviceManager::DevStreamPause(EM_DEV_TYPE dev_type, u32 dev_ip, u32 stream_id)
{
	CBizDevice *pcdev = NULL;
	int dev_idx = INVALID_VALUE;
	int ret = SUCCESS;
	struct in_addr in;
	in.s_addr = dev_ip;

	dev_idx = GetDevIdx(dev_type, dev_ip);
	if (dev_idx < 0)
	{
		ERR_PRINT("IP(%s) GetDevIdx failed\n", inet_ntoa(in));
		
		return -EDEV_NOTFOUND;
	}
	
	pplock_dev[dev_idx]->Lock();

	pcdev = ppcdev[dev_idx];
	if (NULL == pcdev)
	{	
		ERR_PRINT("IP(%s) pcdev == NULL\n", inet_ntoa(in));

		pplock_dev[dev_idx]->Unlock();
		return -EDEV_NOTFOUND;		
	}

	ret = pcdev->StreamPause(stream_id);
	if (ret)
	{
		ERR_PRINT("IP(%s) StreamPause failed: %d\n", inet_ntoa(in), ret);
		
		pplock_dev[dev_idx]->Unlock();
		return ret;
	}
	
	pplock_dev[dev_idx]->Unlock();
	return SUCCESS;
}

int CBizDeviceManager::DevStreamResume(EM_DEV_TYPE dev_type, u32 dev_ip, u32 stream_id)
{
	CBizDevice *pcdev = NULL;
	int dev_idx = INVALID_VALUE;
	int ret = SUCCESS;
	struct in_addr in;
	in.s_addr = dev_ip;

	dev_idx = GetDevIdx(dev_type, dev_ip);
	if (dev_idx < 0)
	{
		ERR_PRINT("IP(%s) GetDevIdx failed\n", inet_ntoa(in));
		
		return -EDEV_NOTFOUND;
	}
	
	pplock_dev[dev_idx]->Lock();

	pcdev = ppcdev[dev_idx];
	if (NULL == pcdev)
	{	
		ERR_PRINT("IP(%s) pcdev == NULL\n", inet_ntoa(in));

		pplock_dev[dev_idx]->Unlock();
		return -EDEV_NOTFOUND;		
	}

	ret = pcdev->StreamResume(stream_id);
	if (ret)
	{
		ERR_PRINT("IP(%s) StreamResume failed: %d\n", inet_ntoa(in), ret);
		
		pplock_dev[dev_idx]->Unlock();
		return ret;
	}
	
	pplock_dev[dev_idx]->Unlock();
	return SUCCESS;
}

int CBizDeviceManager::DevStreamStep(EM_DEV_TYPE dev_type, u32 dev_ip, u32 stream_id)//֡��
{
	CBizDevice *pcdev = NULL;
	int dev_idx = INVALID_VALUE;
	int ret = SUCCESS;
	struct in_addr in;
	in.s_addr = dev_ip;

	dev_idx = GetDevIdx(dev_type, dev_ip);
	if (dev_idx < 0)
	{
		ERR_PRINT("IP(%s) GetDevIdx failed\n", inet_ntoa(in));
		
		return -EDEV_NOTFOUND;
	}
	
	pplock_dev[dev_idx]->Lock();

	pcdev = ppcdev[dev_idx];
	if (NULL == pcdev)
	{	
		ERR_PRINT("IP(%s) pcdev == NULL\n", inet_ntoa(in));

		pplock_dev[dev_idx]->Unlock();
		return -EDEV_NOTFOUND;		
	}

	ret = pcdev->StreamStep(stream_id);
	if (ret)
	{
		ERR_PRINT("IP(%s) StreamStep failed: %d\n", inet_ntoa(in), ret);
		
		pplock_dev[dev_idx]->Unlock();
		return ret;
	}
	
	pplock_dev[dev_idx]->Unlock();
	return SUCCESS;
}

int CBizDeviceManager::DevStreamRate(EM_DEV_TYPE dev_type, u32 dev_ip, u32 stream_id, s32 rate)//{-8, -4, -2, 1, 2, 4, 8}
{
	CBizDevice *pcdev = NULL;
	int dev_idx = INVALID_VALUE;
	int ret = SUCCESS;
	struct in_addr in;
	in.s_addr = dev_ip;

	dev_idx = GetDevIdx(dev_type, dev_ip);
	if (dev_idx < 0)
	{
		ERR_PRINT("IP(%s) GetDevIdx failed\n", inet_ntoa(in));
		
		return -EDEV_NOTFOUND;
	}
	
	pplock_dev[dev_idx]->Lock();

	pcdev = ppcdev[dev_idx];
	if (NULL == pcdev)
	{	
		ERR_PRINT("IP(%s) pcdev == NULL\n", inet_ntoa(in));

		pplock_dev[dev_idx]->Unlock();
		return -EDEV_NOTFOUND;		
	}

	ret = pcdev->StreamRate(stream_id, rate);
	if (ret)
	{
		ERR_PRINT("IP(%s) StreamRate failed: %d\n", inet_ntoa(in), ret);
		
		pplock_dev[dev_idx]->Unlock();
		return ret;
	}
	
	pplock_dev[dev_idx]->Unlock();
	return SUCCESS;
}

int CBizDeviceManager::DevStreamSeek(EM_DEV_TYPE dev_type, u32 dev_ip, u32 stream_id, u32 time)
{
	CBizDevice *pcdev = NULL;
	int dev_idx = INVALID_VALUE;
	int ret = SUCCESS;
	struct in_addr in;
	in.s_addr = dev_ip;

	dev_idx = GetDevIdx(dev_type, dev_ip);
	if (dev_idx < 0)
	{
		ERR_PRINT("IP(%s) GetDevIdx failed\n", inet_ntoa(in));
		
		return -EDEV_NOTFOUND;
	}
	
	pplock_dev[dev_idx]->Lock();

	pcdev = ppcdev[dev_idx];
	if (NULL == pcdev)
	{	
		ERR_PRINT("IP(%s) pcdev == NULL\n", inet_ntoa(in));

		pplock_dev[dev_idx]->Unlock();
		return -EDEV_NOTFOUND;		
	}

	ret = pcdev->StreamSeek(stream_id, time);
	if (ret)
	{
		ERR_PRINT("IP(%s) StreamSeek failed: %d\n", inet_ntoa(in), ret);
		
		pplock_dev[dev_idx]->Unlock();
		return ret;
	}
	
	pplock_dev[dev_idx]->Unlock();
	return SUCCESS;
}





//�ײ����ݽ���
int CBizDeviceManager::NetCom(int sock, u16 event, const void *content, int length, void* ackbuf, int ackbuflen, int *realacklen)
{
	//u8 abyBuf[1024];
	int ret = SUCCESS;
	int msglen = 0;
	int rcvlen = 0;
	u16 number = 0;
	ifly_cp_header_t cp_head_snd;
	ifly_cp_header_t cp_head_rcv;
	
	if(sock < 0)
	{
		DBG_PRINT("sock invalid\n");
		return -EPARAM;
	}
	
	if(length < 0)
	{
		DBG_PRINT("length: %d invalid\n", length);
		return -EPARAM;
	}
	
	if(NULL == realacklen)
	{
		DBG_PRINT("realacklen invalid\n");
		return -EPARAM;
	}
	
	//ȫ�ֻػ���
	plock4sock->Lock();

	//����
	msglen = sizeof(cp_head_snd)+length;
	
	memset(&cp_head_snd, 0, sizeof(cp_head_snd));
	cp_head_snd.length = htonl(msglen);
	cp_head_snd.type = htons(CTRL_COMMAND);
	cp_head_snd.event = htons(event);
	
	number = ++sync_num;//_IncAndGetSyncNum();
	//printf("%s snd number1: %d\n", , number);
	
	cp_head_snd.number = htons(number);
	cp_head_snd.version = htons(CTRL_VERSION);
	if(msglen > SendBuf)
	{
		ERR_PRINT("event: %d, cp_head_snd.length(%d) > SendBuf, failed\n", event, msglen);
		
		plock4sock->Unlock();
		return -EPARAM;
	}
	
	memcpy(send_buf, &cp_head_snd, sizeof(cp_head_snd));

	if (content && length)
	{
		memcpy(send_buf+sizeof(cp_head_snd), content, length);
	}

	sync_sem.TryPend();//��ֹ��һ����Ϣ��Ӧ��

	ret = loopsend(sock, (char *)send_buf, msglen);
	if(ret < 0)
	{
		ERR_PRINT("event: %d, loopsend failed\n", event);

		plock4sock->Unlock();
		return -ESEND;
	}
	//printf("snd number: %d, event: %d\n", number, event);
	
	//�ȴ����շ���
	ret = sync_sem.TimedPend(DIALOGUE_TIMEOUT);
	if (ret)//timeout or failed
	{
		ERR_PRINT("event: %d, sem_timedwait failed, error:[%d,%s], failed!\n", 
					event, errno, strerror(errno));
		
		plock4sock->Unlock();

		//��ӡ������Ϣ
		if (isSockIOErr(sock))//����ǶԶ˹رգ�socker error(32, Broken pipe)
		{
			return -ERECV;
		}
		
		return -ETIMEOUT;
	}

	if (sync_ack_len < (int)sizeof(ifly_cp_header_t))
	{
		ERR_PRINT("event: %d, sync_ack_len < sizeof(ifly_cp_header_t), failed\n", event);
		
		plock4sock->Unlock();
		return -FAILURE;
	}

	memcpy(&cp_head_rcv, sync_buf, sizeof(ifly_cp_header_t));
	
	if (CTRL_SUCCESS != cp_head_rcv.event)
	{
		if (CTRL_FAILED_NETRCV == cp_head_rcv.event)//���޸ĵĽ��ճ���
		{
			plock4sock->Unlock();
			return -ERECV;
		}
		
		ERR_PRINT("rcv event: %d, failed\n", cp_head_rcv.event);
		
		plock4sock->Unlock();
		return -FAILURE;
	}

	if (NULL == ackbuf)//������ܲ���Ҫ������Ϣ
	{
	
		plock4sock->Unlock();
		return SUCCESS;
	}

	rcvlen = sync_ack_len - sizeof(ifly_cp_header_t);
	
	if(NULL != realacklen) 
	{
		*realacklen = rcvlen;
	}
	
	if(rcvlen > 0)
	{		
		if(rcvlen > ackbuflen)
		{
			ERR_PRINT("event: %d, ackbuflen(%d) < rcvlen(%d), failed\n", event, ackbuflen, rcvlen);
			
			plock4sock->Unlock();
			return -ESPACE;
		}
		
		memcpy(ackbuf, sync_buf+sizeof(ifly_cp_header_t), rcvlen);
	}

	plock4sock->Unlock();
	return SUCCESS;
}


//�ⲿ����
int CBizDeviceManager::_AddDev2Map(EM_DEV_TYPE dev_type, u32 dev_ip)
{
	MAP_IP_IDX *pmap = NULL;
	CBizDevice *pcdev = NULL;
	std::string str_dev_type;
	int ret = -FAILURE;
	u32 ip_le = ntohl(dev_ip);	//ntohl(ip) С�˸�ʽ��map ������	
	s32 i = 0;
	u32 tmp_idx = 0;
	u32 pre_idx = 0;
	struct in_addr in;
	
	in.s_addr = dev_ip;

	if (dev_type < EM_NVR || dev_type > EM_SWITCH_DEC)
    {
        ERR_PRINT("dev ip: %s, dev_type(%d) not support\n", inet_ntoa(in), dev_type);
		
		return -EPARAM;
    }
	pmap = &map_dev[dev_type-EM_NVR];
	str_dev_type = str_dev_list[dev_type-EM_NVR];
	
	//���IP ��ͻ
	if (set_dev_ip.find(ip_le) != set_dev_ip.end())
	{
		ERR_PRINT("dev ip: %s, set_dev_ip conflict\n", inet_ntoa(in));
		return SUCCESS;
	}

	//init CBizDevice
	pcdev = NULL;
	pcdev = new CBizDevice;
	if (NULL == pcdev)
	{
		ERR_PRINT("dev ip: %s, type: %s new CBizDevice failed\n", inet_ntoa(in), str_dev_type.c_str());
		
		return -FAILURE;
	}

	if (pcdev->Init())
	{
		ERR_PRINT("dev ip: %s, type: %s CBizDevice init failed\n", inet_ntoa(in), str_dev_type.c_str());
		
		ret = -FAILURE;
		goto fail;
	}

	pcdev->dev_info.devicetype = dev_type;
	pcdev->dev_info.deviceIP = dev_ip;
	pcdev->dev_info.devicePort = CTRL_PROTOCOL_SERVERPORT;    
	
	//�����豸��
	plock_dev_pool->Lock();
	pre_idx = pool_cur_idx;
	
	//�ӵ�ǰλ�ÿ�ʼ�ҵ���һ��Ϊʹ�õ�����
	for (i = pool_cur_idx+1;
		tmp_idx = i & (pool_size-1), tmp_idx != pool_cur_idx;
		++i)
	{
		if (!pool_bits[tmp_idx])//false ��ǰλ��δʹ��
		{
			break;
		}
	}

	if (tmp_idx == pool_cur_idx)
	{
		ERR_PRINT("dev ip: %s, type: %s dev pool full, pool_dev_count: %d\n", inet_ntoa(in), str_dev_type.c_str(), pool_dev_count);
		
		plock_dev_pool->Lock();
		ret = -FAILURE;
		goto fail;
	}

	pcdev->dev_idx = tmp_idx;//�豸���豸���е�����

	pplock_dev[tmp_idx]->Lock();

	if (ppcdev[tmp_idx])
	{
		ERR_PRINT("ppcdev[%d] != NULL\n", tmp_idx);
	}
	ppcdev[tmp_idx] = pcdev;
	
	pplock_dev[tmp_idx]->Unlock();
	
	pool_bits[tmp_idx] = 1;
	pool_cur_idx = tmp_idx;
	++pool_dev_count;
	
	plock_dev_pool->Unlock();
	
	
	//insert map
	if (!pmap->insert(std::make_pair(ip_le, tmp_idx)).second)
	{
		ERR_PRINT("dev ip: %s, type: %s map insert failed, IP conflict\n", inet_ntoa(in), str_dev_type.c_str());
		ret = -EIP_CONFLICT;
		goto fail2;
	}

	//insert set
	if (!set_dev_ip.insert(ip_le).second)
	{
		ERR_PRINT("dev ip: %s, type: %s set insert failed, IP conflict\n", inet_ntoa(in), str_dev_type.c_str());
		pmap->erase(ip_le);
		
		ret = -EIP_CONFLICT;
		goto fail2;
	}

	DBG_PRINT("svr(%s) add success, dev_idx: %d, pool_dev_count: %d\n", inet_ntoa(in), tmp_idx, pool_dev_count);

	return SUCCESS;
	
fail2:
	plock_dev_pool->Lock();
	
	pplock_dev[tmp_idx]->Lock();
	ppcdev[tmp_idx] = NULL;
	pplock_dev[tmp_idx]->Unlock();
	
	pool_bits[tmp_idx] = 0;
	pool_cur_idx = pre_idx;
	--pool_dev_count;
	
	plock_dev_pool->Unlock();
	
fail:
	
	delete pcdev;
	pcdev = NULL;
	
	return ret;
}

/************************************************************
�����ӵ��豸
1-���Ƿ����߷�
2-1-�����豸--> reconnect
2-2-�����豸��������������ٷ�
2-2-1-�޴�--> keepalive
2-2-2-�д�--> ���������һ�μ��ʱΪ�����豸
************************************************************/
void CBizDeviceManager::_SplitDevFromMap(EM_DEV_TYPE dev_type, 
							std::list<s32> &list_devs_online, 
							std::list<s32> &list_devs_offline)
{
	MAP_IP_IDX *pmap = NULL;
	MAP_IP_IDX::iterator map_iter;
	CBizDevice *pcdev = NULL;
	std::string str_dev_type;
	s32 dev_idx;
	
	struct in_addr in;
	
	if (dev_type <= EM_DEV_TYPE_NONE
            || dev_type >= EM_DEV_TYPE_MAX)
    {
        ERR_PRINT("dev_type(%d) not support\n", dev_type);
				
        return;
    }
    pmap = &map_dev[dev_type-EM_NVR];
	str_dev_type = str_dev_list[dev_type-EM_NVR];
	
	plock4param->Rdlock();
	
	for (map_iter = pmap->begin();
		map_iter != pmap->end();
		++map_iter)
	{
		in.s_addr = htonl(map_iter->first);
		dev_idx = map_iter->second;

		if (dev_idx < 0 || dev_idx >= pool_size)
		{
			ERR_PRINT("%s dev(%s) idx(%d) out of range, pool_size: %d\n", 
				str_dev_type.c_str(), inet_ntoa(in), dev_idx, pool_size);
			
			continue;
		}		
		
		pplock_dev[dev_idx]->Lock();

		pcdev = ppcdev[dev_idx];
		if (pcdev)
		{	
			if (pcdev->b_alive)
			{
				//DBG_PRINT("device online, IP(%s)\n", inet_ntoa(in));
				if (pcdev->err_cnt > NETDIALOGUE_ERR_MAX)//����
				{
					//�Ͽ�������
					pcdev->ShutdownStreamAll();
					
					//ע��logout
					pcdev->DevDisconnect();
					
					//�ر�cmd socket
					pcdev->CleanSock();	
				}
				else
				{
					list_devs_online.push_back(dev_idx);
				}
			}
			else
			{
				//DBG_PRINT("device offline, IP(%s)\n", inet_ntoa(in));
				list_devs_offline.push_back(dev_idx);
			}
		}
		else
		{
			ERR_PRINT("IP(%s) pdev == NULL\n", inet_ntoa(in));
		}

		pplock_dev[dev_idx]->Unlock();		
	}
	
	plock4param->Unlock();
}

int CBizDeviceManager::_UnlinkAllDevs()
{
	return SUCCESS;
}

//add and del  map_fd_idx ��Ҫ����������Ϣ�ı�
int CBizDeviceManager::AddMapRcv(s32 sock_fd, s32 dev_idx)//��pplock_dev[dev_idx]->Lock(); ���ⲿ
{
	if (INVALID_SOCKET == sock_fd)
	{
		ERR_PRINT("INVALID_SOCKET == sock_fd\n");
		return SUCCESS;
	}
	
	plock_map_fd_idx->Lock();
	
	if (!map_fd_idx.insert(std::make_pair(sock_fd, dev_idx)).second)
	{
		ERR_PRINT("map_fd_idx insert failed, fd_tmp(%d) conflict\n", sock_fd);
		plock_map_fd_idx->Unlock();
		
		return -FAILURE;
	}
	
	plock_map_fd_idx->Unlock();
	return SUCCESS;
}

int CBizDeviceManager::DelMapRcv(s32 sock_fd)//��pplock_dev[dev_idx]->Lock(); ���ⲿ
{
	MAP_FD_IDX::iterator map_iter;

	if (INVALID_SOCKET == sock_fd)
	{
		ERR_PRINT("INVALID_SOCKET == sock_fd\n");
		return SUCCESS;
	}
	
	plock_map_fd_idx->Lock();
	
	map_iter = map_fd_idx.find(sock_fd);
	if (map_iter == map_fd_idx.end())
	{
		DBG_PRINT("sock fd(%d) not found in map_fd_idx\n", sock_fd);
		
		plock_map_fd_idx->Unlock();		
		return -FAILURE;
	}

	map_fd_idx.erase(map_iter);
	
	plock_map_fd_idx->Unlock();
	return SUCCESS;
}

//��ѯdev: sock_cmd�Ƿ���map_fd_idx
//�������˵����thread_Rcv ���Ѿ��������մ���
VD_BOOL CBizDeviceManager::isInMapRcv(s32 sock_fd)
{
	MAP_FD_IDX::iterator map_iter;

	if (INVALID_SOCKET == sock_fd)
	{
		ERR_PRINT("INVALID_SOCKET == sock_fd\n");
		return FALSE;
	}
	
	plock_map_fd_idx->Lock();
	
	map_iter = map_fd_idx.find(sock_fd);
	if (map_iter == map_fd_idx.end())
	{
		plock_map_fd_idx->Unlock();
		
		return FALSE;
	}
	
	plock_map_fd_idx->Unlock();
	return TRUE;
}


// 1 ���շ�������Ӧ
// 2 ���շ���������
// 3 ����ɾ���豸
void CBizDeviceManager::threadRcv(uint param)
{
	std::list<s32> list_fd;
	std::list<s32>::iterator list_iter;
	MAP_FD_IDX::iterator map_iter;
	//CBizDevice *pcdev = NULL;
	s32 dev_idx;
	//u32 deviceIP;
	
	fd_set rset;
	int fd_max = INVALID_SOCKET;
	int fd_tmp = INVALID_SOCKET;
	struct timeval timeout;
	//struct in_addr in;

	char *rcv_buf = NULL;
	int ret = -FAILURE;
	int msglen = 0;
	ifly_cp_header_t cprcvhead;
	
	DBG_PRINT("CBizDeviceManager::threadRcv running\n");

	rcv_buf = new char[RcvBuf];
	if (NULL == rcv_buf)
	{
		ERR_PRINT("new recv buffer failed\n");
		goto fail;
	}
	
	while(1)
	{
		list_fd.clear();		
		FD_ZERO(&rset);
		fd_max = INVALID_SOCKET;
		timeout.tv_sec = 1;
		timeout.tv_usec = 0;

		//DBG_PRINT("while 1\n");
		plock_map_fd_idx->Lock();
		
		for (map_iter = map_fd_idx.begin();
			map_iter != map_fd_idx.end();
			++map_iter)
		{
			fd_tmp = map_iter->first;

			if (INVALID_SOCKET != fd_tmp)
			{
				FD_SET(fd_tmp, &rset);
				fd_max = MAX(fd_tmp, fd_max);
				list_fd.push_back(fd_tmp);
			}
		}
				
		plock_map_fd_idx->Unlock();
		
		//DBG_PRINT("fd_max: %d\n", fd_max);
		if (list_fd.empty())
		{
			sleep(1);
			continue;
		}

		ret = select(fd_max+1, &rset, NULL, NULL, &timeout);
		if (ret < 0)
		{
			ERR_PRINT("select failed\n");
			sleep(1);
			continue;
		}
		else if (ret == 0)//timeout
		{
			continue;
		}

		for (list_iter = list_fd.begin();
			list_iter != list_fd.end();
			++list_iter)
		{
			fd_tmp = *list_iter;			
			//DBG_PRINT("for 1\n");
			if ((INVALID_SOCKET != fd_tmp)
				&& FD_ISSET(fd_tmp, &rset))
			{
				ret = looprecv(fd_tmp, rcv_buf, sizeof(ifly_cp_header_t));
				if (ret != SUCCESS)//���ճ���
				{
					ERR_PRINT("recv ifly_cp_header_t failed\n");

					//��ӡ������Ϣ
					isSockIOErr(fd_tmp);
					//�Ƴ�����map_fd_idx
					DelMapRcv(fd_tmp);			
					
					continue;
				}
				//DBG_PRINT("for 3\n");
				memcpy(&cprcvhead, rcv_buf, sizeof(ifly_cp_header_t));
				cprcvhead.length	= ntohl(cprcvhead.length);
				cprcvhead.type		= ntohs(cprcvhead.type);
				cprcvhead.version	= ntohs(cprcvhead.version);
				cprcvhead.number	= ntohs(cprcvhead.number);
				cprcvhead.event		= ntohs(cprcvhead.event);

				memcpy(rcv_buf, &cprcvhead, sizeof(ifly_cp_header_t));//ת����ɺ�д�أ�NetCom�л��õ�ifly_cp_header_t�ṹ
#if 0
				printf("recv msg: \n");
				printf("\t number: %d\n", cprcvhead.number);
				printf("\t type: %d\n", cprcvhead.type);
				printf("\t event: %d\n", cprcvhead.event);
				printf("\t length: %d\n", cprcvhead.length);
#endif					
				if(cprcvhead.length < sizeof(ifly_cp_header_t) || cprcvhead.type > CTRL_ACK || cprcvhead.version != CTRL_VERSION)
				{
					DBG_PRINT("sock_cli ifly_cp_header_t param error, len: %d, type: %d, version: %d\n", 
						cprcvhead.length, cprcvhead.type, cprcvhead.version);					
					
					continue;
				}

				//����Ϣ��Ŀǰֻ���շ�������Ӧ
				if(cprcvhead.length > sizeof(ifly_cp_header_t))
				{
					//�������ݳ��ȳ������ջ�����
					if (cprcvhead.length > RcvBuf)
					{
						ERR_PRINT("recv msg too long(%d) OUTOFMEMORY\n", cprcvhead.length);

						int data_size = cprcvhead.length > sizeof(ifly_cp_header_t);
						int rcv_len = RcvBuf - (int)sizeof(ifly_cp_header_t);
						while (data_size > 0)
						{
							ret = looprecv(fd_tmp, rcv_buf+sizeof(ifly_cp_header_t), rcv_len);
							if (ret != SUCCESS)
							{
								ERR_PRINT("OUTOFMEMORY recv data failed\n");
								
								break;
							}
							
							data_size -= rcv_len;
							rcv_len = data_size>(RcvBuf-(int)sizeof(ifly_cp_header_t)) ? (RcvBuf-(int)sizeof(ifly_cp_header_t)) : data_size;					
						}

						if (data_size == 0)//�������
						{
							DBG_PRINT("OUTOFMEMORY looprecv finish!\n");
						}
						else	//���ճ���
						{
							//��ӡ������Ϣ
							isSockIOErr(fd_tmp);
							//�Ƴ�����map_fd_idx
							DelMapRcv(fd_tmp);

							//����ǻ�Ӧ��Ϣ���޸�Ϊ���ճ���󷵻�
							if (cprcvhead.type == CTRL_ACK)
							{
								if(cprcvhead.number == sync_num)//_GetSyncNum())
								{
									cprcvhead.event		= CTRL_FAILED_NETRCV;
									memcpy(sync_buf, &cprcvhead, sizeof(ifly_cp_header_t));//ת����ɺ�д�أ�NetCom�л��õ�ifly_cp_header_t�ṹ
									sync_ack_len = sizeof(ifly_cp_header_t);
									sync_sem.Post();
								}
							}
						}
						
						continue;
					}

					//���ݳ���OK
					msglen = cprcvhead.length - sizeof(ifly_cp_header_t);
					ret = looprecv(fd_tmp, rcv_buf+sizeof(ifly_cp_header_t), msglen);
					if (ret != SUCCESS)
					{
						ERR_PRINT("recv data failed\n");

						//��ӡ������Ϣ
						isSockIOErr(fd_tmp);
						//�Ƴ�����map_fd_idx
						DelMapRcv(fd_tmp);

						//����ǻ�Ӧ��Ϣ���޸�Ϊ���ճ���󷵻�
						if (cprcvhead.type == CTRL_ACK)
						{
							if(cprcvhead.number == sync_num)//_GetSyncNum())
							{
								cprcvhead.event		= CTRL_FAILED_NETRCV;
								memcpy(sync_buf, &cprcvhead, sizeof(ifly_cp_header_t));//ת����ɺ�д�أ�NetCom�л��õ�ifly_cp_header_t�ṹ
								sync_ack_len = sizeof(ifly_cp_header_t);
								sync_sem.Post();
							}					
						}
						
						continue;
					}
				}

				//���ݽ������
				if (cprcvhead.type == CTRL_ACK)//�յ���������Ӧ��Ϣ
				{
					if(cprcvhead.number == sync_num)//_GetSyncNum())
					{
						memcpy(sync_buf, rcv_buf, cprcvhead.length);
						sync_ack_len = cprcvhead.length;

						sync_sem.Post();
					}					
				}
				else if (cprcvhead.type == CTRL_NOTIFY)//������֪ͨ��Ϣ(������������ȵ�)
				{
					//DBG_PRINT("recv notify, event: %d\n", cprcvhead.event);
					//from fd >>> dev index >>> IP
					plock_map_fd_idx->Lock();
						
					map_iter = map_fd_idx.find(fd_tmp);
					if (map_iter == map_fd_idx.end())
					{
						plock_map_fd_idx->Unlock();
						continue;
					}
				
					dev_idx = map_iter->second;
					plock_map_fd_idx->Unlock();

					if ((dev_idx < 0) || (dev_idx >= pool_size))
					{
						ERR_PRINT("dev_idx(%d) out of range, pool_size: %d\n", dev_idx, pool_size);

						continue;
					}
				#if 1
					dealDevNotify(dev_idx, 
							cprcvhead.event,
							rcv_buf + sizeof(ifly_cp_header_t), 
							cprcvhead.length - sizeof(ifly_cp_header_t));
				#endif
				}
				else
				{
					ERR_PRINT("svr msg not support, msg type: %d, event: %d\n", cprcvhead.type, cprcvhead.event);
				}
			}
		}
	}

fail:
	ERR_PRINT("CBizDeviceManager::threadRcv exit, inconceivable\n");

	if (rcv_buf)
	{
		delete []rcv_buf;
		rcv_buf = NULL;
	}
}

void CBizDeviceManager::dealDevNotify(s32 dev_idx, u16 event, s8 *pbyMsgBuf, int msgLen) //�����豸֪ͨ��Ϣ
{
	//DBG_PRINT("event: %d\n", event);
	
	switch (event)
	{
		case CTRL_NOTIFY_PLAYPROGRESS: //�طŽ���֪ͨ
		{			
			ifly_progress_t progress;
			memcpy(&progress, pbyMsgBuf, sizeof(ifly_progress_t));

			progress.id = ntohl(progress.id);				//�ط�linkid
			progress.currPos = ntohl(progress.currPos);		//��ǰ����
			progress.totallen = ntohl(progress.totallen);	//��ʱ��
					
#if 1	
			//send msg to stream manager	
			SBizMsg_t msg;
			memset(&msg, 0, sizeof(SBizMsg_t));
			msg.msg_type = EM_DEV_MSG_PROGRESS;
			msg.dev_idx = dev_idx;
			msg.un_part_chn.link_id = progress.id;
			msg.un_part_data.stream_progress.cur_pos = progress.currPos;
			msg.un_part_data.stream_progress.total_size = progress.totallen;

			//DBG_PRINT("msg_type: %d, link_id(%u)\n", msg.msg_type, progress.id);
			
			int ret = WriteMsg(&msg, sizeof(SBizMsg_t));
			if (ret)
			{
				ERR_PRINT("link_id(%u), msg_type: %d, WriteMsg failed, ret: %d\n",
					progress.id, msg.msg_type, ret);
		 	}
#endif			
		} break;

		case CTRL_NOTIFY_PLAYEND: //����������ļ��ط�(��)��ʱ��ط�(��)
		{
			u32 link_id = 0;
			
			memcpy(&link_id, pbyMsgBuf, sizeof(link_id));
			link_id = ntohl(link_id);
			
			//send msg to stream manager
			SBizMsg_t msg;
			memset(&msg, 0, sizeof(SBizMsg_t));
			msg.msg_type = EM_DEV_MSG_FINISH;
			msg.dev_idx = dev_idx;
			msg.un_part_chn.link_id = link_id;

			int ret = WriteMsg(&msg, sizeof(SBizMsg_t));
			if (ret)
			{
				ERR_PRINT("link_id(%u), msg_type: %d, WriteMsg failed, ret: %d\n",
					link_id, msg.msg_type, ret);
		 	}
		} break;

		case CTRL_NOTIFY_ALARMINFO: //�첽������Ϣ
		{
		} break;

		default:
			DBG_PRINT("notify event(%d) not support\n", event);
	}

#if 0
	BizDealSvrNotify(dev_ip, 
			cprcvhead.event,
			rcv_buf + sizeof(ifly_cp_header_t), 
			cprcvhead.length - sizeof(ifly_cp_header_t));																			
#endif
}

//����
void CBizDeviceManager::timerFuncReconnect(uint param)
{
	std::list<u32> *plist_devs_offline = (std::list<u32> *)param;
	std::list<u32>::iterator list_iter;
	MAP_FD_IDX map_fd_dev;
	MAP_FD_IDX::iterator map_iter;	
	CBizDevice *pcdev = NULL;
	s32 dev_idx;
	
	struct sockaddr_in svr_addr;
	struct in_addr in;
	int fd_tmp = INVALID_SOCKET;
	int ret = 0;

	fd_set wset, set_raw;
	int fd_max = INVALID_SOCKET;
	struct timeval timeout;

	u64 pre_tick, cur_tick;

	FD_ZERO(&wset);
	FD_ZERO(&set_raw);

	DBG_PRINT("CBizDeviceManager::timerFuncReconnect running\n");
	
	//�������豸����
	for (list_iter = plist_devs_offline->begin();
		list_iter != plist_devs_offline->end();
		++list_iter)
	{
		dev_idx = *list_iter;
		if ((dev_idx < 0) || (dev_idx >= pool_size))
		{
			ERR_PRINT("dev_idx(%d) out of range, pool_size: %d\n", dev_idx, pool_size);

			continue;
		}
	
		pplock_dev[dev_idx]->Lock();
		
		pcdev = ppcdev[dev_idx];
		if (NULL == pcdev)
		{
			pplock_dev[dev_idx]->Unlock();
			ERR_PRINT("dev_idx(%d) pcdev == NULL\n", dev_idx);

			continue;
		}

		in.s_addr = pcdev->dev_info.deviceIP;
		if (pcdev->b_alive)
		{
			DBG_PRINT("svr IP(%s) is alive, inconceivable\n", inet_ntoa(in));
			pplock_dev[dev_idx]->Unlock();
			
			continue;
		}		
		
		memset(&svr_addr, 0, sizeof(svr_addr));
		svr_addr.sin_family = AF_INET;
		svr_addr.sin_addr.s_addr = pcdev->dev_info.deviceIP;
		svr_addr.sin_port = htons(pcdev->dev_info.devicePort);		
		
		pplock_dev[dev_idx]->Unlock();

		fd_tmp = socket(AF_INET, SOCK_STREAM, 0);
		if (fd_tmp < 0)
		{
			ERR_PRINT("svr IP(%s) create socket failed, %s\n", inet_ntoa(in), strerror(errno));
			continue;
		}

		//�����׽ӿڷ����� 
		//�������������connect ���� select ͳһ����
		ret = fdSetNonblockFlag(fd_tmp);
		if (ret)
		{
			ERR_PRINT("svr IP(%s) fdSetNonblockFlag\n", inet_ntoa(in));
			close(fd_tmp);
			continue;
		}
		
		ret = connect(fd_tmp, (struct sockaddr *)&svr_addr, sizeof(struct sockaddr_in));
	#if 0	
		if (ret == 0)//�ɹ�
		{
			//ȥ��O_NONBLOCK
			if (fdClrNonblockFlag(fd_tmp))
			{
				DBG_PRINT("svr IP(%s) fdClrNonblockFlag\n", inet_ntoa(in));
				close(fd_tmp);
				continue;
			}

			if (SetSockLinkOpt(fd_tmp))
			{				
				ERR_PRINT("svr IP(%s) SetSockLinkOpt failed, %s\n", inet_ntoa(in), strerror(errno));
				close(fd_tmp);
				continue;
			}
		/*
			if (SetSockIOTimeout(fd_tmp, CTRL_SOCKET_IO_TIMEOUT))
			{
				ERR_PRINT("svr IP(%s) SetSockIOTimeout failed, %s\n", inet_ntoa(in), strerror(errno));
				close(fd_tmp);
				continue;
			}		
		*/
			//�������map_fd_idx
			if (AddMapRcv(fd_tmp, dev_idx))
			{
				ERR_PRINT("AddMapRcv failed, fd_tmp(%d) conflict\n", fd_tmp);
				close(fd_tmp);
				continue;
			}			

			pplock_dev[dev_idx]->Lock();
			pcdev->sock_cmd = fd_tmp;

			if (pcdev->DevConnect())//���ӷ�����ʧ��
			{
				pcdev->CleanSock();//close fd
				pplock_dev[dev_idx]->Unlock();
				
				//�Ƴ�����map_fd_idx
				DelMapRcv(fd_tmp);
			}
			else //���ӷ������ɹ�
			{
				pcdev->b_alive = TRUE;
				pplock_dev[dev_idx]->Unlock();
				
				//֪ͨ�豸����
			}
			continue;
		}
	#endif
		//����������û���������سɹ�������ʧ�ܣ��������ڴ���
		//�����ж�
		//if (errno != EINPROGRESS)//EINPROGRESS ��ʾϵͳ���ڴ�������
		if (ret < 0 && (errno != EINPROGRESS))
		{
			close(fd_tmp);
			continue;
		}

		//�ռ�fd, ���潻��select ��ѯconnect ���
		//ȥ��O_NONBLOCK
		if (fdClrNonblockFlag(fd_tmp))
		{
			ERR_PRINT("svr IP(%s) fdClrNonblockFlag\n", inet_ntoa(in));
			close(fd_tmp);
			continue;
		}
		
		//insert map��������֮�������ӳ�ʱ���豸
		if (!map_fd_dev.insert(std::make_pair(fd_tmp, dev_idx)).second)
		{
			ERR_PRINT("map_fd_dev insert failed, IP(%s) conflict\n", inet_ntoa(in));
			close(fd_tmp);
			continue;
		}

		FD_SET(fd_tmp, &set_raw);
		fd_max = MAX(fd_tmp, fd_max);
	}

	pre_tick = SystemGetMSCount64();
	do
	{
		cur_tick = SystemGetMSCount64();

		if (map_fd_dev.empty()) //���з������������|| û�з�������Ҫ����
		{
			DBG_PRINT("map_fd_dev empty, work done\n");
			break;
		}

		if (cur_tick - pre_tick > RECONNECT_TIMEOUT*1000) //5s
		{
			DBG_PRINT("timeout\n");
			break;
		}	

		timeout.tv_sec = 1;
		timeout.tv_usec = 0;
		wset = set_raw;

		ret = select(fd_max+1, NULL, &wset, NULL, NULL);
		if (ret < 0)
		{
			sleep(1);
			continue;
		}
		else if (ret == 0)
		{
			//timeout
			continue;
		}

		for (map_iter = map_fd_dev.begin();
			map_iter != map_fd_dev.end();
			/* �� */)
		{
			fd_tmp = map_iter->first;
			dev_idx = map_iter->second;
			
			if ((INVALID_SOCKET != fd_tmp) && FD_ISSET(fd_tmp, &wset))
			{
				if (isSockIOErr(fd_tmp)) //socket ����
				{
					ERR_PRINT("isSockIOErr failed\n");
					close(fd_tmp);
					goto remove;
				}
				
				if (SetSockLinkOpt(fd_tmp))
				{				
					ERR_PRINT("SetSockLinkOpt failed, %s\n", strerror(errno));
					close(fd_tmp);
					goto remove;
				}
								
				if ((dev_idx < 0) || (dev_idx >= pool_size))
				{
					ERR_PRINT("dev_idx(%d) out of range, pool_size: %d\n", dev_idx, pool_size);
					
					close(fd_tmp);
					goto remove;
				}

				pplock_dev[dev_idx]->Lock();
				
				pcdev = ppcdev[dev_idx];
				if (NULL == pcdev)
				{
					ERR_PRINT("dev_idx(%d) pcdev == NULLs\n", dev_idx);

					pplock_dev[dev_idx]->Unlock();
					close(fd_tmp);
					goto remove;
				}

				//�������map_fd_idx
				if (AddMapRcv(fd_tmp, dev_idx))
				{
					ERR_PRINT("_AddMapRcv failed, fd_tmp(%d) conflict\n", fd_tmp);

					pplock_dev[dev_idx]->Unlock();
					close(fd_tmp);
					goto remove;
				}
				
				pcdev->sock_cmd = fd_tmp;
				in.s_addr = pcdev->dev_info.deviceIP;

				if (pcdev->DevConnect())//���ӷ�����ʧ��
				{
					DBG_PRINT("svr(%s) connect failed\n", inet_ntoa(in));

					pcdev->CleanSock();
					pplock_dev[dev_idx]->Unlock();
				}
				else //���ӷ������ɹ�
				{
					DBG_PRINT("svr(%s) connect success\n", inet_ntoa(in));	
					
					pplock_dev[dev_idx]->Unlock();

					//map_fd_idx һֱ���գ����Ƴ�
				}
				
remove:
				//�ɹ���������Ƴ���ʣ�µļ��ǳ�ʱ��
				FD_CLR(fd_tmp, &set_raw);
				map_fd_dev.erase(map_iter++);
			}
			else
			{
				++map_iter;
			}
		}		
	} while(1);

	//����map �б�ʾ���������ӳ�ʱ
	if (!map_fd_dev.empty())
	{
		for (map_iter = map_fd_dev.begin();
			map_iter != map_fd_dev.end();
			++map_iter)
		{
			close(map_iter->first);
		}
	}

	reconnect_sem.Post();
}

// 1 �����ɾ���豸
// 2 ����
void CBizDeviceManager::threadKeepAlive(uint param)
{
	//list dev_idx
	std::list<s32> list_devs_online;
	std::list<s32> list_devs_offline;
	std::list<s32>::iterator list_iter;
	s32 dev_idx;
	CBizDevice *pcdev = NULL;
	SGuiDev gdev;
	
	//ifly_DeviceInfo_t device_info;
	ifly_sysTime_t dev_sys_time;
	struct in_addr in;
	//int state = 0;//����״̬��
	//VD_BOOL b_network_ok = TRUE;
	VD_BOOL b_wait_timer = FALSE;
	//VD_BOOL b_dev_offline = FALSE;
	u64 pre_tick = 0;
	u64 cur_tick = 0;
	u64 keep_alive_pre_tick = 0;
	int fd_tmp = INVALID_SOCKET;
	
	DBG_PRINT("CBizDeviceManager::threadKeepAlive running\n");

	//��ѯgui �Ƿ�׼���ý���֪ͨ��Ϣ
	while (!BizGuiIsReady())
	{
		DBG_PRINT("wait BizGuiIsReady\n");
		sleep(1);
	}
	
	while(1)
	{	
		pre_tick = SystemGetMSCount64();
#if 0		
		//����״̬������������Ƿ�����
		b_network_ok = BizisNetworkOk();
		switch (state)
		{
			case 0:	//�����쳣��ȥ��ʼ��
				if (!b_network_ok)
				{
					state = 1;
					_UnlinkAllDevs();
				}
				break;
			case 1:	//�ȴ���������
				if (b_network_ok)	//��������
				{
					state = 0;
				}
				break;
			default:				
				ERR_PRINT("state%d, module not support*****************\n", state);				
		}

		if (state)
		{
			sleep(1);
			continue;
		}
#endif		
		// 1�������ɾ���豸
		do
		{
			plock_list_del->Lock();
		
			list_iter = list_dev_del.begin();
			if (list_iter == list_dev_del.end())//ɾ���ɾ���
			{
				plock_list_del->Unlock();
				break;
			}

			dev_idx = *list_iter;
			list_dev_del.erase(list_iter);
			
			plock_list_del->Unlock();

			if ((dev_idx < 0) || (dev_idx >= pool_size))
			{
				ERR_PRINT("dev_idx(%d) out of range, pool_size: %d\n", dev_idx, pool_size);

				continue;
			}

			//ɾ���豸	
			pplock_dev[dev_idx]->Lock();
			
			pcdev = ppcdev[dev_idx];
			if (NULL == pcdev)
			{
				ERR_PRINT("dev_idx(%d) pcdev == NULL\n", dev_idx);
				pplock_dev[dev_idx]->Unlock();

				continue;
			}

			fd_tmp = pcdev->sock_cmd;

			//�Ͽ�������
			pcdev->ShutdownStreamAll();
			
			//ע��logout
			pcdev->DevDisconnect();
			
			//�ر�cmd socket
			pcdev->CleanSock();
			
			pplock_dev[dev_idx]->Unlock();
			
			//���ø�����
			plock_dev_pool->Lock();
			
			delete ppcdev[dev_idx];
			ppcdev[dev_idx] = NULL;
			
			pool_bits[dev_idx] = 0;
			--pool_dev_count;
			
			plock_dev_pool->Unlock();
			
		} while(1);

	/************************************************************
		�����ӵ��豸
		1-���Ƿ����߷�
		2-1-�����豸--> reconnect
		2-2-�����豸��������������ٷ�
		2-2-1-�޴�--> keepalive
		2-2-2-�д�--> ���������һ�μ��ʱΪ�����豸
	************************************************************/
		list_devs_online.clear();
		list_devs_offline.clear();

		int dev_type = 0;
		for (dev_type = EM_NVR; dev_type < EM_DEV_TYPE_MAX; ++dev_type)
		{
			_SplitDevFromMap((EM_DEV_TYPE)dev_type, list_devs_online, list_devs_offline);
		}

		//�ύ�����豸�б����ʱ����������
		b_wait_timer = FALSE;
		if (!list_devs_offline.empty())
		{
			b_wait_timer = TRUE;
			m_reconnect_timer->Start(this, (VD_TIMERPROC)&CBizDeviceManager::timerFuncReconnect, 0, 0, (u32)&list_devs_offline);
		}

		//�������豸����
		cur_tick = SystemGetMSCount64();

		if ((keep_alive_pre_tick == 0) || (cur_tick - keep_alive_pre_tick > KEEP_ALIVE_INTERVAL*1000))
		{
			keep_alive_pre_tick = cur_tick;
			
			for (list_iter = list_devs_online.begin();
				list_iter != list_devs_online.end();
				++list_iter)
			{
				dev_idx = *list_iter;

				if ((dev_idx < 0) || (dev_idx >= pool_size))
				{
					ERR_PRINT("dev_idx(%d) out of range, pool_size: %d\n", dev_idx, pool_size);

					continue;
				}
				
				//�豸����
				pplock_dev[dev_idx]->Lock();
				
				pcdev = ppcdev[dev_idx];
				if (NULL == pcdev)
				{
					ERR_PRINT("dev_idx(%d) pcdev == NULL\n", dev_idx);
					
					pplock_dev[dev_idx]->Unlock();
					continue;
				}
				
				in.s_addr = pcdev->dev_info.deviceIP;
				
				if (pcdev->GetDevSysTime(&dev_sys_time))//����
				{
					DBG_PRINT("svr(%s) keepalive failed\n", inet_ntoa(in));
				}
				else //�ɹ����豸���ߣ������Ҫ��������������
				{
					//DBG_PRINT("svr(%s) dev_idx(%d) keep alive success\n", inet_ntoa(in), dev_idx);
					//pcdev->CheckAndReconnectStream();
				}

				pplock_dev[dev_idx]->Unlock();
			}
		}
		
		//�ȴ���ʱ����������
		if (b_wait_timer)
		{
			reconnect_sem.Pend();
		}

		cur_tick = SystemGetMSCount64();
		if (cur_tick < pre_tick)
		{
			ERR_PRINT("cur_tick(%llu) < pre_tick(%llu), inconceivable\n", cur_tick, pre_tick);
			sleep(3);
		}
		else if ((cur_tick - pre_tick)/1000 < 3)
		{
			sleep(3-(cur_tick - pre_tick)/1000);
		}
	}

	ERR_PRINT("CBizDeviceManager::threadKeepAlive exit, inconceivable\n");
}

// ����Ϣ
int CBizDeviceManager::WriteMsg(SBizMsg_t *pmsg, u32 msg_len)
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
		
		pthread_mutex_unlock(&mq_mutex);
		return ret;
	} 

	mq_msg_count++;

	ret = pthread_cond_signal(&mq_cond);
	if (ret)
	{
		ERR_PRINT("pthread_cond_signal failed, err(%d, %s)\n", ret, strerror(ret));

		pthread_mutex_unlock(&mq_mutex);
		return ret;
	}

	ret = pthread_mutex_unlock(&mq_mutex);
	if (ret)
	{
		ERR_PRINT("pthread_mutex_unlock failed, err(%d, %s)\n", ret, strerror(ret));
		
		return -FAILURE;
	}
	
	return SUCCESS;
}
void CBizDeviceManager::threadMsg(uint param)//����Ϣ
{
	VD_BOOL b_process = FALSE;
	int ret = SUCCESS;
	struct in_addr in;
	s32 dev_idx = INVALID_VALUE;//�豸���� biz_dev
	CBizDevice *pcdev = NULL;
	u32 dev_ip = INADDR_NONE;
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
			
			//DBG_PRINT("msg type: %d\n", msg_type);		
			
			switch (msg_type)
			{
				case EM_DEV_MSG_PROGRESS: //�طŽ���֪ͨ
				{
					dev_idx = msg.dev_idx;
					u32 link_id = msg.un_part_chn.link_id;
					u32 cur_pos = msg.un_part_data.stream_progress.cur_pos;
					u32 total_size = msg.un_part_data.stream_progress.total_size;
					
					u32 stream_id = INVALID_VALUE;
					MAP_LID_PSTREAM::iterator map_iter;
					SDevStream_t* pstream = NULL;
					
					pplock_dev[dev_idx]->Lock();

					pcdev = ppcdev[dev_idx];
					if (NULL == pcdev)
					{
						ERR_PRINT("dev_idx(%d) pcdev == NULL\n", dev_idx);
						
						pplock_dev[dev_idx]->Unlock();						
						break ;
					}
					
					dev_ip = pcdev->dev_info.deviceIP;
					
					pcdev->plock4stream->Lock();
								
					
					
					in.s_addr = dev_ip;
#if 0

					DBG_PRINT("link_id: %d, cur_pos: %d, total_size: %d\n", 
						link_id, cur_pos, total_size);
#endif
					map_iter = pcdev->map_lid_pstream.find(link_id);
					if (map_iter == pcdev->map_lid_pstream.end())
					{
						ERR_PRINT("svr IP: %s, link_id(%u), map_lid_pstream not find\n", 
							inet_ntoa(in), link_id);//���ܷ�����StreamStart ��map_lid_pstream.insert ��֮ǰʧ��
						
						pcdev->plock4stream->Unlock();
						pplock_dev[dev_idx]->Unlock();
						break ;
					}
					
					pstream = map_iter->second;
					if (NULL == pstream)
					{
						ERR_PRINT("svr IP: %s, link_id(%u), NULL == pstream\n", 
							inet_ntoa(in), link_id);
						
						pcdev->plock4stream->Unlock();
						pplock_dev[dev_idx]->Unlock();
						break ;
					}

					stream_id = pstream->stream_id;
					pcdev->plock4stream->Unlock();
					pplock_dev[dev_idx]->Unlock();
					
#if 1
					//send msg to stream manager
					memset(&msg, 0, sizeof(SBizMsg_t));
					msg.msg_type = EM_STREAM_MSG_PROGRESS;
					msg.un_part_chn.stream_id = stream_id;
					msg.un_part_data.stream_progress.cur_pos = cur_pos;
					msg.un_part_data.stream_progress.total_size = total_size;

					int ret = BizSendMsg2StreamManager(&msg, sizeof(SBizMsg_t));
					if (ret)
					{
						ERR_PRINT("svr IP: %s, link_id(%u), msg_type: %d, BizSendMsg2StreamManager failed, ret: %d\n",
							inet_ntoa(in), link_id, msg.msg_type, ret);
				 	}
#endif			
				} break;

				case EM_DEV_MSG_FINISH: //����������ļ��ط�(��)��ʱ��ط�(��)
				{
					dev_idx = msg.dev_idx;
					u32 link_id = msg.un_part_chn.link_id;
					u32 stream_id = INVALID_VALUE;
					MAP_LID_PSTREAM::iterator map_iter;
					SDevStream_t* pstream = NULL;
					
					pplock_dev[dev_idx]->Lock();

					pcdev = ppcdev[dev_idx];
					if (NULL == pcdev)
					{
						ERR_PRINT("dev_idx(%d) pcdev == NULL\n", dev_idx);
						
						pplock_dev[dev_idx]->Unlock();						
						break ;
					}
					
					dev_ip = pcdev->dev_info.deviceIP;
					
					pcdev->plock4stream->Lock();
			
					in.s_addr = dev_ip;
#if 0

					DBG_PRINT("link_id: %d, totallen: %d, currPos: %d\n", 
						progress.id, progress.totallen, progress.currPos);
#endif
					map_iter = pcdev->map_lid_pstream.find(link_id);
					if (map_iter == pcdev->map_lid_pstream.end())
					{
						ERR_PRINT("svr IP: %s, link_id(%u), map_lid_pstream not find\n", 
							inet_ntoa(in), link_id);//���ܷ�����StreamStart ��map_lid_pstream.insert ��֮ǰʧ��
						
						pcdev->plock4stream->Unlock();
						pplock_dev[dev_idx]->Unlock();
						break ;
					}
					
					pstream = map_iter->second;
					if (NULL == pstream)
					{
						ERR_PRINT("svr IP: %s, link_id(%u), NULL == pstream\n", 
							inet_ntoa(in), link_id);
						
						pcdev->plock4stream->Unlock();
						pplock_dev[dev_idx]->Unlock();
						break ;
					}

					stream_id = pstream->stream_id;
					pcdev->plock4stream->Unlock();
					pplock_dev[dev_idx]->Unlock();

					//send msg to stream manager
					memset(&msg, 0, sizeof(SBizMsg_t));
					msg.msg_type = EM_STREAM_MSG_FINISH;
					msg.un_part_chn.stream_id = stream_id;

					int ret = BizSendMsg2StreamManager(&msg, sizeof(SBizMsg_t));
					if (ret)
					{
						ERR_PRINT("svr IP: %s, link_id(%u), msg_type: %d, BizSendMsg2StreamManager failed, ret: %d\n",
							inet_ntoa(in), link_id, msg.msg_type, ret);
				 	}
				} break;

				case CTRL_NOTIFY_ALARMINFO: //�첽������Ϣ
				{
				} break;

				default:
					ERR_PRINT("msg_type: %d, not support\n", msg_type);
			}
		}
	}

thread_exit:
	
	ERR_PRINT("CBizDeviceManager::threadMsg exit, inconceivable\n");
}

#if 0
u16 CBizDeviceManager::_IncAndGetSyncNum()
{
	u16 no = 0;
	
	plock4sync_num->Lock();
	no = ++sync_num;
	plock4sync_num->Unlock();

	return no;
}

u16 CBizDeviceManager::_GetSyncNum()
{
	u16 no = 0;
	
	plock4sync_num->Lock();
	no = sync_num;	
	plock4sync_num->Unlock();

	return no;
}
#endif


/***********************************
 	               search device
***********************************/
static pthread_t g_ack_thread_id = 0;
static pthread_mutex_t g_search_lock = PTHREAD_MUTEX_INITIALIZER;
static unsigned char g_search_start = 0;
static std::vector<SBiz_DeviceInfo_t> *g_search_nvr_list;//�������NVR�б�
static std::vector<SBiz_DeviceInfo_t> *g_search_patrol_dec_list;//�������patrol_dec �б�
static std::vector<SBiz_DeviceInfo_t> *g_search_switch_dec_list;//�������switch_dec �б�


void SearchDevice_CB(ifly_DeviceInfo_t dev, void* pContext)
{
	SBiz_DeviceInfo_t dev_info;
	
	pthread_mutex_lock(&g_search_lock);

	if (g_search_start == 0)//δ��������
	{
		pthread_mutex_unlock(&g_search_lock);
		ERR_PRINT("Has not yet started\n");
		return;
	}

#if 1
	struct in_addr in;
	in.s_addr = dev.deviceIP;
	DBG_PRINT("dev name: %s, deviceIP: %s, port: %u, nNVROrDecoder: %d\n", \
		dev.device_name, inet_ntoa(in), dev.devicePort, dev.nNVROrDecoder);
#endif

	//�õ�����IP���ų�����
	u32 localIP = GetLocalIp();
	if (localIP == 0)
	{
		pthread_mutex_unlock(&g_search_lock);
		ERR_PRINT("GetLocalIp failed\n");
		return;
	}

	if (localIP == dev.deviceIP)
	{
		pthread_mutex_unlock(&g_search_lock);
		//printf("%s search device is itself, return\n", );
		return;
	}

	//
	dev_info.maxChnNum 		= dev.maxChnNum;
	//nNVROrDecoder: 1 nvr��2 ��Ѳ��������3 �л���������4 �������
	//ת��һ��
	dev_info.devicetype		= (u8)EM_DEV_TYPE_NONE;
	dev_info.devicetype 	+= dev.nNVROrDecoder;
	dev_info.devicePort 	= dev.devicePort;
	dev_info.deviceIP 		= dev.deviceIP;

	switch (dev_info.devicetype)
	{
		case EM_NVR:
			if (NULL != g_search_nvr_list)
				g_search_nvr_list->push_back(dev_info);
			break;
		case EM_PATROL_DEC:
			if (NULL != g_search_patrol_dec_list)
				g_search_patrol_dec_list->push_back(dev_info);
			break;
		case EM_SWITCH_DEC:
			if (NULL != g_search_switch_dec_list)
				g_search_switch_dec_list->push_back(dev_info);
			break;
		default:
			ERR_PRINT("dev_type%d nonsupport\n", dev_info.devicetype);
	}
	
	pthread_mutex_unlock(&g_search_lock);
}

//�豸����
int BizDevSearch(std::vector<SBiz_DeviceInfo_t> *pvnvr_list, 
							std::vector<SBiz_DeviceInfo_t> *pvpatrol_dec_list, 
							std::vector<SBiz_DeviceInfo_t> *pvswitch_dec_list)
{
	int ret = 0;

	if (!g_biz_device_manager.isInited())
	{
		ERR_PRINT("CBizDeviceManager not inited\n");
		return 0;
	}
	
	pthread_mutex_lock(&g_search_lock);

	if (g_search_start)//�Ѿ���ʼ����
	{
		pthread_mutex_unlock(&g_search_lock);
		DBG_PRINT("has already started\n");
		return -FAILURE;
	}

	ret = CreateAckSearchProc(&g_ack_thread_id);
	if (ret)
	{
		pthread_mutex_unlock(&g_search_lock);
		DBG_PRINT("CreateAckSearchProc failed\n");
		return -FAILURE;
	}
	
	g_search_start = 1;
	g_search_nvr_list = pvnvr_list;//�������NVR�б�
	g_search_patrol_dec_list = pvpatrol_dec_list;//�������patrol_dec �б�
	g_search_switch_dec_list = pvswitch_dec_list;//�������switch_dec �б�

	pthread_mutex_unlock(&g_search_lock);

	ret = SearchDevice();
	if (ret)
	{
		ERR_PRINT("SearchDevice failed\n");
		return -FAILURE;
	}
	
	sleep(3);

	//ȡ����Ӧ�߳�
	pthread_mutex_lock(&g_search_lock);
	
	g_search_start = 0;
	
	if (g_ack_thread_id)
	{
		//ȡ����Ӧ�߳�
		ret = pthread_cancel(g_ack_thread_id);
		if (ret)
		{
			printf("%s pthread_cancel failed\n", __func__);
			return 0;
		}
		
		ret = pthread_join(g_ack_thread_id, NULL);
		if (ret)
		{
			printf("%s pthread_join failed\n", __func__);
			return 0;
		}

		g_ack_thread_id = 0;
	}

	if (NULL != g_search_nvr_list)
		std::sort(g_search_nvr_list->begin(), g_search_nvr_list->end());

	if (NULL != g_search_patrol_dec_list)
		std::sort(g_search_patrol_dec_list->begin(), g_search_patrol_dec_list->end());

	if (NULL != g_search_switch_dec_list)
		std::sort(g_search_switch_dec_list->begin(), g_search_switch_dec_list->end());

	
	pthread_mutex_unlock(&g_search_lock);

#if 0
	struct in_addr in;

	std::vector<SBiz_DeviceInfo_t>::iterator iter;
	for (iter = g_search_nvr_list->begin();
			iter != g_search_nvr_list->end();
			++iter)
	{		
		in.s_addr = iter->deviceIP;
		DBG_PRINT("nvr list deviceIP: %s\n", inet_ntoa(in));
	}

	for (iter = g_search_patrol_dec_list->begin();
			iter != g_search_patrol_dec_list->end();
			++iter)
	{		
		in.s_addr = iter->deviceIP;
		DBG_PRINT("patrol dec list deviceIP: %s\n", inet_ntoa(in));
	}

	for (iter = g_search_switch_dec_list->begin();
			iter != g_search_switch_dec_list->end();
			++iter)
	{		
		in.s_addr = iter->deviceIP;
		DBG_PRINT("switch dec list deviceIP: %s\n", inet_ntoa(in));
	}
#endif

	return SUCCESS;
}


/*****************************************************
			Device  Manager�ⲿ�ӿ�ʵ��
*****************************************************/

int BizDeviceFirstInit(void)
{	
	return g_biz_device_manager.FirstInit();
}

int BizDeviceSecondInit(void)
{
	
	//ע�������ص�����
	SetFindDeviceCB(SearchDevice_CB, NULL);
	
	return g_biz_device_manager.SecondInit();
}

int BizAddDev(EM_DEV_TYPE dev_type, u32 dev_ip)
{
	return g_biz_device_manager.AddDev(dev_type, dev_ip);
}

int BizDelDev(EM_DEV_TYPE dev_type, u32 dev_ip)
{
	return g_biz_device_manager.DelDev(dev_type, dev_ip);
}

int BizGetDevIPList(EM_DEV_TYPE dev_type, std::list<u32> &dev_ip_list)//�����ֽ���
{
	return g_biz_device_manager.GetDevIPList(dev_type, dev_ip_list);
}

int BizGetDevIdx(EM_DEV_TYPE dev_type, u32 dev_ip)
{
	return g_biz_device_manager.GetDevIdx(dev_type, dev_ip);
}

int BizGetDevInfo(EM_DEV_TYPE dev_type, u32 dev_ip, SGuiDev *pdev)
{
	return g_biz_device_manager.GetDevInfo(dev_type, dev_ip, pdev);
}

//��ȡ����ͨ����IPC��Ϣ
int BizGetDevChnIPCInfo(EM_DEV_TYPE dev_type, u32 dev_ip, ifly_ipc_info_t * pipc_info, u32 size)
{
	return g_biz_device_manager.GetDevChnIPCInfo(dev_type, dev_ip, pipc_info, size);
}

//ֻ֧��NVR���õ�NVRͨ����(osd info)
int BizGetDevChnName(EM_DEV_TYPE dev_type, u32 dev_ip, u8 chn, char *pbuf, u32 size)
{
	return g_biz_device_manager.GetDevChnName(dev_type, dev_ip, chn, pbuf, size);
}

//���ý�����ͨ����Ӧ��NVR ͨ��
int BizSetDevChnIpc(EM_DEV_TYPE dev_type, u32 dec_ip , u8 dec_chn, u32 nvr_ip, u8 nvr_chn)
{
	return g_biz_device_manager.SetDevChnIpc(dev_type, dec_ip, dec_chn, nvr_ip, nvr_chn);
}

//ɾ��ͨ��IPC
int BizDelDevChnIpc(EM_DEV_TYPE dev_type, u32 dec_ip , u8 dec_chn)
{
	return g_biz_device_manager.DelDevChnIpc(dev_type, dec_ip, dec_chn);
}

//NVR ¼������
int BizDevRecFilesSearch(u32 nvr_ip, ifly_recsearch_param_t *psearch_para, ifly_search_file_result_t *psearch_result)
{
	return g_biz_device_manager.DevRecFilesSearch(nvr_ip, psearch_para, psearch_result);
}








//��ȡ�豸��Ѳ����
int BizDevGetPatrolPara(EM_DEV_TYPE dev_type, u32 dec_ip, ifly_patrol_para_t *para, u32 *pbuf_size)
{
	return g_biz_device_manager.DevGetPatrolPara(dev_type, dec_ip, para, pbuf_size);
}











int BizStartNotifyDevInfo(void)	//ʹ��֪ͨ���豸�㽫��Ϣ֪ͨ���ϲ�
{
	return g_biz_device_manager.StartNotifyDevInfo();
}

//������API
//������ID
int BizDevReqStreamStart(EM_DEV_TYPE dev_type, u32 dev_ip, u32 stream_id, ifly_TCP_Stream_Req *preq, s32 *psock_stream)
{
	return g_biz_device_manager.DevReqStreamStart(dev_type, dev_ip, stream_id, preq, psock_stream);
}

int BizDevReqStreamStop(EM_DEV_TYPE dev_type, u32 dev_ip, u32 stream_id, s32 stop_reason)//GLB_ERROR_NUM
{
	return g_biz_device_manager.DevReqStreamStopByStreamID(dev_type, dev_ip, stream_id, stop_reason);
}

int BizDevStreamProgress(EM_DEV_TYPE dev_type, u32 dev_ip, u32 stream_id, VD_BOOL b)//���ջطŽ�����Ϣ
{
	return g_biz_device_manager.DevStreamProgress(dev_type, dev_ip, stream_id, b);//���ջطŽ�����Ϣ
}

int BizDevStreamPause(EM_DEV_TYPE dev_type, u32 dev_ip, u32 stream_id)
{
	return g_biz_device_manager.DevStreamPause(dev_type, dev_ip, stream_id);
}

int BizDevStreamResume(EM_DEV_TYPE dev_type, u32 dev_ip, u32 stream_id)
{
	return g_biz_device_manager.DevStreamResume(dev_type, dev_ip, stream_id);
}

int BizDevStreamStep(EM_DEV_TYPE dev_type, u32 dev_ip, u32 stream_id)//֡��
{
	return g_biz_device_manager.DevStreamStep(dev_type, dev_ip, stream_id);
}

int BizDevStreamRate(EM_DEV_TYPE dev_type, u32 dev_ip, u32 stream_id, s32 rate)//{-8, -4, -2, 1, 2, 4, 8}
{
	return g_biz_device_manager.DevStreamRate(dev_type, dev_ip, stream_id, rate);
}

int BizDevStreamSeek(EM_DEV_TYPE dev_type, u32 dev_ip, u32 stream_id, u32 time)
{
	return g_biz_device_manager.DevStreamSeek(dev_type, dev_ip, stream_id, time);
}






