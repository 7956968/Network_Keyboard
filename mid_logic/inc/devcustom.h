#ifndef __DEVCUSTOM_H__
#define __DEVCUSTOM_H__

//#define DVR_CUSTOM_PATH  "/tmp/data/cfg/DevCustom.ini"
#define DVR_CUSTOM_PATH  "/mnt/NetworkKeyboad/DevCustom.ini"


//���ñ���
#define DVR_CONFIG_INFO 			"DevCfg"			
#define DVR_CONFIG_INFO_SAVETYPE	"CfgWay"			//���ñ��淽ʽ(0: flash, 1: file)
#define DVR_CONFIG_INFO_FORMAT		"CfgFormat"			//���ñ����ʽ(0=Struct;1=Ini;2=XML)
#define DVR_CONFIG_INFO_FLASHDEV	"CfgFlashDev"		//flash�豸
#define DVR_CONFIG_INFO_DEFAULTPATH	"CfgPathDefault"	//Ĭ�������ļ�·��
#define DVR_CONFIG_INFO_FILEPATH	"CfgPath"			//�����ļ�·��
#define DVR_CONFIG_INFO_ZIP			"CfgZip"			//ѹ����ʽ(0: ��ѹ��, 1: zipѹ��)
#define DVR_CONFIG_INFO_OFFSET		"Offset"			//��flash�豸�е�ƫ�Ƶ�ַ
#define DVR_CONFIG_INFO_LENGTH		"Length"			//���������С����
//

//DVR����
#define DVR_PROPERTY				"DevProperty"		
#define DVR_PROPERTY_VERSION		"Version"			//�汾��
#define DVR_PROPERTY_MODEL			"Model"				//�ͺ�
#define DVR_PROPERTY_PRODUCTNUMBER	"Productnumber"		//��Ʒ��
#define DVR_PROPERTY_PREVIEWNUMS	"PreviewNums"		//Ԥ��ͨ����
#define DVR_PROPERTY_SVRCONNECTNUMS "SvrMaxConnectNums"	//��Ϊ��������tcp ���������
#define DVR_PROPERTY_TVWALLNUMS		"TvWallNums"		//����ǽ���ñ���������
#define DVR_PROPERTY_TVWALLDEVSMAX	"TvwallDevsMax"		//����ǽ���豸���������ֵ
#define DVR_PROPERTY_TVWALLLINE		"TvWallLine"		//����ǽÿ���豸��������
#define DVR_PROPERTY_TVWALLCOL		"TvWallCol"			//����ǽÿ���豸��������
#define DVR_PROPERTY_NVRNUMS		"NvrNums"			//����NVR��������
#define DVR_PROPERTY_DECNUMS		"DecNums"			//����DEC��������

//ʱ�����
#define CONFIG_TIME_PARAM				"TimeDate"		//
#define CONFIG_TIME_PARAM_DATA_FORMAT	"DateFormat"	//
#define CONFIG_TIME_PARAM_TIME_FORMAT	"TimeFormat"	//
#define CONFIG_TIME_PARAM_TIME_SYNC		"TimeSync"		//
#define CONFIG_TIME_PARAM_NTPSVR_IP		"NtpServerAdress"	//

//�������
#define CONFIG_NET_PARAM 			"NetWork"
#define CONFIG_NET_PARAM_HOSTIP		"HostIP"
#define CONFIG_NET_PARAM_SUBMASK	"Submask"
#define CONFIG_NET_PARAM_GATEWAY	"GateWayIP"
#define CONFIG_NET_PARAM_DNS1		"DNSIP1"
#define CONFIG_NET_PARAM_DNS2		"DNSIP2"
#define CONFIG_NET_PARAM_SVRPORT	"TCPPort"
#define CONFIG_NET_PARAM_MAC		"Mac"


//����ǽ����
#define CONFIG_TVWALL_PARAM					"TvWall"
#define CONFIG_TVWALL_PARAM_DEVS_PER_LINE	"DevsPerLine"
#define CONFIG_TVWALL_PARAM_DEVS_PER_COL	"DevsPerCol"
#define CONFIG_TVWALL_PARAM_DEVLIST			"DevList"		//�豸�б�
#define CONFIG_TVWALL_PARAM_NUMS			"Nums"			//��ǰ��¼�˼��ݵ���ǽ�б�


#define CONFIG_DEVLIST_PARAM_NUMS		"Nums"
#define CONFIG_DEVLIST_PARAM_DEVLIST	"DevList"
//NVR �б�
#define CONFIG_NVRLIST_PARAM			"NvrList"

//��Ѳ��DEC �б�
#define CONFIG_PATROL_DECLIST_PARAM		"patrolDecList"

//�л���DEC �б�
#define CONFIG_SWITCH_DECLIST_PARAM		"SwitchDecList"



//���������б�
#define CONFIG_ALARM_LINK_LIST_PARAM			"AlmLinkList"
#define CONFIG_ALARM_LINK_LIST_PARAM_NUMS		"Nums"
#define CONFIG_ALARM_LINK_LIST_PARAM_DEVLIST	"Item"
enum
{
	EM_CONFIG_ALARM_LINK_LIST_PARAM_NVRIP_U32,
	EM_CONFIG_ALARM_LINK_LIST_PARAM_ALM_TYPE_U32,
	EM_CONFIG_ALARM_LINK_LIST_PARAM_ALM_CHN_U32,
	EM_CONFIG_ALARM_LINK_LIST_PARAM_DECIP_U32,
	EM_CONFIG_ALARM_LINK_LIST_PARAM_DEC_CHN_U32,
	EM_CONFIG_ALARM_LINK_LIST_PARAM_MAX,
};



#endif
