#ifndef __BIZ_H__
#define __BIZ_H__

#include "types.h"
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

int BizInit(void);

//���ò���
int BizSetNetParam(SConfigNetParam &snet_param);


int BizEventCB(SBizEventPara* pSBizEventPara);

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

//��������������¼�֪ͨ
void BizDealSvrNotify(u32 svr_ip, u16 event, s8 *pbyMsgBuf, int msgLen);


#ifdef __cplusplus
}
#endif

#endif

