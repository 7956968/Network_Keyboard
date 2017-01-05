#ifndef __BIZ_MSG_TYPE_H__
#define __BIZ_MSG_TYPE_H__

typedef enum
{
	
	//CMediaStream
	EM_STREAM_MSG_CONNECT_SUCCESS,	//���ӳɹ�
	EM_STREAM_MSG_CONNECT_FALIURE,	//����ʧ��
	EM_STREAM_MSG_STOP,			//���رգ����ܳ���
	EM_STREAM_MSG_PROGRESS,		//�ļ��ط�/���ؽ���
	EM_STREAM_MSG_FINISH,		//�ļ��������
	
	//CMediaStreamManager �ڲ�����
	EM_STREAM_CMD_CONNECT,	//������
	EM_STREAM_CMD_DEL,		//ɾ����

	//playback
	EM_PLAYBACK_MSG_CONNECT_SUCCESS,	//���ӳɹ�
	EM_PLAYBACK_MSG_CONNECT_FALIURE,	//����ʧ��
	EM_PLAYBACK_MSG_STOP,			//���رգ����ܳ���
	EM_PLAYBACK_MSG_PROGRESS,		//�ļ��ط�/���ؽ���
	EM_PLAYBACK_MSG_FINISH,		//�ļ��ط����

	//preview
	EM_PREVIEW_MSG_CONNECT_SUCCESS,	//���ӳɹ�
	EM_PREVIEW_MSG_CONNECT_FALIURE,	//����ʧ��
	EM_PREVIEW_MSG_STOP,			//���رգ����ܳ���
	EM_PREVIEW_MSG_PROGRESS,		//�ļ��ط�/���ؽ���

	//file download
	EM_FILE_DOWNLOAD_MSG_CONNECT_SUCCESS,	//���ӳɹ�
	EM_FILE_DOWNLOAD_MSG_CONNECT_FALIURE,	//����ʧ��
	EM_FILE_DOWNLOAD_MSG_STOP,			//���رգ����ܳ���
	EM_FILE_DOWNLOAD_MSG_PROGRESS,		//�ļ��ط���NVR �˷��������غ����������ɱ��ؽ��ս��̷���
	EM_FILE_DOWNLOAD_MSG_FINISH,		//�ļ��������
	EM_FILE_CONVERT_MSG_FINISH,		//�ļ�ת��AVI ���
} EM_BIZ_MSG_TYPE;



//CMediaStreamManager ��Ϣ�ṹ

typedef struct 
{
	s32 msg_type;

	union UN_PART_CHN_T
	{
		u32 stream_id;//�ؼ���ϵͳΨһ
		u32 playback_chn;
		u32 preview_chn;
	} un_part_chn;
		
	union UN_PART_DATA_T
	{
	//��������ݽṹ
		
		//��������
		s32 stream_errno;// EM_STREAM_MSG_STOP
		
		//�طš��ļ����ؽ���
		struct
		{
			u32 cur_pos;
			u32 total_size;
		} stream_progress;
		
	} un_part_data;
}SBizMsg_t;	



#endif

