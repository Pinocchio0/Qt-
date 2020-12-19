#include "addnewdevice.h"
#include "ui_addnewdevice.h"

//构造
addnewdevice::addnewdevice(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::addnewdevice)
{
    ui->setupUi(this);

    //移动到程序中心
    QDesktopWidget *desktop = QApplication::desktop();
    this->move((desktop->width() - this->width()) / 2, (desktop->height() - this->height()) / 2);

    //设置标题
    this->setWindowTitle("添加设备窗口");

    //固定窗口大小
    this->setFixedSize(400, 300);

    // 初始化
    NET_DVR_Init();
}

//析构
addnewdevice::~addnewdevice()
{
    delete ui;
}

//退出添加
void addnewdevice::on_ExitBtn_clicked(bool checked)
{
    this->close();
}

//添加
void addnewdevice::on_AddBtn_clicked(bool checked)
{
    //提示
    this->ui->wait->setText("正在添加设备。。。");
    this->ui->wait->repaint();

    //初始化清零
    IPaddr.clear();
    Devicename.clear();
    username.clear();
    passward.clear();

    IPaddr = this->ui->IpAddress->text();
    Devicename = this->ui->DeviceName->text();
    username = this->ui->UserName->text();
    passward = this->ui->DevicePassward->text();

    if(IPaddr.isEmpty() || Devicename.isEmpty() || username.isEmpty() || passward.isEmpty())
    {
        //创建消息,输入信息不完整
        this->ui->wait->setText("");
        QMessageBox::information(this, "信息", "输入信息不完整，请重新输入！");
        this->ui->IpAddress->setText("192.168.");
        this->ui->DeviceName->setText("");
        this->ui->DevicePassward->setText("");
        this->ui->UserName->setText("admin");
        IPaddr.clear();
        Devicename.clear();
        username.clear();
        passward.clear();
        return;
    }

    //QString to char *
    std::string str = IPaddr.toStdString();
    const char * addressstr = str.c_str();

    std::string str1 = username.toStdString();
    const char * usernamestr = str1.c_str();

    std::string str2 = passward.toStdString();
    const char * passwardstr = str2.c_str();

    //登录参数，包括设备地址、登录用户、密码等
    NET_DVR_USER_LOGIN_INFO struLoginInfo = {0};
    struLoginInfo.bUseAsynLogin = 0; //同步登录方式
    strcpy(struLoginInfo.sDeviceAddress, addressstr); //设备IP地址
    struLoginInfo.wPort = 8000; //设备服务端口
    strcpy(struLoginInfo.sUserName, usernamestr); //设备登录用户名
    strcpy(struLoginInfo.sPassword, passwardstr); //设备登录密码

    //登陆设备，输出设备信息以及用户ID
    NET_DVR_DEVICEINFO_V40 struDeviceInfoV40 = {0};//获取信息

    UserID = NET_DVR_Login_V40(&struLoginInfo, &struDeviceInfoV40);
    if(UserID < 0)
    {
        if(1 == NET_DVR_GetLastError())
        {
            //创建消息,用户名密码错误
            this->ui->wait->setText("");
            QMessageBox::information(this, "信息", "用户名或密码错误，请重新输入！");
            this->ui->DevicePassward->setText("");
            IPaddr.clear();
            Devicename.clear();
            username.clear();
            passward.clear();
            return;
        }
        else if(7 == NET_DVR_GetLastError())
        {
            //创建消息,设备不在线
            this->ui->wait->setText("");
            QMessageBox::information(this, "信息", "设备不在线!");
            this->ui->IpAddress->setText("192.168.");
            this->ui->DevicePassward->setText("");
            this->ui->UserName->setText("admin");
            IPaddr.clear();
            Devicename.clear();
            username.clear();
            passward.clear();
            return;
        }
        else if(17 == NET_DVR_GetLastError())
        {
            //创建消息,输入错误
            this->ui->wait->setText("");
            QMessageBox::information(this, "信息", "输入IP格式错误，请重新输入！");
            this->ui->IpAddress->setText("192.168.");
            this->ui->DeviceName->setText("");
            this->ui->DevicePassward->setText("");
            this->ui->UserName->setText("admin");
            IPaddr.clear();
            Devicename.clear();
            username.clear();
            passward.clear();
            return;
        }
        else
        {
            //创建消息,添加异常
            this->ui->wait->setText("");
            QString list = QString("添加异常,错误码：%1").arg(NET_DVR_GetLastError());
            QMessageBox::information(this, "信息", list);
            qDebug() << "on_AddBtn_clicked-NET_DVR_Login_V40 failed, error code: " << NET_DVR_GetLastError();
            this->ui->IpAddress->setText("192.168.");
            this->ui->DeviceName->setText("");
            this->ui->DevicePassward->setText("");
            this->ui->UserName->setText("admin");
            IPaddr.clear();
            Devicename.clear();
            username.clear();
            passward.clear();
            NET_DVR_Cleanup();
            return;
        }
    }

    //比较过往设备数据,，防重名
    QFile file("DeviceMessage");
    if(!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        qDebug() << "on_AddBtn_clicked-file.open error or not exist!";
    }
    else
    {
        while(!file.atEnd())
        {
            //比较自定义设备名
            QString data = file.readLine();
            data.chop(1);
            if(data == Devicename)
            {
                //创建消息,设备重名
                this->ui->wait->setText("");
                QMessageBox::information(this, "信息", "设备名称重名，请修改！");
                this->ui->DeviceName->setText("");
                this->ui->DevicePassward->setText("");
                IPaddr.clear();
                Devicename.clear();
                username.clear();
                passward.clear();
                return;
            }
            //比较IP地址
            data = file.readLine();
            data.chop(1);
            if(data == IPaddr)
            {
                //创建消息,设备IP地址已占用
                this->ui->wait->setText("");
                QMessageBox::information(this, "信息", "设备IP地址已占用，请重新输入！");
                this->ui->IpAddress->setText("192.168.");
                this->ui->DevicePassward->setText("");
                IPaddr.clear();
                Devicename.clear();
                username.clear();
                passward.clear();
                return;
            }

            data = file.readLine();
            data = file.readLine();
        }
        file.close();
    }

    //用户名、设备名、设备密码、ip地址确定
    IPaddrfinal = IPaddr;
    Devicenamefinal = Devicename;
    usernamefinal = username;
    passwardfinal = passward;

    this->close();
}
