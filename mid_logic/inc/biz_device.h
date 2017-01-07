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
#include "gui_dev.h"
#include "ctrlprotocol.h"
#include "glb_error_num.h"

#include "biz_config.h"
#include "biz_msg_type.h"


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

	~DeviceInfo_t()
	{
		
	}
} SBiz_DeviceInfo_t;



//�ⲿ�ӿ�
#ifdef __cplusplus
extern "C" {
#endif

int BizDeviceFirstInit(void);

int BizDeviceSecondInit(void);



//�豸����
int BizDevSearch(std::vector<SBiz_DeviceInfo_t> *pvnvr_list, 
							std::vector<SBiz_DeviceInfo_t> *pvpatrol_dec_list, 
							std::vector<SBiz_DeviceInfo_t> *pvswitch_dec_list);

int BizSendMsg2DevManager(SBizMsg_t *pmsg, u32 msg_len);


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
int BizDevReqStreamStart(EM_DEV_TYPE dev_type, u32 dev_ip, u32 stream_id, ifly_TCP_Stream_Req *preq, s32 *psock_stream);
int BizDevReqStreamStop(EM_DEV_TYPE dev_type, u32 dev_ip, u32 stream_id, s32 stop_reason=SUCCESS);//GLB_ERROR_NUM
int BizDevStreamProgress(EM_DEV_TYPE dev_type, u32 dev_ip, u32 stream_id, VD_BOOL b);//���ջطŽ�����Ϣ




#ifdef __cplusplus
}
#endif





#endif

