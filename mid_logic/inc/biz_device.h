#ifndef __BIZ_DEVICE_H__
#define __BIZ_DEVICE_H__

#include <map>
#include <vector>

#include <arpa/inet.h>


#include "types.h"
#include "C_Lock.h"
#include "object.h"
#include "csemaphore.h"
#include "cthread.h"
#include "biz_config.h"
#include "biz_remote_stream_define.h"



//�豸��Ϣ
typedef struct DeviceInfo_t
{
	u8	maxChnNum;		//���ͨ����
	u8 	devicetype;		//EM_DEV_TYPE
	u16	devicePort;		//�豸�˿� 
	u32	deviceIP; 		//�豸IP  
	bool operator==(const DeviceInfo_t &a) const
	{
		return a.deviceIP == deviceIP;
	}

	bool operator!=(const DeviceInfo_t &a) const
	{
		return a.deviceIP != deviceIP;
	}

	bool operator<(const DeviceInfo_t &a) const
	{
		return ntohl(deviceIP) < ntohl(a.deviceIP);
	}

	DeviceInfo_t& operator=(const DeviceInfo_t &a)
	{
		maxChnNum = maxChnNum;
		devicetype = devicetype;
		devicePort = devicePort;
		deviceIP = deviceIP;
		return *this;
	}
	
	DeviceInfo_t()
	: maxChnNum(0)
	, devicetype(0)
	, devicePort(0)
	, deviceIP(0)
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
	//���ӡ���¼������
	int DevConnect();
	//�Ͽ���������
	int DevDisconnect();

	//stream
	//�ɹ�����stearm_rcv[MaxMediaLinks] �±�stream_idx
	int StreamStart(ifly_TCP_Stream_Req *preq, CMediaStream *pstream);
	int StreamStop(int stream_idx);
	void _CleanStream(int stream_idx);

	//�ر���������������
	int ShutdownStreamAll();
	
	//������������������
	int CheckAndReconnectStream();
	
	
private:
    CBizDevice(CBizDevice &)
	{
		
	}

	void FreeSrc();//�ͷ���Դ
	int _DevLogin(ifly_loginpara_t *plogin);
	int _DevLogout(ifly_loginpara_t *plogin);
	int _DevSetAlarmUpload(u8 upload_enable);

private:
	C_Lock *plock4param;//mutex
	SBiz_DeviceInfo_t dev_info;
	ifly_loginpara_t login;
	s32 dev_idx; //dev pool index
	int sock_cmd;		//����sock����¼�����ձ���
	VD_BOOL b_alive;	//�Ƿ�����
	int cnt_err;		//��ʱδ�á�����������ۼ�2�Σ����������豸
						//_Add2SetFromMap ���cnt_err ��close dev sock
	//stream					
	C_Lock *plock4stream;//mutex
	VD_BOOL bthread_stream_running;
	VD_BOOL bthread_stream_exit;//�ⲿ�����߳��˳�
	int stream_cnt;
	int idle_cnt;//���м������߳̿���1���Ӻ��˳�
	CSemaphore sem_exit;//�ȴ�threadStreamRcv�˳��ź���
	SDev_StearmRcv_t stearm_rcv[MaxMediaLinks]; //�������ṹMaxMediaLinks
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
int BizGetDevIdx(EM_DEV_TYPE dev_type, u32 dev_ip);
int BizReqStreamStart(s32 dev_idx, ifly_TCP_Stream_Req *preq, CMediaStream *pstream);
int BizReqStreamStop(s32 dev_idx, s32 stream_idx);



#ifdef __cplusplus
}
#endif





#endif

