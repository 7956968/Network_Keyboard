#include "bond.h"

Cbond *Cbond::_instance = 0;
int Cbond::b_inited = 0;

void Cbond::bondNotifyUpdateTime(SDateTime *pdt)
{
    SDateTime dt = *pdt;

    if (b_inited)
    {
        emit signalNotifyUpdateTime(dt);
    }
}

void Cbond::bondNotifyDevInfo(SGuiDev *pdev)
{
	SGuiDev dev = *pdev;
	
	if (b_inited)
    {
        emit signalNotifyDevInfo(dev);
    }
}




//extern function
int notifyGuiUpdateTime(SDateTime *pdt)
{
    gp_bond->bondNotifyUpdateTime(pdt);
    return 0;
}

int notifyDevInfo(SGuiDev *pdev)//�豸�㽫��Ϣ֪ͨ���ϲ�
{
	gp_bond->bondNotifyDevInfo(pdev);
    return 0;
}


