#ifndef __BIZ_TYPES_H__
#define __BIZ_TYPES_H__

#include "types.h"


//ͼ���������
#define  MEDIA_TYPE_H264		(BYTE)98//H.264//������109?
#define  MEDIA_TYPE_MP4			(BYTE)97//MPEG-4
#define  MEDIA_TYPE_H261		(BYTE)31//H.261
#define  MEDIA_TYPE_H263		(BYTE)34//H.263
#define  MEDIA_TYPE_MJPEG		(BYTE)26//Motion JPEG
#define  MEDIA_TYPE_MP2			(BYTE)33//MPEG2 video

//������������
#define	 MEDIA_TYPE_MP3			(BYTE)96//mp3
#define  MEDIA_TYPE_PCMU		(BYTE)0//G.711 ulaw
#define  MEDIA_TYPE_PCMA		(BYTE)8//G.711 Alaw
#define	 MEDIA_TYPE_G7231		(BYTE)4//G.7231
#define	 MEDIA_TYPE_G722		(BYTE)9//G.722
#define	 MEDIA_TYPE_G728		(BYTE)15//G.728
#define	 MEDIA_TYPE_G729		(BYTE)18//G.729
#define	 MEDIA_TYPE_RAWAUDIO	(BYTE)19//raw audio
#define  MEDIA_TYPE_ADPCM		(BYTE)20//adpcm
#define  MEDIA_TYPE_ADPCM_HISI	(BYTE)21//adpcm-hisi
// #endif//MEDIA_TYPE


//ҵ�����Ϣ����
typedef enum
{
	//������Ӧ�Ĳ���
	//BIZ CONFIG
	EM_BIZCONFIG_TIME_DATE_PARAM,
	EM_BIZCONFIG_NET_PARAM,
	EM_BIZCONFIG_TVWALL_LIST,
	EM_BIZCONFIG_NVR_LIST,
	EM_BIZCONFIG_PATROL_DEC_LIST,
	EM_BIZCONFIG_SWITCH_DEC_LIST,
	EM_BIZCONFIG_ALARM_LINK_LIST,
	EM_BIZCONFIG_ALL_FILE,//�����ļ�
	
	//��������ı�
	EM_NET_PARAM_CHANGE,
	
	//�������ⲿ��Ҫ��ͣ�����
	EM_THREAD_PAUSE,
	EM_THREAD_RESUME,
	EM_MSG_TYPE_MAX,
} EM_MSG_TYPE;



//ҵ���ص�
//param used by SBizEventPara
typedef enum
{
    EM_BIZ_EVENT_UNKNOW = -1,
    EM_BIZ_EVENT_RECORD = 0,  //¼��״̬������
    EM_BIZ_EVENT_ALARM_SENSOR, //����������״̬
    EM_BIZ_EVENT_ALARM_VMOTION, //�ƶ����״̬
    EM_BIZ_EVENT_ALARM_VBLIND,  //�ڵ�״̬
    EM_BIZ_EVENT_ALARM_VLOSS, //��Ƶ��ʧ״̬
    EM_BIZ_EVENT_ALARM_OUT, //�������״̬
    EM_BIZ_EVENT_ALARM_BUZZ,  //������״̬
    EM_BIZ_EVENT_ALARM_IPCEXT,//IPC�ⲿ�����������¼�
    EM_BIZ_EVENT_ALARM_IPCCOVER,
    EM_BIZ_EVENT_ALARM_DISK_LOST,
    EM_BIZ_EVENT_ALARM_DISK_ERR,
    EM_BIZ_EVENT_ALARM_DISK_NONE,
    EM_BIZ_EVENT_ALARM_485EXT,
    

    EM_BIZ_EVENT_LOCK = 50,
    EM_BIZ_EVENT_RESTART,		//ϵͳ����nDelay������
    EM_BIZ_EVENT_POWEROFF,		//ϵͳ����nDelay��ػ�
    EM_BIZ_EVENT_POWEROFF_MANUAL,	//��ʾ�����ֶ��ػ���

    EM_BIZ_EVENT_TIMETICK = 90,
    EM_BIZ_EVENT_DATETIME_YMD,
	
    EM_BIZ_EVENT_FORMAT_INIT = 100,  //��ʽ����ʼ��
    EM_BIZ_EVENT_FORMAT_RUN, //��ʽ����
    EM_BIZ_EVENT_FORMAT_DONE, //��ʽ������

    EM_BIZ_EVENT_PLAYBACK_INIT = 150,  //�طų�ʼ��
    EM_BIZ_EVENT_PLAYBACK_START, //�طſ�ʼ
    EM_BIZ_EVENT_PLAYBACK_RUN, //�ط���
    EM_BIZ_EVENT_PLAYBACK_DONE, //�طŽ���
    EM_BIZ_EVENT_PLAYBACK_NETWORK_ERR, //�ط�ʱ�����������

    EM_BIZ_EVENT_BACKUP_INIT = 200,  //���ݳ�ʼ��
    EM_BIZ_EVENT_BACKUP_RUN, //������
    EM_BIZ_EVENT_BACKUP_DONE, //���ݽ���

    
    EM_BIZ_EVENT_UPGRADE_INIT = 250,  //������ʼ��
    EM_BIZ_EVENT_UPGRADE_RUN, //������
    EM_BIZ_EVENT_UPGRADE_DONE, //��������
    EM_BIZ_EVENT_REMOTEUP_START,//cw_remote
    EM_BIZ_EVENT_GETDMINFO,
    EM_BIZ_EVENT_SATARELOAD,//
    EM_BIZ_EVENT_DISKCHANGED,//

    EM_BIZ_EVENT_PREVIEW_REFRESH = 300,
    EM_BIZ_EVENT_LIVE_REFRESH = 301,
    EM_BIZ_EVENT_SHOWTIME_REFRESH = 302,

	EM_BIZ_EVENT_ENCODE_GETRASTER = 350,

	EM_BIZ_EVENT_NET_STATE_DHCP = 400,
	EM_BIZ_EVENT_NET_STATE_PPPOE,
	EM_BIZ_EVENT_NET_STATE_MAIL,
	EM_BIZ_EVENT_NET_STATE_DDNS,
	EM_BIZ_EVENT_NET_STATE_CONN,
	EM_BIZ_EVENT_NET_STATE_UPDATEMAINBOARDSTART,
	EM_BIZ_EVENT_NET_STATE_UPDATEPANNELSTART,
	EM_BIZ_EVENT_NET_STATE_UPDATESTARTLOGOSTART,
	EM_BIZ_EVENT_NET_STATE_UPDATEAPPLOGOSTART,
	EM_BIZ_EVENT_NET_STATE_FORMATSTART,

	EM_BIZ_EVENT_NET_STATE_DHCP_STOP = 410,
	EM_BIZ_EVENT_NET_STATE_PPPOE_STOP,
	EM_BIZ_EVENT_NET_STATE_MAIL_STOP,
	EM_BIZ_EVENT_NET_STATE_DDNS_STOP,
	EM_BIZ_EVENT_NET_STATE_CONN_STOP,

	EM_BIZ_EVENT_NET_STATE_SGUPLOAD,
	
	EM_BIZ_EVENT_NET_CHANGEPARA_RESET,
	
}EMBIZEVENT;

