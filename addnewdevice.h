#ifndef ADDNEWDEVICE_H
#define ADDNEWDEVICE_H

#include <QWidget>
#include <QDialog>
#include <QDesktopWidget>
#include <QMessageBox>
#include <QString>
#include <QFile>
#include <QDebug>
#include "incCn/HCNetSDK.h"

namespace Ui {
class addnewdevice;
}

class addnewdevice : public QDialog
{
    Q_OBJECT

public:
    explicit addnewdevice(QWidget *parent = nullptr);
    ~addnewdevice();

    // 注册设备(用户ID)临时
    LONG UserID = -1;

    //用户名、设备名、设备密码、ip地址临时
    QString IPaddr;
    QString Devicename;
    QString username;
    QString passward;

    //用户名、设备名、设备密码、ip地址确定
    QString IPaddrfinal;
    QString Devicenamefinal;
    QString usernamefinal;
    QString passwardfinal;

private slots:
    void on_ExitBtn_clicked(bool checked);
    void on_AddBtn_clicked(bool checked);

private:
    Ui::addnewdevice *ui;
};

#endif // ADDNEWDEVICE_H
