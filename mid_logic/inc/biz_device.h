#ifndef __BIZ_DEVICE_H__
#define __BIZ_DEVICE_H__

#include <map>
#include <vector>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>


#include "types.h"
#include "C_Lock.h"
#include "object.h"
#include "csemaphore.h"
#include "cthread.h"
#include "biz_config.h"
#include "biz_remote_stream_define.h"
#include "gui_dev.h"
#include "ctrlprotocol.h"
#include "glb_error_num.h"


//�豸��Ϣ
typedef struct DeviceInfo_t
{
	u8	maxChnNum;		//���ͨ����
	u8 	devicetype;		//EM_DEV_TYPE
	u16	devicePort;		//�豸�˿� 
	u32	deviceIP; 		//�豸IP  
	bool operator==(const DeviceInfo_t &a) const
	{
		return deviceIP == a.deviceIP;
	}

	bool operator!=(const DeviceInfo_t &a) const
	{
		return deviceIP != a.deviceIP;
	}

	bool operator<(const DeviceInfo_t &a) const
	{
		return ntohl(deviceIP) < ntohl(a.deviceIP);
	}

	DeviceInfo_t& operator=(const DeviceInfo_t &a)
	{
		maxChnNum = a.maxChnNum;
		devicetype = a.devicetype;
		devicePort = a.devicePort;
		deviceIP = a.deviceIP;
		return *this;
	}
	
	DeviceInfo_t()
	: maxChnNum(0)
	, devicetype(0)
	, devicePort(0)
	, deviceIP(INADDR_NONE)
	{
		
	}

} SBiz_DeviceInfo_t;


#define MaxMediaLinks (5)
#define MaxIdleNum (60)

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

	//stream
	//�ɹ�����stearm_rcv[MaxMediaLinks] �±�stream_idx
	int StreamStart(ifly_TCP_Stream_Req *preq, CMediaStream *pstream);
	int StreamStop(int stream_idx, EM_STREAM_STATE_TYPE stop_reason = EM_STREAM_STOP);//�ر�ԭ��Ĭ�������ر�
	void _CleanStream(int stream_idx);

	//�ر���������������
	int ShutdownStreamAll();
	
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
	//�ײ����ݽ���
	int DevNetDialogue(u16 event, const void *content, int length, void* ackbuf, int ackbuflen);
	//���ڴ�����
	int DevNetDialogueAfter(int net_ret);
	
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
	VD_BOOL bthread_stream_running;
	VD_BOOL bthread_stream_exit;//�ⲿ�����߳��˳�
	int stream_cnt;//�ͻ�����������������stearm_rcv ������Ч��Ա��
	int idle_cnt;//���м������߳̿���1���Ӻ��˳�
	CSemaphore sem_exit;//�ȴ�threadStreamRcv�˳��ź���
	SDev_StearmRcv_t stream_rcv[MaxMediaLinks]; //�������ṹMaxMediaLinks
	void threadStreamRcv(uint param);
	Threadlet m_threadlet_stream_rcv;
};


//�ⲿ�ӿ�
#ifdef __cplusplus
extern "C" {
#endif

int	BizDeviceInit();

//�豸����
int BizDevSearch(std::vector<SBiz_DeviceInfo_t> *pvnvr_list, 
							std::vector<SBiz_DeviceInfo_t> *pvpatrol_dec_list, 
							std::vector<SBiz_DeviceInfo_t> *pvswitch_dec_list);

int BizAddDev(EM_DEV_TYPE dev_type, u32 dev_ip);
int BizDelDev(EM_DEV_TYPE dev_type, u32 dev_ip);
int BizGetDevIPList(EM_DEV_TYPE dev_type, std::list<u32> &dev_ip_list);//�����ֽ���
int BizGetDevIdx(EM_DEV_TYPE dev_type, u32 dev_ip);
int BizGetDevInfo(EM_DEV_TYPE dev_type, u32 dev_ip, SGuiDev *pdev);
//��ȡ����ͨ����IPC��Ϣ
int BizGetDevChnIPCInfo(EM_DEV_TYPE dev_type, u32 dev_ip, ifly_ipc_info_t * pipc_info, u32 size);
//ֻ֧��NVR���õ�NVRͨ����(osd info)
int BizGetDevChnName(EM_DEV_TYPE dev_type, u32 dev_ip, u8 chn, char *pbuf, u32 size);
//���ý�����ͨ����Ӧ��NVR ͨ��
int BizSetDevChnIpc(EM_DEV_TYPE dev_type, u32 dec_ip , u8 dec_chn, u32 nvr_ip, u8 nvr_chn);
//ɾ��ͨ��IPC
int BizDelDevChnIpc(EM_DEV_TYPE dev_type, u32 dec_ip , u8 dec_chn);
//��ȡ�豸��Ѳ����
int BizDevGetPatrolPara(EM_DEV_TYPE dev_type, u32 dec_ip, ifly_patrol_para_t *para, u32 *pbuf_size);
//NVR ¼������
int BizDevRecFilesSearch(u32 nvr_ip, ifly_recsearch_param_t *psearch_para, ifly_search_file_result_t *psearch_result);













int BizStartNotifyDevInfo();//ʹ��֪ͨ���豸�㽫��Ϣ֪ͨ���ϲ�


//�ɹ�����stearm_rcv[MaxMediaLinks] �±�stream_idx
int BizReqStreamStart(EM_DEV_TYPE dev_type, u32 dev_ip, ifly_TCP_Stream_Req *preq, CMediaStream *pstream);
int BizReqStreamStop(EM_DEV_TYPE dev_type, u32 dev_ip, s32 stream_idx);





#ifdef __cplusplus
}
#endif





#endif

