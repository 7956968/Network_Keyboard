#ifndef __BOND_H__
#define __BOND_H__

#include "biz.h"
#include "biz_config.h"
#include "biz_system_complex.h"

#include "gui_dev.h"

/**********************************************************/
#ifdef GUI_BOND_CLASS_DECLARE   //仅 gui 部分需要看到类的声明

#include <QObject>
#include <QMutex>

class Cbond: public QObject
{
    Q_OBJECT

public:
    static Cbond *Instance()
    {
        static QMutex mutex;
        if (!_instance)
        {
            QMutexLocker locker(&mutex);
            if (!_instance)
            {
                _instance = new Cbond;
            }
        }
        return _instance;
    }

    ~Cbond() {}

    void bondNotifyUpdateTime(SDateTime *pdt);
	void bondNotifyDevInfo(SGuiDev *pdev);

public:
    static int b_inited;//main.cpp set

private:
    explicit Cbond(QObject *parent = 0) {}

private:
    static Cbond *_instance;

signals:
    void signalNotifyUpdateTime(SDateTime dt);
	void signalNotifyDevInfo(SGuiDev dev);
};

#define gp_bond (Cbond::Instance())

#endif

/**********************************************************/

//extern function
#ifdef __cplusplus
extern "C" {
#endif

int notifyGuiUpdateTime(SDateTime *pdt);

int notifyDevInfo(SGuiDev *pdev); //�豸�㽫��Ϣ֪ͨ���ϲ�


#ifdef __cplusplus
}
#endif

#endif
