#ifndef __BIZ_CONFIG_H__
#define __BIZ_CONFIG_H__

#include<vector>
#include<string>
#include<algorithm>

#include "types.h"
#include "biz_types.h"
#include "devcustom.h"
#include "ctrlprotocol.h"


//��ȡ�������
typedef enum
{
	//�豸���Ͳ���
	EM_DEV_TYPE_NONE,
	EM_NVR,
	EM_PATROL_DEC,
	EM_SWITCH_DEC,
	EM_DEV_TYPE_MAX,
} EM_DEV_TYPE;


typedef struct
{
	u8 nConfSaveType;			//0:flash 1;file
	u8 nConfFormat; 			//0=Struct;1=Ini;2=XML
	std::string nDevPath;			//flash�豸·��
	std::string nDefaultConfPath;	//Ĭ�������ļ�·��
	std::string nFilePath;			//�����ļ�·��
	u8 nConfZip;				//0:��ѹ�� 1:ѹ��
	u32 nFlashOffset;			//���������ƫ�Ƶ�ַ
	u32 nFlashSizeLimit;		//��������Ĵ�С
} SConfigDevInfo;

typedef struct
{
	std::string nVersion;		//�汾��
	std::string nModel;			//�ͺ�
	std::string nproductnumber;	//��Ʒ��
	u32 nPreview_nums;			//Ԥ��ͨ����
	u32 nsvr_connect_nums;	//��Ϊ��������tcp ���������
	u32 nTvwall_nums;		//����ǽ���ñ���������
	u32 nTvwall_devs_max;	//����ǽ���豸���������ֵ
	u32 nTvwall_devs_per_line;	//����ǽÿ���豸��������
	u32 nTvwall_devs_per_col;	//����ǽÿ���豸��������
	u32 nNvr_nums;			//����NVR��������
	u32 nDec_nums;			//����DEC��������
} SConfigDvrProperty;

//ʱ�����ò���
typedef struct ConfigTimeParam
{
	u8 ndate_format;	//���ڸ�ʽ
	u8 ntime_format;	//ʱ���ʽ
	u8 btime_sync;		//�Զ�ͬ��
	std::string ntp_svr_ip;	//ʱ�������IP
	bool operator==(const ConfigTimeParam &a)
	{
		return ((a.ndate_format == ndate_format) \
			&&(a.ntime_format == ntime_format) \
			&&(a.btime_sync == btime_sync) \
			&&(a.ntp_svr_ip == ntp_svr_ip));
	}
	bool operator!=(const ConfigTimeParam &a)
	{
		return ((a.ndate_format != ndate_format) \
			||(a.ntime_format != ntime_format) \
			||(a.btime_sync != btime_sync) \
			||(a.ntp_svr_ip != ntp_svr_ip));
	}
} SConfigTimeParam;

//�������ò���
typedef struct ConfigNetParam
{
	u32 nhost_ip;
	u32 nsubmask;
	u32 ngateway;
	u32 ndns1;
	u32 ndns2;
	u32 nsvr_port;
	std::string  mac;
	bool operator==(const ConfigNetParam &a)
	{
		return ((a.nhost_ip == nhost_ip) \
			&&(a.nsubmask == nsubmask) \
			&&(a.ngateway == ngateway) \
			&&(a.ndns1 == ndns1) \
			&&(a.ndns2 == ndns2) \
			&&(a.nsvr_port == nsvr_port) \
			&&(a.mac == mac));
	}
	bool operator!=(const ConfigNetParam &a)
	{
		return ((a.nhost_ip != nhost_ip) \
			||(a.nsubmask != nsubmask) \
			||(a.ngateway != ngateway) \
			||(a.ndns1 != ndns1) \
			||(a.ndns2 != ndns2) \
			||(a.nsvr_port != nsvr_port) \
			||(a.mac != mac));
	}
} SConfigNetParam;

//����ǽ���ò���
typedef struct ConfigTvWallParam
{
	u32 ndevs_per_line;	//����ǽ����Ļ��	
	u32 ndevs_per_col;	//����ǽ����Ļ��
	std::vector<uint> devlist;	//��ά����ǽת��Ϊһά�豸�б������豸һһ��Ӧ���ո�����0��ʾ����δ���ý�����
	bool operator==(const ConfigTvWallParam &a)
	{
		return ((a.ndevs_per_line == ndevs_per_line) \
			&&(a.ndevs_per_col == ndevs_per_col) \
			&&(a.devlist == devlist));
	}
	bool operator!=(const ConfigTvWallParam &a)
	{
		return ((a.ndevs_per_line != ndevs_per_line) \
			||(a.ndevs_per_col != ndevs_per_col) \
			||(a.devlist != devlist));
	}
} SConfigTvWallParam;

typedef struct ConfigAlmLinkParam

