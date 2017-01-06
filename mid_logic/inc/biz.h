#ifndef __BIZ_H__
#define __BIZ_H__

#include "types.h"
#include "biz_msg_type.h"
#include "biz_types.h"
#include "biz_config.h"

#include "ctrlprotocol.h"


/*******************************************
���ϲ���õĽӿڶ�����������
��bond ģ�����
*******************************************/
#ifdef __cplusplus
extern "C" {
#endif

int BizSendMsg2BizManager(SBizMsg_t *pmsg, u32 msg_len);


//��ѯgui �Ƿ�׼���ý���֪ͨ��Ϣ
VD_BOOL BizGuiIsReady();

int BizEventCB(SBizEventPara* pSBizEventPara);
int BizDataCB(SBizDataPara* SBizDataPara);


//��������ͻ�������(��Ӧ�ͻ�������)
u16 BizDealClientCmd(
	CPHandle 	cph,
	u16 		event,
	char*		pbyMsgBuf,
	int 		msgLen,
	char*		pbyAckBuf,
	int*		pAckLen );

//��������ͻ�����������
int BizDealClientDataLink(
	int sock, 
	ifly_TCP_Stream_Req *preq_hdr, 
	struct sockaddr_in *pcli_addr_in);

#if 0 // ��ʱδ��
//��������������¼�֪ͨ
void BizDealSvrNotify(u32 dev_ip, u16 event, s8 *pbyMsgBuf, int msgLen);
#endif


#ifdef __cplusplus
}
#endif

#endif

