#include <QtGui>
#include <QtDebug>
#include "bond.h"
#include "page_main.h"
#include "page_manager.h"
#include "ui_page_main.h"

#include "page_config.h"
#include "page_dev_mgt.h"
#include "page_alm.h"
#include "page_tvwall.h"
#include "page_playback.h"
#include "page_preview.h"

#include "types.h"
#include "biz_preview.h"
#include "biz_config.h"

page_main::page_main(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::page_main)
{
    ui->setupUi(this);
    this->init_style();
    this->init_form();
    //this->setGeometry(qApp->desktop()->availableGeometry());
}

page_main::~page_main()
{
    delete ui;
}

//初始化无边框窗体
void page_main::init_style()
{
    this->setWindowFlags(Qt::FramelessWindowHint | Qt::WindowSystemMenuHint | Qt::WindowMinimizeButtonHint);
    //this->setAttribute(Qt::WA_TranslucentBackground, true);
    /*
    QPalette pal = palette();
    pal.setColor(QPalette::Background, QColor(0x00,0xff,0x00,0x00));
    setPalette(pal);
    */

    //ui->lab_title->setText(tr("网络键盘"));
    //ui->lab_title->setFont(QFont("wenquanyi", 25));
    //ui->lab_title->adjustSize();
    //ui->lab_date->setFont(QFont("wenquanyi", 16));
    //ui->lab_time->setFont(QFont("wenquanyi", 16));

    QPixmap map(QString::fromUtf8(":/banner/logo.bmp"));
    ui->lab_ico->setPixmap(map);
    ui->lab_ico->setMinimumWidth(map.width());
}

void page_main::init_form() //init stackwidget
{
    ndate_format = 0;
    ntime_format = 0;

    SConfigTimeParam stime_param;
    int b_ret = BizConfigGetTimeParam(stime_param);
    if (b_ret)
    {
        qDebug("%s BizGetTimeParam failed\n", __func__);
        //return 1;
    }
    else
    {
        ndate_format = stime_param.ndate_format;
        ntime_format = stime_param.ntime_format;
    }
    //qDebug() << "123" <<endl;
    QList<QToolButton *> btns = ui->widget_top->findChildren<QToolButton *>();

    foreach (QToolButton * btn, btns) {
        connect(btn, SIGNAL(clicked()), this, SLOT(button_clicked()));
    }

    page_preview *preview = new page_preview;
    ui->stackedWidget->addWidget(preview);
    if (registerPage(PAGE_PREVIEW, preview))
    {
        ERR_PRINT("registerPage PAGE_PREVIEW failed\n");
        return ;
    }

    page_playback *playback = new page_playback;
    ui->stackedWidget->addWidget(playback);
    if (registerPage(PAGE_PLAYBACK, playback))
    {
        ERR_PRINT("registerPage PAGE_PLAYBACK failed\n");
        return ;
    }

    page_tvWall *tvWall = new page_tvWall;
    ui->stackedWidget->addWidget(tvWall);
    if (registerPage(PAGE_TVWALL, tvWall))
    {
        ERR_PRINT("registerPage PAGE_TVWALL failed\n");
        return ;
    }

    page_alm *alm = new page_alm;
    ui->stackedWidget->addWidget(alm);
    if (registerPage(PAGE_ALM, tvWall))
    {
        ERR_PRINT("registerPage PAGE_ALM failed\n");
        return ;
    }

    page_dev_mgt *dev_mgt = new page_dev_mgt;
    ui->stackedWidget->addWidget(dev_mgt);
    if (registerPage(PAGE_DEV_MGT, tvWall))
    {
        ERR_PRINT("registerPage PAGE_DEV_MGT failed\n");
        return ;
    }

    page_config *config = new page_config;
    ui->stackedWidget->addWidget(config);
    if (registerPage(PAGE_CONFIG, tvWall))
    {
        ERR_PRINT("registerPage PAGE_CONFIG failed\n");
        return ;
    }

    qRegisterMetaType<SDateTime>("SDateTime");
    connect(gp_bond, SIGNAL(signalUpdateTime(SDateTime)), this, SLOT(update_time(SDateTime)), Qt::QueuedConnection);

    ui->btn_tvWall->click();
}

void page_main::button_clicked()
{
    QToolButton *btn = (QToolButton *)sender();
    QString name = btn->objectName();

    QList<QToolButton *> btns = ui->widget_top->findChildren<QToolButton *>();
    foreach (QToolButton * b, btns) {
        b->setChecked(false);
    }

    if (ui->btn_preview->objectName() == name)
    {
        ui->stackedWidget->setCurrentIndex(0);

        ui->btn_preview->setChecked(true);
    /*
        if (BizPreviewStart((unsigned int)4244744384, 0, 1))
        {
            ERR_PRINT("BizPreviewStart failed\n");
        }
    */
    }
    else if (ui->btn_playback->objectName() == name)
    {
        ui->stackedWidget->setCurrentIndex(1);

        ui->btn_playback->setChecked(true);
    }
    else if (ui->btn_tvWall->objectName() == name)
    {
        ui->stackedWidget->setCurrentIndex(2);

        ui->btn_tvWall->setChecked(true);
    }
    else if (ui->btn_alarm->objectName() == name)
    {
        ui->stackedWidget->setCurrentIndex(3);

        ui->btn_alarm->setChecked(true);
    }
    else if (ui->btn_device->objectName() == name)
    {
        ui->stackedWidget->setCurrentIndex(4);

        ui->btn_device->setChecked(true);
    }
    else if (ui->btn_config->objectName() == name)
    {
        ui->stackedWidget->setCurrentIndex(5);

        ui->btn_config->setChecked(true);
    }
}

void page_main::setTimeFormat(u8 date_format, u8 time_format)
{
    ndate_format = date_format;
    ntime_format = time_format;
}

void page_main::update_time(SDateTime dt)
{
    QDate qd(dt.nYear, dt.nMonth, dt.nDay);
    switch (ndate_format)
    {
        case 0: //年-月-日
            ui->lab_date->setText(qd.toString("yyyy年MM月dd日"));
        break;
        case 1: //月-日-年
            ui->lab_date->setText(qd.toString("MM月dd日yyyy年"));
        break;
        case 2: //日-月-年
            ui->lab_date->setText(qd.toString("dd日MM月yyyy年"));
        break;
        default:    //年-月-日
            ui->lab_date->setText(qd.toString("yyyy年MM月dd日"));
    }


    QTime qt(dt.nHour, dt.nMinute, dt.nSecode);
    //ui->lab_time->setText(qt.toString("hh:mm:ss"));
    if (ntime_format == 0)//24小时制
    {
        ui->lab_time->setText(qt.toString("hh:mm:ss"));
    }
    else if (ntime_format == 1)//12小时制
    {
        ui->lab_time->setText(qt.toString("AP hh:mm:ss"));
    }
    else    //默认24小时制
    {
        ui->lab_time->setText(qt.toString("hh:mm:ss"));
    }
}

#if 0
void page_main::mouseMoveEvent(QMouseEvent *e)
{
    if (mousePressed && (e->buttons() && Qt::LeftButton)) {
        this->move(e->globalPos() - mousePoint);
        e->accept();
    }
}

void page_main::mousePressEvent(QMouseEvent *e)
{
    qDebug() << "mousePressEvent" <<endl;
    if (e->button() == Qt::LeftButton) {
        mousePressed = true;
        mousePoint = e->globalPos() - this->pos();
        e->accept();
    }
}

void page_main::mouseReleaseEvent(QMouseEvent *)
{
    mousePressed = false;
}
#endif
