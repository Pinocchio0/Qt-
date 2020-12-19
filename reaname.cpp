#include "reaname.h"
#include "ui_reaname.h"

//构造
reaname::reaname(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::reaname)
{
    ui->setupUi(this);

    //移动到程序中心
    QDesktopWidget *desktop = QApplication::desktop();
    this->move((desktop->width() - this->width()) / 2, (desktop->height() - this->height()) / 2);

    //设置标题
    this->setWindowTitle("添加设备窗口");

    //固定窗口大小
    this->setFixedSize(300, 150);
}

//析构
reaname::~reaname()
{
    delete ui;
}

//退出重命名
void reaname::on_RenameExit_clicked(bool checked)
{
    this->close();
}

//设备重命名
void reaname::on_RenameCertain_clicked(bool checked)
{
    DeviceRename = this->ui->rename->text();
    if(DeviceRename.isEmpty())
    {
        //创建消息,输入不能为空
        QMessageBox::information(this, "信息", "输入不能为空！");
        this->ui->rename->setText("");
        DeviceRename.clear();
        return;
    }

    //比较过往设备数据,防重名
    QFile file("DeviceMessage");
    if(!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        qDebug() << "on_RenameCertain_clicked-file.open error or not exist!";
    }
    else
    {
        while(!file.atEnd())
        {
            //比较自定义设备名
            QString data = file.readLine();
            data.chop(1);
            if(data == DeviceRename)
            {
                //创建消息,设备重名
                QMessageBox::information(this, "信息", "设备名称重名，重新输入修改命名！");
                this->ui->rename->setText("");
                DeviceRename.clear();
                return;
            }
            data = file.readLine();
            data = file.readLine();
            data = file.readLine();
        }
        file.close();
    }

    //命名修改确定
    DeviceRenamefinal = DeviceRename;

    this->close();
}