//֡����ͷ
typedef struct
{
    u8     m_byMediaType; //ý������
    u8    *m_pData;       //���ݻ���
	u32    m_dwPreBufSize;//m_pData����ǰԤ���˶��ٿռ䣬���ڼ�
	// RTP option��ʱ��ƫ��ָ��һ��Ϊ12+4+12
	// (FIXED HEADER + Extence option + Extence bit)
    u32    m_dwDataSize;  //m_pDataָ���ʵ�ʻ����С
    u8     m_byFrameRate; //����֡��,���ڽ��ն�
	u32    m_dwFrameID;   //֡��ʶ�����ڽ��ն�
	u32    m_dwTimeStamp; //ʱ���, ���ڽ��ն�
    union
    {
        struct{
			u32		m_bKeyFrame;    //Ƶ֡����(I or P)
			u16		m_wVideoWidth;  //��Ƶ֡��
			u16		m_wVideoHeight; //��Ƶ֡��
			u32		m_wBitRate;
		}m_tVideoParam;
        u8    m_byAudioMode;//��Ƶģʽ
    };
}FRAMEHDR,*PFRAMEHDR;


//param used by BizEventCB
typedef struct
{
	EMBIZEVENT type;
	
	union
	{
		u32 stream_id;//�ؼ���ϵͳΨһ
		u32 playback_chn;
		u32 preview_chn;
	} un_part_chn;
	
	union 
	{
		s32 stream_errno; //GLB_ERROR_NUM

		//�طš��ļ����ؽ���
		struct
		{
			u32 cur_pos;
			u32 total_size;
		} stream_progress;
		
	} un_part_data;   
} SBizEventPara;

//param used by  SBizInitPara.pfnBizEventCb
//typedef void (* FNBIZEVENTCB)(SBizEventPara* sBizEventPara); //ҵ����¼��ص�����
typedef int (* FNBIZEVENTCB)(SBizEventPara* sBizEventPara); //ҵ����¼��ص�����


//param used by BizDataCB
typedef enum
{
    EM_BIZ_DATA_UNKNOW = -1,
		
    EM_BIZ_DATA_PREVIEW = 10,	//Ԥ������
    EM_BIZ_DATA_PLAYBACK,  		//�ط�����
    EM_BIZ_DATA_DOWNLOAD,		//����
    EM_BIZ_DATA_UPGRADE,		//Զ����������
    
} EMBIZDATA;


typedef struct
{
	EMBIZDATA type;
	
	union
	{
		u32 stream_id;//�ؼ���ϵͳΨһ
		u32 playback_chn;
		u32 preview_chn;
	} un_part_chn;
	
	union
	{
		FRAMEHDR *pframe_hdr;
		
	} un_part_data;   
} SBizDataPara;


#endif

