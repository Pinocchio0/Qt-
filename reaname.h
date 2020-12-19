#ifndef REANAME_H
#define REANAME_H

#include <QWidget>
#include <QDesktopWidget>
#include <QDialog>
#include <QString>
#include <QMessageBox>
#include <QFile>
#include <QDebug>

namespace Ui {
class reaname;
}

class reaname : public QDialog
{
    Q_OBJECT

public:
    explicit reaname(QWidget *parent = nullptr);
    ~reaname();

    //设备重命名
    QString DeviceRename;

    //命名修改确定
    QString DeviceRenamefinal;

private slots:
    void on_RenameExit_clicked(bool checked);
    void on_RenameCertain_clicked(bool checked);

private:
    Ui::reaname *ui;
};

#endif // REANAME_H
