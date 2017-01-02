#ifndef __BIZ_MSG_TYPE_H__
#define __BIZ_MSG_TYPE_H__

typedef enum
{
	
	//�������Ϣ
	EM_STREAM_MSG_CONNECT_SUCCESS,	//���ӳɹ�
	EM_STREAM_MSG_CONNECT_FALIURE,	//����ʧ��
	EM_STREAM_MSG_DEL_SUCCESS,		//ɾ�����ɹ�
	EM_STREAM_MSG_DEL_FALIURE,		//ɾ����ʧ��
	EM_STREAM_MSG_ERR,		//������
	EM_STREAM_MSG_STOP,			//���ر�
	EM_STREAM_MSG_PROGRESS,		//�ļ��ط�/���ؽ���
	EM_STREAM_MSG_FINISH,		//�ļ��������
		//CMediaStreamManager �ڲ�����
	EM_STREAM_CMD_CONNECT,	//������
	EM_STREAM_CMD_DEL,		//ɾ����
} EM_BIZ_MSG_TYPE;



//CMediaStreamManager ��Ϣ�ṹ

typedef struct 
{
	s32 msg_type;
		
	union		//72byte
	{
	//��������ݽṹ
		//
		u32 stream_id;//�ؼ���ϵͳΨһ
		
		//��������
		struct
		{
			u32 stream_id;//�ؼ���ϵͳΨһ
			s32 stream_errno;//GLB_ERROR_NUM
		} stream_err;
		
		//�طš��ļ����ؽ���
		struct
		{
			u32 stream_id;//�ؼ���ϵͳΨһ
			u32 cur_pos;
			u32 total_size;
		} stream_progress;						
		
		
	};
}SBizMsg_t;	



#endif