{
	u32 nvrip;	//����������NVR ip
	u32 alm_type;	//����Դ����(0: sensor, 1: ipc)
	u32 alm_chn;	//����Դ����ͨ��
	u32 decip;		//������DEC
	u32 dec_chn;	//DEC ��ͨ���Ŵ�
	bool operator==(const ConfigAlmLinkParam &a)
	{
		return ((a.nvrip == nvrip) \
			&&(a.alm_type == alm_type) \
			&&(a.alm_chn == alm_chn) \
			&&(a.decip == decip) \
			&&(a.dec_chn == dec_chn));
	}
	bool operator!=(const ConfigAlmLinkParam &a)
	{
		return ((a.nvrip != nvrip) \
			||(a.alm_type != alm_type) \
			||(a.alm_chn != alm_chn) \
			||(a.decip != decip) \
			||(a.dec_chn != dec_chn));
	}
} SConfigAlmLinkParam;

//�������ò�������
typedef struct
{
	SConfigTimeParam stime_param;	//ʱ�����ò���
	SConfigNetParam snet_param;	//�������ò���
	std::vector<SConfigTvWallParam> vtvwall_list;
	std::vector<uint> vnvr_list;
	std::vector<uint> vpatrol_dec_list; //��Ѳ�ͽ������б�
	std::vector<uint> vswitch_dec_list; //�л��ͽ������б�
	std::vector<SConfigAlmLinkParam> valm_link_list; //���������б�
} SConfigAllParam;

#ifdef __cplusplus
extern "C" {
#endif

int BizConfigInit(void);

//��ȡĬ�ϲ���
int BizConfigGetDefaultTimeParam(SConfigTimeParam &stimme_param);
int BizConfigGetDefaultNetParam(SConfigNetParam &snet_param);

//��ȡ����
int BizConfigGetDevInfo(ifly_DeviceInfo_t *pdev_info);
int BizConfigGetDvrProperty(SConfigDvrProperty &sdev_property);
int BizConfigGetTimeParam(SConfigTimeParam &stimme_param);
int BizConfigGetNetParam(SConfigNetParam &snet_param);

int BizConfigGetTvWallList(std::vector<SConfigTvWallParam> &vtvwall_list);
int BizConfigGetNvrList(std::vector<uint> &vdev_list);
int BizConfigGetPatrolDecList(std::vector<uint> &vdev_list);
int BizConfigGetSwitchDecList(std::vector<uint> &vdev_list);
int BizConfigGetAlmLinkList(std::vector<SConfigAlmLinkParam> &valm_link_list);

//���ò���
int BizConfigSetTimeParam(SConfigTimeParam &stimme_param);
int BizConfigSetNetParam(SConfigNetParam &snet_param);


//��ӡ�ɾ��һ������
//�޸�ֻ����һ��
int BizConfigAddTvWallParam(SConfigTvWallParam &tvwall_param);
int BizConfigAddTvWallParamList(std::vector<SConfigTvWallParam> &vtvwall_list);
int BizConfigDelTvWallParam(SConfigTvWallParam &tvwall_param);
int BizConfigDelTvWallParamList(std::vector<SConfigTvWallParam> &vtvwall_list);
int BizConfigDelAllTvWallParamList();
int BizConfigModifyTvWallParam(u32 index, SConfigTvWallParam &stvwall_param);

int BizConfigAddAlmLinkParam(SConfigAlmLinkParam &alm_link_param);
int BizConfigAddAlmLinkParamList(std::vector<SConfigAlmLinkParam> &valm_link_list);
int BizConfigDelAlmLinkParam(SConfigAlmLinkParam &alm_link_param);
int BizConfigDelAlmLinkParamList(std::vector<SConfigAlmLinkParam> &valm_link_list);
int BizConfigDelAllAlmLinkParamList();
int BizConfigModifyAlmLinkParam(u32 index, SConfigAlmLinkParam &salm_link_param);


int BizConfigAddNvr(uint dev_ip);
int BizConfigDelNvr(uint dev_ip);
int BizConfigAddNvrList(std::vector<uint> &vdev_list);//nvr IP list
int BizConfigDelNvrList(std::vector<uint> &vdev_list);
int BizConfigDelAllNvrList();

int BizConfigAddPatrolDec(uint dev_ip);
int BizConfigDelPatrolDec(uint dev_ip);
int BizConfigAddPatrolDecList(std::vector<uint> &vdev_list);
int BizConfigDelPatrolDecList(std::vector<uint> &vdev_list);
int BizConfigDelAllPatrolDecList();

int BizConfigAddSwitchDec(uint dev_ip);
int BizConfigDelSwitchDec(uint dev_ip);
int BizConfigAddSwitchDecList(std::vector<uint> &vdev_list);
int BizConfigDelSwitchDecList(std::vector<uint> &vdev_list);
int BizConfigDelAllSwitchDecList(std::vector<uint> &vdev_list);


#ifdef __cplusplus
}
#endif

#endif
