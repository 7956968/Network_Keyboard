#ifndef __GLB_ERROR_NUM_H__
#define __GLB_ERROR_NUM_H__

enum GLB_ERROR_NUM
{
	SUCCESS = 0,
	FAILURE,/* ������޷������Ĵ��� */
	EPARAM,	/* �������� */
	/* ����ͨ�� */
	ESTREAM_ID_CONFLICT, /* stream ID ��ͻ */
	ETIMEOUT,
	ESEND,
	ERECV,
	
	/* CcircularBuffer */
	ESPACE,	/* ʣ��ռ䲻�� */
	EDATA,	/* ��Ч���ݲ��� */

	/* biz_device */
	EIP_CONFLICT, /* IP ��ͻ */
	EDEV_NOTFOUND,	/* �豸δ�ҵ� */
	EDEV_OFFLINE,	/* �豸���� */
	EDEVS_LIMIT, /* �豸��Ŀ�������� */

	ESYS_MODE,
};


#endif

