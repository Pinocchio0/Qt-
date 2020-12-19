#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QFileDialog>
#include <QMediaPlayer>
#include <QMediaPlaylist>
#include <QVideoWidget>
#include <QPalette>
#include <QDesktopWidget>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QFrame>
#include <QString>
#include <QAction>
#include <QTreeWidget>
#include <QDebug>
#include <stdio.h>
#include <iostream>
#include <time.h>
#include <unistd.h>
#include <QVector>
#include <QFile>
#include <QThread>
#include <QMenu>
#include "tree.h"
#include "incCn/HCNetSDK.h"
#include "addnewdevice.h"
#include "multiplayer.h"
#include "reaname.h"
#include "incCn/PlayM4.h"
#include "incCn/plaympeg4.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT
    friend class addnewdevice;

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    addnewdevice * adddevicewindow = NULL;
    multiplayer * player = NULL;
    reaname * renamewindow = NULL;

private slots:
    void on_FullScr_clicked(bool checked);
    void on_AddDeviceBtn_clicked(bool checked);
    void checkself(QTreeWidgetItem *, int);
    void deleteItem();
    void rename();
    void popMenu(const QPoint&);
    void closeplayer1();
    void popClose1(const QPoint&);
    void closeplayer2();
    void popClose2(const QPoint&);
    void closeplayer3();
    void popClose3(const QPoint&);
    void closeplayer4();
    void popClose4(const QPoint&);
    void getsername(char * sername, QTreeWidgetItem * hItem);
    void mousePressEvent(QMouseEvent * e);
    void keyPressEvent(QKeyEvent * event);
    void mouseDoubleClickEvent(QMouseEvent * e);

private:
    Ui::MainWindow *ui;

    //用户名和实时流
    LONG lRealPlayHandle[4] = {-1, -1, -1, -1};
    LONG lUserID[4] = {-1, -1, -1, -1};

    //总窗口数
    int SUBPLAYER_NUM = 4;

    //设备名和IP容器
    QVector<QVector<QString>> NameAAddr;
    QVector<QString> A;

    //鼠标双击锁, 0表示解锁，1表示上锁
    int mouseDoubleClicklock = 0;

    //窗口模式, 0表示4分屏模式，1表示最大窗口模式
    int Mode = 0;

    //调色盘
    QPalette pal;
};
#endif // MAINWINDOW_H
