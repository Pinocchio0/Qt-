#include "mainwindow.h"
#include "ui_mainwindow.h"

static Ui::MainWindow * Pui = NULL;  //全局ui
LONG lPort[4] = {-1, -1, -1, -1};  //输出端口port

//构造
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    //获取ui指针
    Pui = ui;

    //移动到屏幕中心
    QDesktopWidget *desktop = QApplication::desktop();
    move((desktop->width() - this->width()) / 4, (desktop->height() - this->height()) / 6);

    //固定窗口大小
    this->setFixedSize(1600,850);

    //设置主窗口标题
    this->setWindowTitle("实时监控视频播放器");

    //设置主窗口背景颜色
    pal.setColor(QPalette::Background, Qt::gray);
    this->setAutoFillBackground(true);
    setPalette(pal);

    // 初始化
    NET_DVR_Init();

    //实例化播放窗口
    player = new multiplayer;

    //树设置
    //设置标题
    ui->tree->setHeaderLabel("当前设备：");

    //双击树叶，触发播放
    connect(this->ui->tree, SIGNAL(itemDoubleClicked(QTreeWidgetItem *, int)), this, SLOT(checkself(QTreeWidgetItem *, int)));

    //右键树叶，弹出菜单
    this->ui->tree->setContextMenuPolicy(Qt::CustomContextMenu);//右键 不可少否则右键无反应
    connect(this->ui->tree, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(popMenu(const QPoint&)));

    this->ui->player1->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this->ui->player1, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(popClose1(const QPoint&)));

    this->ui->player2->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this->ui->player2, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(popClose2(const QPoint&)));

    this->ui->player3->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this->ui->player3, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(popClose3(const QPoint&)));

    this->ui->player4->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this->ui->player4, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(popClose4(const QPoint&)));

    //读取过往设备数据
    QFile file("DeviceMessage");
    if(!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        qDebug() << "MainWindow-file.open error or not exist!";
    }
    else
    {
        while(!file.atEnd())
        {
            //存入容器
            QString data = file.readLine();
            data.chop(1);
            A.push_back(data);
            data = file.readLine();
            data.chop(1);
            A.push_back(data);
            data = file.readLine();
            data.chop(1);
            A.push_back(data);
            data = file.readLine();
            data.chop(1);
            A.push_back(data);
            NameAAddr.push_back(A);
            A.clear();
        }
        file.close();
    }

    //展示过往树叶
    QVector<QVector<QString>>::iterator it1;
    QVector<QString>::iterator it2;
    for(it1 = NameAAddr.begin(); it1 != NameAAddr.end(); it1 ++)
    {
        it2 = (*it1).begin();
        //加载顶层节点
        QTreeWidgetItem * item = new QTreeWidgetItem(QStringList()<<it2[0]);
        ui->tree->addTopLevelItem(item);

        //加载子节点
        QTreeWidgetItem * item1 = new QTreeWidgetItem(QStringList()<<it2[1]);
        item->addChild(item1);
    }
}

//析构
MainWindow::~MainWindow()
{
    //保存数据
    QVector<QVector<QString>>::iterator it1;
    QVector<QString>::iterator it2;
    QFile file("DeviceMessage");
    if(!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        qDebug() << "~MainWindow-file.open error!";
    }

    for(it1 = NameAAddr.begin(); it1 != NameAAddr.end(); it1 ++)
    {
       it2 = (*it1).begin();

       file.write((it2[0]).toUtf8());
       file.write("\n");
       file.write((it2[1]).toUtf8());
       file.write("\n");
       file.write((it2[2]).toUtf8());
       file.write("\n");
       file.write((it2[3]).toUtf8());
       file.write("\n");
    }
    file.close();

    for(int i = 0; i < SUBPLAYER_NUM; i ++)
    {
        if("" != player->currentplayername[i])
        {
            //关闭预览
            if(!NET_DVR_StopRealPlay(lRealPlayHandle[i]))
            {
                qDebug() << "~MainWindow-NET_DVR_StopRealPlay error" << NET_DVR_GetLastError();
                return;
            }

            //释放播放库资源
            if(!PlayM4_Stop(lPort[i]))
            {
                qDebug() << "~MainWindow-PlayM4_Stop error" << PlayM4_GetLastError(lPort[i]);
                return;
            }
            if(!PlayM4_CloseStream(lPort[i]))
            {
                qDebug() << "~MainWindow-PlayM4_CloseStream error" << PlayM4_GetLastError(lPort[i]);
                return;
            }
            if(!PlayM4_FreePort(lPort[i]))
            {
                qDebug() << "~MainWindow-PlayM4_FreePort error" << PlayM4_GetLastError(lPort[i]);
                return;
            }

            //注销用户
            if(!NET_DVR_Logout(lUserID[i]))
            {
                qDebug() << "~MainWindow-NET_DVR_Logout error" << NET_DVR_GetLastError();
                return;
            }

            //释放SDK资源
            if(!NET_DVR_Cleanup())
            {
                qDebug() << "~MainWindow-NET_DVR_Cleanup error" << NET_DVR_GetLastError();
                return;
            }
        }
    }

    delete ui;
}

//回调预览数据流函数 第一个窗口
void CALLBACK g_RealDataCallBack_V31(LONG lRealHandle, DWORD dwDataType, BYTE * pBuffer, DWORD dwBufSize, void * dwUser)
{
    HWND m_hwndPlayer1 = (HWND)Pui->player1->winId();//获取当前播放sub窗口的句柄

    switch(dwDataType)
    {
    case NET_DVR_SYSHEAD: //系统头
        if(0 > lPort[0])
        {
            if(!PlayM4_GetPort(&lPort[0]))  //获取播放库未使用的通道号
            {
                qDebug() << "g_RealDataCallBack_V31-PlayM4_GetPort error" << PlayM4_GetLastError(lPort[0]);
                break;
            }

            //当前窗口标志位
            Pui->player1->currentplayer = 1;

            if(dwBufSize > 0)
            {
                if(!PlayM4_SetStreamOpenMode(lPort[0], STREAME_REALTIME))  //设置实时流播放模式
                {
                    qDebug() << "g_RealDataCallBack_V31-PlayM4_SetStreamOpenMode error" << PlayM4_GetLastError(lPort[0]);
                    break;
                }

                if(!PlayM4_OpenStream(lPort[0], pBuffer, dwBufSize, 1024 * 1024)) //打开流接口
                {
                    qDebug() << "g_RealDataCallBack_V31-PlayM4_OpenStream error" << PlayM4_GetLastError(lPort[0]);
                    break;
                }

                if(!PlayM4_Play(lPort[0], m_hwndPlayer1)) //播放开始
                {
                    qDebug() << "g_RealDataCallBack_V31-PlayM4_Play error" << PlayM4_GetLastError(lPort[0]);
                    break;
                }

//                if(!PlayM4_SetDisplayType(lPort[0], DISPLAY_QUARTER))  //低分辨率模式
//                {
//                    qDebug() << "g_RealDataCallBack_V31-PlayM4_SetDisplayType error" << PlayM4_GetLastError(lPort[0]);
//                    break;
//                }
            }
            break;
        }
        break;

    case NET_DVR_STREAMDATA:   //码流数据
        if(0 < dwBufSize && -1 != lPort[0])
        {  
//            if(11 == Pui->player1->currentplayer)
//            {
//                if(!PlayM4_WndResolutionChange(lPort[0]))
//                {
//                    qDebug() << "g_RealDataCallBack_V31-PlayM4_WndResolutionChange1 error" << PlayM4_GetLastError(lPort[0]);
//                    return;
//                }
//            }
//            if(-11 == Pui->player1->currentplayer)
//            {
//                Pui->player1->currentplayer = 1;
//                if(!PlayM4_WndResolutionChange(lPort[0]))
//                {
//                    qDebug() << "g_RealDataCallBack_V31-PlayM4_WndResolutionChange-back1 error" << PlayM4_GetLastError(lPort[0]);
//                    return;
//                }
//            }

            while(!PlayM4_InputData(lPort[0], pBuffer, dwBufSize))
            {
                qDebug() << "g_RealDataCallBack_V31-PlayM4_InputData error" << PlayM4_GetLastError(lPort[0]);
                if(11 == PlayM4_GetLastError(lPort[0]))//缓冲区满，输入流失败，需要重复送入数据
                {
                    sleep(2);
                    continue;
                }
                break;
            }
        }
        break;

    default: //其他数据
        if(0 < dwBufSize && -1 != lPort[0])
        {
            if(!PlayM4_InputData(lPort[0], pBuffer, dwBufSize))
            {
                qDebug() << "g_RealDataCallBack_V31-playM4_InputData-other error" << PlayM4_GetLastError(lPort[0]);
                break;
            }
        }
        break;
    }
}

//回调预览数据流函数 第二个窗口
void CALLBACK g_RealDataCallBack_V32(LONG lRealHandle, DWORD dwDataType, BYTE * pBuffer, DWORD dwBufSize, void * dwUser)
{
    HWND m_hwndPlayer2 = (HWND)Pui->player2->winId();//获取当前播放sub窗口的句柄

    switch (dwDataType)
    {
    case NET_DVR_SYSHEAD: //系统头
        if(0 > lPort[1])
        {
            if(!PlayM4_GetPort(&lPort[1]))  //获取播放库未使用的通道号
            {
                qDebug() << "g_RealDataCallBack_V32-PlayM4_GetPort error" << PlayM4_GetLastError(lPort[1]);
                break;
            }

            //当前窗口标志位
            Pui->player2->currentplayer = 2;

            if(0 < dwBufSize)
            {
                if(!PlayM4_SetStreamOpenMode(lPort[1], STREAME_REALTIME))  //设置实时流播放模式
                {
                    qDebug() << "g_RealDataCallBack_V32-PlayM4_SetStreamOpenMode error" << PlayM4_GetLastError(lPort[1]);
                    break;
                }

                if(!PlayM4_OpenStream(lPort[1], pBuffer, dwBufSize, 1024 * 1024)) //打开流接口
                {
                    qDebug() << "g_RealDataCallBack_V32-PlayM4_OpenStream error" << PlayM4_GetLastError(lPort[1]);
                    break;
                }

                if(!PlayM4_Play(lPort[1], m_hwndPlayer2)) //播放开始
                {
                    qDebug() << "g_RealDataCallBack_V32-PlayM4_Play error" << PlayM4_GetLastError(lPort[1]);
                    break;
                }

//                if(!PlayM4_SetDisplayType(lPort[1], DISPLAY_QUARTER))  //低分辨率模式
//                {
//                    qDebug() << "g_RealDataCallBack_V32-PlayM4_SetDisplayType error" << PlayM4_GetLastError(lPort[1]);
//                    break;
//                }
            }
            break;
        }
        break;

    case NET_DVR_STREAMDATA:   //码流数据
        if(0 < dwBufSize && -1 != lPort[1])
        {
//            if(21 == Pui->player2->currentplayer)
//            {
//                if(!PlayM4_WndResolutionChange(lPort[1]))
//                {
//                    qDebug() << "g_RealDataCallBack_V32-PlayM4_WndResolutionChange2 error" << PlayM4_GetLastError(lPort[1]);
//                    return;
//                }
//            }
//            else if(-21 == Pui->player2->currentplayer)
//            {
//                Pui->player2->currentplayer = 2;
//                if(!PlayM4_WndResolutionChange(lPort[1]))
//                {
//                    qDebug() << "g_RealDataCallBack_V32-PlayM4_WndResolutionChange-back2 error" << PlayM4_GetLastError(lPort[1]);
//                    return;
//                }
//            }

            while(!PlayM4_InputData(lPort[1], pBuffer, dwBufSize))
            {
                qDebug() << "g_RealDataCallBack_V32-PlayM4_InputData error" << PlayM4_GetLastError(lPort[1]);
                if(11 == PlayM4_GetLastError(lPort[1]))//缓冲区满，输入流失败，需要重复送入数据
                {
                    sleep(2);
                    continue;
                }
                break;
            }
        }
        break;

    default: //其他数据
        if(0 < dwBufSize && -1 != lPort[1])
        {
            if(!PlayM4_InputData(lPort[1], pBuffer, dwBufSize))
            {
                qDebug() << "g_RealDataCallBack_V32-layM4_InputData-other error" << PlayM4_GetLastError(lPort[1]);
                break;
            }
        }
        break;
    }
}

//回调预览数据流函数 第三个窗口
void CALLBACK g_RealDataCallBack_V33(LONG lRealHandle, DWORD dwDataType, BYTE * pBuffer, DWORD dwBufSize, void * dwUser)
{
    HWND m_hwndPlayer3 = (HWND)Pui->player3->winId();//获取当前播放sub窗口的句柄

    switch (dwDataType)
    {
    case NET_DVR_SYSHEAD: //系统头
        if(0 > lPort[2])
        {
            if(!PlayM4_GetPort(&lPort[2]))  //获取播放库未使用的通道号
            {
                qDebug() << "g_RealDataCallBack_V33-PlayM4_GetPort error" << PlayM4_GetLastError(lPort[2]);
                break;
            }

            //当前窗口标志位
            Pui->player3->currentplayer = 3;

            if(0 < dwBufSize)
            {
                if(!PlayM4_SetStreamOpenMode(lPort[2], STREAME_REALTIME))  //设置实时流播放模式
                {
                    qDebug() << "g_RealDataCallBack_V33-PlayM4_SetStreamOpenMode error" << PlayM4_GetLastError(lPort[2]);
                    break;
                }

                if(!PlayM4_OpenStream(lPort[2], pBuffer, dwBufSize, 1024 * 1024)) //打开流接口
                {
                    qDebug() << "g_RealDataCallBack_V33-PlayM4_OpenStream error" << PlayM4_GetLastError(lPort[2]);
                    break;
                }

                if(!PlayM4_Play(lPort[2], m_hwndPlayer3)) //播放开始
                {
                    qDebug() << "g_RealDataCallBack_V33-PlayM4_Play error" << PlayM4_GetLastError(lPort[2]);
                    break;
                }

//                if(!PlayM4_SetDisplayType(lPort[2], DISPLAY_QUARTER))  //低分辨率模式
//                {
//                    qDebug() << "g_RealDataCallBack_V33-PlayM4_SetDisplayType error" << PlayM4_GetLastError(lPort[2]);
//                    break;
//                }
            }
            break;
        }
        break;

    case NET_DVR_STREAMDATA:   //码流数据
        if(0 < dwBufSize && -1 != lPort[2])
        {
//            if(31 == Pui->player3->currentplayer)
//            {
//                if(!PlayM4_WndResolutionChange(lPort[2]))
//                {
//                    qDebug() << "g_RealDataCallBack_V33-PlayM4_WndResolutionChange3 error" << PlayM4_GetLastError(lPort[2]);
//                    return;
//                }
//            }
//            else if(-31 == Pui->player3->currentplayer)
//            {
//                Pui->player3->currentplayer = 3;
//                if(!PlayM4_WndResolutionChange(lPort[2]))
//                {
//                    qDebug() << "g_RealDataCallBack_V33-PlayM4_WndResolutionChange-back3 error" << PlayM4_GetLastError(lPort[2]);
//                    return;
//                }
//            }

            while(!PlayM4_InputData(lPort[2], pBuffer, dwBufSize))
            {
                qDebug() << "g_RealDataCallBack_V33-PlayM4_InputData error" << PlayM4_GetLastError(lPort[2]);
                if(11 == PlayM4_GetLastError(lPort[2]))//缓冲区满，输入流失败，需要重复送入数据
                {
                    sleep(2);
                    continue;
                }
                break;
            }
        }
        break;

    default: //其他数据
        if(0 < dwBufSize && -1 != lPort[2])
        {
            if(!PlayM4_InputData(lPort[2], pBuffer, dwBufSize))
            {
                qDebug() << "g_RealDataCallBack_V33-layM4_InputData-other error" << PlayM4_GetLastError(lPort[2]);
                break;
            }
        }
        break;
    }
}

//回调预览数据流函数 第四个窗口
void CALLBACK g_RealDataCallBack_V34(LONG lRealHandle, DWORD dwDataType, BYTE * pBuffer, DWORD dwBufSize, void * dwUser)
{
    HWND m_hwndPlayer4 = (HWND)Pui->player4->winId();//获取当前播放sub窗口的句柄

    switch(dwDataType)
    {
    case NET_DVR_SYSHEAD: //系统头
        if(0 > lPort[3])
        {
            if(!PlayM4_GetPort(&lPort[3]))  //获取播放库未使用的通道号
            {
                qDebug() << "g_RealDataCallBack_V34-PlayM4_GetPort error" << PlayM4_GetLastError(lPort[3]);
                break;
            }

            //当前窗口标志位
            Pui->player4->currentplayer = 4;

            if(0 < dwBufSize)
            {
                if(!PlayM4_SetStreamOpenMode(lPort[3], STREAME_REALTIME))  //设置实时流播放模式
                {
                    qDebug() << "g_RealDataCallBack_V34-PlayM4_SetStreamOpenMode error" << PlayM4_GetLastError(lPort[3]);
                    break;
                }

                if(!PlayM4_OpenStream(lPort[3], pBuffer, dwBufSize, 1024 * 1024)) //打开流接口
                {
                    qDebug() << "g_RealDataCallBack_V34-PlayM4_OpenStream error" << PlayM4_GetLastError(lPort[3]);
                    break;
                }

                if(!PlayM4_Play(lPort[3], m_hwndPlayer4)) //播放开始
                {
                    qDebug() << "g_RealDataCallBack_V34-PlayM4_Play error" << PlayM4_GetLastError(lPort[3]);
                    break;
                }

//                if(!PlayM4_SetDisplayType(lPort[3], DISPLAY_QUARTER))  //低分辨率模式
//                {
//                    qDebug() << "g_RealDataCallBack_V34-PlayM4_SetDisplayType error" << PlayM4_GetLastError(lPort[3]);
//                    break;
//                }
            }
            break;
        }
        break;

    case NET_DVR_STREAMDATA:   //码流数据
        if(0 < dwBufSize && -1 != lPort[3])
        {
//            if(41 == Pui->player4->currentplayer)
//            {
//                if(!PlayM4_WndResolutionChange(lPort[3]))
//                {
//                    qDebug() << "g_RealDataCallBack_V34-PlayM4_WndResolutionChange4 error" << PlayM4_GetLastError(lPort[3]);
//                    return;
//                }
//            }
//            else if(-41 == Pui->player4->currentplayer)
//            {
//                Pui->player4->currentplayer = 4;
//                if(!PlayM4_WndResolutionChange(lPort[3]))
//                {
//                    qDebug() << "g_RealDataCallBack_V34-PlayM4_WndResolutionChange-back4 error" << PlayM4_GetLastError(lPort[3]);
//                    return;
//                }
//            }

            while(!PlayM4_InputData(lPort[3], pBuffer, dwBufSize))
            {
                qDebug() << "g_RealDataCallBack_V34-PlayM4_InputData error" << PlayM4_GetLastError(lPort[3]);
                if(11 == PlayM4_GetLastError(lPort[3]))//缓冲区满，输入流失败，需要重复送入数据
                {
                    sleep(2);
                    continue;
                }
                break;
            }
        }
        break;

    default: //其他数据
        if(0 < dwBufSize && -1 != lPort[3])
        {
            if(!PlayM4_InputData(lPort[3], pBuffer, dwBufSize))
            {
                qDebug() << "g_RealDataCallBack_V34-playM4_InputData-other error" << PlayM4_GetLastError(lPort[3]);
                break;
            }
        }
        break;
    }
}

//获得设备名
void MainWindow::getsername(char *sername, QTreeWidgetItem *hItem)
{
    if(!hItem)
    {
        return;
    }
    QTreeWidgetItem * phItem = hItem->parent();    //获取当前item的父item
    if(!phItem)
    {	// 说明是根节点
        memset(sername, 0, 255);
        QString  qstr = hItem->text(0);
        QByteArray ba = qstr.toLatin1();         //实现QString和 char *的转换
        const char *cstr = ba.data();
        strcpy(sername, cstr);
    }
    while(phItem)
    {
        memset(sername, 0, 255);
        QString  qstr = phItem->text(0);
        QByteArray ba = qstr.toLatin1();        //实现QString和char *的转换
        const char *cstr = ba.data();
        strcpy(sername, cstr);
        phItem = phItem->parent();
    }
}

//双击树叶触发播放视频
void MainWindow::checkself(QTreeWidgetItem * item, int column)
{
    if(0 == Mode)   //4分屏模式
    {
        //当前节点
        QTreeWidgetItem * curltem = this->ui->tree->currentItem();
        if(NULL != curltem->parent())
        {
            //选择的子窗口
            int ChooseSubPlayer = -1;
            //若全部子窗口都在播放，则将第一个替换
            if("" != player->currentplayername[0] && "" != player->currentplayername[1] && "" != player->currentplayername[2] && "" != player->currentplayername[3])
            {
                //关闭预览
                if(!NET_DVR_StopRealPlay(lRealPlayHandle[0]))
                {
                    qDebug() << "checkself0-NET_DVR_StopRealPlay error" << NET_DVR_GetLastError();
                    return;
                }

                //注销用户
                if(!NET_DVR_Logout(lUserID[0]))
                {
                    qDebug() << "checkself0-NET_DVR_Logout error" << NET_DVR_GetLastError();
                    return;
                }

                //还原
                lRealPlayHandle[0] = -1;
                lUserID[0] = -1;

                ChooseSubPlayer = 0;
            }
            else//若有闲置子窗口，则选择数字小的子窗口播放
            {
                for(int i = 0; i < SUBPLAYER_NUM; i ++)
                {
                    if("" == player->currentplayername[i])
                    {
                        //1为播放
                        ChooseSubPlayer = i;
                        break;
                    }
                }
            }

            // 初始化
            NET_DVR_Init();

            //获得设备名
            char sername[256];
            memset(sername, 0, 255);
            getsername(sername, item);
            QString s1(sername);

            QVector<QVector<QString>>::iterator it;
            QVector<QString>::iterator itip;

            for(it = NameAAddr.begin(); it != NameAAddr.end(); it ++)
            {
               if(s1 == *(*it).begin())  //找到设备名进行信息提取
               {
                   for(int i = 0; i < 4; i ++)
                   {
                       if(s1 == player->currentplayername[i])
                       {
                           mouseDoubleClicklock = 1;
                           QMessageBox::information(this, "信息", "正在预览。。。");
                           mouseDoubleClicklock = 0;
                           return;
                       }
                   }

                   //QString to char *
                   QString addressqstring = (*it)[1];
                   std::string str = addressqstring.toStdString();
                   const char * addressstr = str.c_str();

                   QString usernameqstring = (*it)[2];
                   std::string str1 = usernameqstring.toStdString();
                   const char * usernamestr = str1.c_str();

                   QString passwardqstring = (*it)[3];
                   std::string str2 = passwardqstring.toStdString();
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
                   lUserID[ChooseSubPlayer] = NET_DVR_Login_V40(&struLoginInfo, &struDeviceInfoV40);  //如果设备不在线，这个函数会运行很久
                   if(lUserID[ChooseSubPlayer] < 0)
                   {
                       if(7 == NET_DVR_GetLastError())
                       {
                           mouseDoubleClicklock = 1;
                           QMessageBox::information(this, "信息", "设备不在线");
                           NET_DVR_Cleanup();
                           mouseDoubleClicklock = 0;
                           return;
                       }
                       qDebug() << "checkself0-NET_DVR_Login_V40 error, " << NET_DVR_GetLastError();
                       NET_DVR_Cleanup();
                       return;
                   }
                   break;
               }
            }

            if(0 <= lUserID[ChooseSubPlayer])
            {
                //启动预览并设置回调数据流
                NET_DVR_PREVIEWINFO struPlayInfo = {0};
                struPlayInfo.hPlayWnd      = NULL;         //需要SDK解码时句柄设为有效值，仅取流不解码时可设为空
                struPlayInfo.lChannel      = 1;            //预览通道号
                struPlayInfo.dwStreamType  = 0;            //0-主码流，1-子码流，2-码流3，3-码流4，以此类推
                struPlayInfo.dwLinkMode    = 0;            //0- TCP方式，1- UDP方式，2- 多播方式，3- RTP方式，4-RTP/RTSP，5-RSTP/HTTP
                struPlayInfo.bBlocked      = 1;            //0- 非阻塞取流，1- 阻塞取流
                struPlayInfo.byPreviewMode = 0;            //0- 正常预览

                switch(ChooseSubPlayer)
                {
                case 0:
                    lRealPlayHandle[ChooseSubPlayer] = NET_DVR_RealPlay_V40(lUserID[ChooseSubPlayer], &struPlayInfo, g_RealDataCallBack_V31, NULL);
                    break;

                case 1:
                    lRealPlayHandle[ChooseSubPlayer] = NET_DVR_RealPlay_V40(lUserID[ChooseSubPlayer], &struPlayInfo, g_RealDataCallBack_V32, NULL);
                    break;

                case 2:
                    lRealPlayHandle[ChooseSubPlayer] = NET_DVR_RealPlay_V40(lUserID[ChooseSubPlayer], &struPlayInfo, g_RealDataCallBack_V33, NULL);
                    break;

                case 3:
                    lRealPlayHandle[ChooseSubPlayer] = NET_DVR_RealPlay_V40(lUserID[ChooseSubPlayer], &struPlayInfo, g_RealDataCallBack_V34, NULL);
                    break;

                default:
                    qDebug() << "checkself0-ChooseSubPlayer error";
                }

                if(0 > lRealPlayHandle[ChooseSubPlayer])
                {
                    qDebug() << "checkself0-NET_DVR_RealPlay_V40 error, " << NET_DVR_GetLastError();
                    NET_DVR_Logout(lUserID[ChooseSubPlayer]);
                    NET_DVR_Cleanup();
                    return;
                }
            }

            //当前播放窗口设备名赋值
            player->currentplayername[ChooseSubPlayer] = s1;
        }
    }
    else  //大窗口模式
    {
        //获得设备名
        char sername[256];
        memset(sername, 0, 255);
        getsername(sername, item);
        QString s1(sername);

        for(int n = 0; n < 4; n ++)
        {
            if(s1 == player->currentplayername[n])  //所选择的设备正在子窗口播放
            {
                HWND m_hwndDisplay = (HWND)this->ui->Display->winId();//获取当前播放窗口的句柄
                QString printmessage;
                switch(player->foucsplayer)
                {
                case 1:
                {
                    if((n + 1) == player->foucsplayer)
                    {
                        break;
                    }
                    HWND m_hwndPlayer1 = (HWND)this->ui->player1->winId();//获取当前播放sub窗口的句柄
                    if(!PlayM4_Play(lPort[0], m_hwndPlayer1)) //播放开始
                    {
                        qDebug() << "checkself1-PlayM4_Play11 error" << PlayM4_GetLastError(lPort[0]);
                        return;
                    }

                    this->ui->player1->show();
                    this->ui->player1->hide();
                    if(!PlayM4_Play(lPort[n], m_hwndDisplay)) //播放开始
                    {
                        qDebug() << "checkself1-PlayM4_Play12 error" << PlayM4_GetLastError(lPort[n]);
                        return;
                    }

                    player->foucsplayer = n + 1;
                    printmessage = QString("选中窗口:%1").arg(n + 1);
                    this->ui->CurrentWindow->setText(printmessage);
                    break;
                }

                case 2:
                {
                    if((n + 1) == player->foucsplayer)
                    {
                        break;
                    }
                    HWND m_hwndPlayer2 = (HWND)this->ui->player2->winId();//获取当前播放sub窗口的句柄
                    if(!PlayM4_Play(lPort[1], m_hwndPlayer2)) //播放开始
                    {
                        qDebug() << "checkself1-PlayM4_Play21 error" << PlayM4_GetLastError(lPort[1]);
                        return;
                    }

                    this->ui->player2->show();
                    this->ui->player2->hide();
                    if(!PlayM4_Play(lPort[n], m_hwndDisplay)) //播放开始
                    {
                        qDebug() << "checkself1-PlayM4_Play22 error" << PlayM4_GetLastError(lPort[n]);
                        return;
                    }

                    player->foucsplayer = n + 1;
                    printmessage = QString("选中窗口:%1").arg(n + 1);
                    this->ui->CurrentWindow->setText(printmessage);
                    break;
                }

                case 3:
                {
                    if((n + 1) == player->foucsplayer)
                    {
                        break;
                    }
                    HWND m_hwndPlayer3 = (HWND)this->ui->player3->winId();//获取当前播放sub窗口的句柄
                    if(!PlayM4_Play(lPort[2], m_hwndPlayer3)) //播放开始
                    {
                        qDebug() << "checkself1-PlayM4_Play31 error" << PlayM4_GetLastError(lPort[2]);
                        return;
                    }

                    this->ui->player3->show();
                    this->ui->player3->hide();
                    if(!PlayM4_Play(lPort[n], m_hwndDisplay)) //播放开始
                    {
                        qDebug() << "checkself1-PlayM4_Play32 error" << PlayM4_GetLastError(lPort[n]);
                        return;
                    }

                    player->foucsplayer = n + 1;
                    printmessage = QString("选中窗口:%1").arg(n + 1);
                    this->ui->CurrentWindow->setText(printmessage);
                    break;
                }

                case 4:
                {
                    if((n + 1) == player->foucsplayer)
                    {
                        break;
                    }
                    HWND m_hwndPlayer4 = (HWND)this->ui->player4->winId();//获取当前播放sub窗口的句柄
                    if(!PlayM4_Play(lPort[3], m_hwndPlayer4)) //播放开始
                    {
                        qDebug() << "checkself1-PlayM4_Play41 error" << PlayM4_GetLastError(lPort[3]);
                        return;
                    }

                    this->ui->player4->show();
                    this->ui->player4->hide();
                    if(!PlayM4_Play(lPort[n], m_hwndDisplay)) //播放开始
                    {
                        qDebug() << "checkself1-PlayM4_Play42 error" << PlayM4_GetLastError(lPort[n]);
                        return;
                    }

                    player->foucsplayer = n + 1;
                    QString printmessage = QString("选中窗口:%1").arg(n + 1);
                    this->ui->CurrentWindow->setText(printmessage);
                    break;
                }

                default:
                    qDebug() << "checkself1-player->foucsplayer-error";
                    break;
                }
                break;
            }

            if(3 == n)  //播放设备不在子窗口中播放
            {
                mouseDoubleClicklock = 1;
                QMessageBox::information(this, "信息", "选中设备未添加至预览窗口中。。。");
                mouseDoubleClicklock = 0;
            }
        }
    }
}

//单击鼠标选中窗口
void MainWindow::mousePressEvent(QMouseEvent * e)
{
     //左键被按下选中屏幕
    if(e->button() == Qt::LeftButton)
    {
        int mouse_x = QCursor::pos().x();//鼠标点击处横坐标
        int mouse_y = QCursor::pos().y();//鼠标点击处纵坐标
        QWidget *action = QApplication::widgetAt(mouse_x, mouse_y);//获取鼠标点击处的控件

        if("player1" == action->objectName())
        {
            player->foucsplayer = 1;
            this->ui->CurrentWindow->setText("选中窗口:1");
        }
        else if("player2" == action->objectName())
        {
            player->foucsplayer = 2;
            this->ui->CurrentWindow->setText("选中窗口:2");
        }
        else if("player3" == action->objectName())
        {
            player->foucsplayer = 3;
            this->ui->CurrentWindow->setText("选中窗口:3");
        }
        else if("player4" == action->objectName())
        {
            player->foucsplayer = 4;
            this->ui->CurrentWindow->setText("选中窗口:4");
        }
    }
}

//鼠标双击触发最大化和缩小
void MainWindow::mouseDoubleClickEvent(QMouseEvent * e)
{
    if(e->button() == Qt::LeftButton && 0 == mouseDoubleClicklock && 1 == Mode)
    {
        if(1 == player->foucsplayer)
        {
            HWND m_hwndPlayer1 = (HWND)this->ui->player1->winId();//获取当前播放sub窗口的句柄
            if(!PlayM4_Play(lPort[0], m_hwndPlayer1)) //播放开始
            {
                qDebug() << "mouseDoubleClickEvent-PlayM4_Play1 error" << PlayM4_GetLastError(lPort[0]);
                return;
            }

            this->ui->Display->lower();
            this->ui->player1->show();
            this->ui->player2->show();
            this->ui->player3->show();
            this->ui->player4->show();
            Mode = 0;
            this->ui->FullScr->setText("大屏");
            PlayM4_WndResolutionChange(lPort[0]);
            PlayM4_WndResolutionChange(lPort[1]);
            PlayM4_WndResolutionChange(lPort[2]);
            PlayM4_WndResolutionChange(lPort[3]);
        }
        else if(2 == player->foucsplayer)
        {
            HWND m_hwndPlayer2 = (HWND)this->ui->player2->winId();//获取当前播放sub窗口的句柄
            if(!PlayM4_Play(lPort[1], m_hwndPlayer2)) //播放开始
            {
                qDebug() << "mouseDoubleClickEvent-PlayM4_Play2 error" << PlayM4_GetLastError(lPort[1]);
                return;
            }

            this->ui->Display->lower();
            this->ui->player1->show();
            this->ui->player2->show();
            this->ui->player3->show();
            this->ui->player4->show();
            Mode = 0;
            this->ui->FullScr->setText("大屏");
            PlayM4_WndResolutionChange(lPort[0]);
            PlayM4_WndResolutionChange(lPort[1]);
            PlayM4_WndResolutionChange(lPort[2]);
            PlayM4_WndResolutionChange(lPort[3]);
        }
        else if(3 == player->foucsplayer)
        {
            HWND m_hwndPlayer3 = (HWND)this->ui->player3->winId();//获取当前播放sub窗口的句柄
            if(!PlayM4_Play(lPort[2], m_hwndPlayer3)) //播放开始
            {
                qDebug() << "mouseDoubleClickEvent-PlayM4_Play3 error" << PlayM4_GetLastError(lPort[2]);
                return;
            }
            this->ui->Display->lower();
            this->ui->player1->show();
            this->ui->player2->show();
            this->ui->player3->show();
            this->ui->player4->show();
            Mode = 0;
            this->ui->FullScr->setText("大屏");
            PlayM4_WndResolutionChange(lPort[0]);
            PlayM4_WndResolutionChange(lPort[1]);
            PlayM4_WndResolutionChange(lPort[2]);
            PlayM4_WndResolutionChange(lPort[3]);
        }
        else if(4 == player->foucsplayer)
        {
            HWND m_hwndPlayer4 = (HWND)this->ui->player4->winId();//获取当前播放sub窗口的句柄
            if(!PlayM4_Play(lPort[3], m_hwndPlayer4)) //播放开始
            {
                qDebug() << "mouseDoubleClickEvent-PlayM4_Play4 error" << PlayM4_GetLastError(lPort[3]);
                return;
            }
            this->ui->Display->lower();
            this->ui->player1->show();
            this->ui->player2->show();
            this->ui->player3->show();
            this->ui->player4->show();
            Mode = 0;
            this->ui->FullScr->setText("大屏");
            PlayM4_WndResolutionChange(lPort[0]);
            PlayM4_WndResolutionChange(lPort[1]);
            PlayM4_WndResolutionChange(lPort[2]);
            PlayM4_WndResolutionChange(lPort[3]);
        }
    }
    else if(e->button() == Qt::LeftButton && 0 == mouseDoubleClicklock && 0 == Mode)
    {
        int mouse_x = QCursor::pos().x();//鼠标点击处横坐标
        int mouse_y = QCursor::pos().y();//鼠标点击处纵坐标
        QWidget *action = QApplication::widgetAt(mouse_x, mouse_y);//获取鼠标点击处的控件

        if("player1" == action->objectName() && "" != player->currentplayername[0])
        {
            HWND m_hwndDisplay = (HWND)this->ui->Display->winId();//获取当前播放窗口的句柄
            this->ui->Display->raise();
            this->ui->player1->hide();
            this->ui->player2->hide();
            this->ui->player3->hide();
            this->ui->player4->hide();
            if(!PlayM4_Play(lPort[0], m_hwndDisplay)) //播放开始
            {
                qDebug() << "mouseDoubleClickEvent-PlayM4_Play1 error" << PlayM4_GetLastError(lPort[0]);
                return;
            }
            Mode = 1;
            this->ui->FullScr->setText("退出");
        }
        else if("player2" == action->objectName() && "" != player->currentplayername[1])
        {
            HWND m_hwndDisplay = (HWND)this->ui->Display->winId();//获取当前播放窗口的句柄
            this->ui->Display->raise();
            this->ui->player1->hide();
            this->ui->player2->hide();
            this->ui->player3->hide();
            this->ui->player4->hide();
            if(!PlayM4_Play(lPort[1], m_hwndDisplay)) //播放开始
            {
                qDebug() << "mouseDoubleClickEvent-PlayM4_Play2 error" << PlayM4_GetLastError(lPort[1]);
                return;
            }
            Mode = 1;
            this->ui->FullScr->setText("退出");
        }
        else if("player3" == action->objectName() && "" != player->currentplayername[2])
        {
            HWND m_hwndDisplay = (HWND)this->ui->Display->winId();//获取当前播放窗口的句柄
            this->ui->Display->raise();
            this->ui->player1->hide();
            this->ui->player2->hide();
            this->ui->player3->hide();
            this->ui->player4->hide();
            if(!PlayM4_Play(lPort[2], m_hwndDisplay)) //播放开始
            {
                qDebug() << "mouseDoubleClickEvent-PlayM4_Play3 error" << PlayM4_GetLastError(lPort[2]);
                return;
            }
            Mode = 1;
            this->ui->FullScr->setText("退出");
        }
        else if("player4" == action->objectName() && "" != player->currentplayername[3])
        {
            HWND m_hwndDisplay = (HWND)this->ui->Display->winId();//获取当前播放窗口的句柄
            this->ui->Display->raise();
            this->ui->player1->hide();
            this->ui->player2->hide();
            this->ui->player3->hide();
            this->ui->player4->hide();
            if(!PlayM4_Play(lPort[3], m_hwndDisplay)) //播放开始
            {
                qDebug() << "mouseDoubleClickEvent-PlayM4_Play4 error" << PlayM4_GetLastError(lPort[3]);
                return;
            }
            Mode = 1;
            this->ui->FullScr->setText("退出");
        }
    }
}

//重命名设备命名
void MainWindow::rename()
{
    //展示添加界面
    renamewindow = new reaname;
    renamewindow->setModal(true);
    mouseDoubleClicklock = 1;
    renamewindow->show();
    renamewindow->exec();
    mouseDoubleClicklock = 0;

    if(!renamewindow->DeviceRenamefinal.isEmpty())
    {
        //当前节点
        QTreeWidgetItem * curltem = this->ui->tree->currentItem();
        if(NULL != curltem->parent())
        {
            curltem = curltem->parent();
        }

        char sername[256];
        memset(sername, 0, 255);
        getsername(sername, curltem);
        QString s1(sername);
        QVector<QVector<QString>>::iterator it;
        curltem->setText(0, renamewindow->DeviceRenamefinal);
        for(it = NameAAddr.begin(); it != NameAAddr.end(); it ++)
        {
            if(s1 == *(*it).begin())
            {
                *(*it).begin() = renamewindow->DeviceRenamefinal;

                //更新播放窗口属性
                for(int i = 0; i < 4; i ++)
                {
                    if(s1 == player->currentplayername[i])
                    {
                        player->currentplayername[i] = renamewindow->DeviceRenamefinal;
                    }
                }

                //更新文件数据
                QVector<QVector<QString>>::iterator it1;
                QVector<QString>::iterator it2;

                QFile filedelete("DeviceMessage");
                filedelete.remove();

                QFile file("DeviceMessage");
                if(!file.open(QIODevice::WriteOnly | QIODevice::Text))
                {
                    qDebug() << "rename-file.open error!";
                    return;
                }

                for(it1 = NameAAddr.begin(); it1 != NameAAddr.end(); it1 ++)
                {
                   it2 = (*it1).begin();

                   file.write((it2[0]).toUtf8());
                   file.write("\n");
                   file.write((it2[1]).toUtf8());
                   file.write("\n");
                   file.write((it2[2]).toUtf8());
                   file.write("\n");
                   file.write((it2[3]).toUtf8());
                   file.write("\n");
                }
                file.close();
                break;
            }
        }
    }
}

//删除树结构节点
void MainWindow::deleteItem()
{
    //删除数据
    //获得设备名
    QTreeWidgetItem * curltem = this->ui->tree->currentItem();
    char sername[256];
    memset(sername, 0, 255);
    getsername(sername, curltem);
    QString s1(sername);

    QVector<QVector<QString>>::iterator it;

    for(it = NameAAddr.begin(); it != NameAAddr.end(); it ++)
    {
       if(s1 == *(*it).begin())
       {
           it = NameAAddr.erase(it);

           //更新正在播放的对应窗口
           for(int i = 0; i < 4; i ++)
           {
               if(s1 == player->currentplayername[i])
               {
                   if(1 == Mode && (i + 1) == player->foucsplayer)
                   {
                       Mode = 0;
                   }

                   //关闭预览
                   if(!NET_DVR_StopRealPlay(lRealPlayHandle[i]))
                   {
                       qDebug() << "deleteItem-NET_DVR_StopRealPlay error" << NET_DVR_GetLastError();
                       return;
                   }

                   //释放播放库资源
                   if(!PlayM4_Stop(lPort[i]))
                   {
                       qDebug() << "deleteItem-PlayM4_Stop error" << PlayM4_GetLastError(lPort[i]);
                       return;
                   }
                   if(!PlayM4_CloseStream(lPort[i]))
                   {
                       qDebug() << "deleteItem-PlayM4_CloseStream error" << PlayM4_GetLastError(lPort[i]);
                       return;
                   }
                   if(!PlayM4_FreePort(lPort[i]))
                   {
                       qDebug() << "deleteItem-PlayM4_FreePort error" << PlayM4_GetLastError(lPort[i]);
                       return;
                   }

                   //注销用户
                   if(!NET_DVR_Logout(lUserID[i]))
                   {
                       qDebug() << "deleteItem-NET_DVR_Logout error" << NET_DVR_GetLastError();
                       return;
                   }

                   //释放SDK资源
                   if(!NET_DVR_Cleanup())
                   {
                       qDebug() << "deleteItem-NET_DVR_Cleanup error" << NET_DVR_GetLastError();
                       return;
                   }

                   //还原
                   lRealPlayHandle[i] = -1;
                   lUserID[i] = -1;
                   lPort[i] = -1;
                   player->currentplayername[i] = "";
                   this->ui->player1->hide();
                   this->ui->player1->show();
                   this->ui->player2->hide();
                   this->ui->player2->show();
                   this->ui->player3->hide();
                   this->ui->player3->show();
                   this->ui->player4->hide();
                   this->ui->player4->show();
                   PlayM4_WndResolutionChange(lPort[0]);
                   PlayM4_WndResolutionChange(lPort[1]);
                   PlayM4_WndResolutionChange(lPort[2]);
                   PlayM4_WndResolutionChange(lPort[3]);
               }
           }

           //更新文件数据
           QVector<QVector<QString>>::iterator it1;
           QVector<QString>::iterator it2;

           QFile filedelete("DeviceMessage");
           filedelete.remove();

           QFile file("DeviceMessage");
           if(!file.open(QIODevice::WriteOnly | QIODevice::Text))
           {
               qDebug() << "deleteItem-file.open error!";
               return;
           }

           for(it1 = NameAAddr.begin(); it1 != NameAAddr.end(); it1 ++)
           {
              it2 = (*it1).begin();

              file.write((it2[0]).toUtf8());
              file.write("\n");
              file.write((it2[1]).toUtf8());
              file.write("\n");
              file.write((it2[2]).toUtf8());
              file.write("\n");
              file.write((it2[3]).toUtf8());
              file.write("\n");
           }
           file.close();
           break;
       }
    }
    //删除树点
    if(NULL != curltem->parent())
    {
        QTreeWidgetItem * tmp = curltem;
        curltem = curltem->parent();
        delete tmp;
    }
    delete curltem;
}

//右键树叶，弹出菜单
void MainWindow::popMenu(const QPoint&)
{
    //当前节点
    QTreeWidgetItem * curltem = this->ui->tree->currentItem();

    //右键空白处返回
    if(NULL == curltem)
    {
        return;
    }

    QAction deleteWell(QString::fromLocal8Bit("删除设备"), this);//删除
    QAction reNameWell(QString ::fromLocal8Bit("设备重命名"), this);//重命名
    //在界面上删除该item
    connect(&deleteWell, SIGNAL(triggered()), this, SLOT(deleteItem()));
    connect(&reNameWell, SIGNAL(triggered()), this, SLOT(rename()));

    QPoint pos;
    QMenu menu(this->ui->tree);
    menu.addAction(&deleteWell);
    menu.addAction(&reNameWell);
    menu.exec(QCursor::pos());  //在当前鼠标位置显示
}

//关闭窗口1函数
void MainWindow::closeplayer1()
{
    //关闭预览
    if(!NET_DVR_StopRealPlay(lRealPlayHandle[0]))
    {
        qDebug() << "closeplayer1-NET_DVR_StopRealPlay error" << NET_DVR_GetLastError();
        return;
    }

    //释放播放库资源
    if(!PlayM4_Stop(lPort[0]))
    {
        qDebug() << "closeplayer1-PlayM4_Stop error" << PlayM4_GetLastError(lPort[0]);
        return;
    }
    if(!PlayM4_CloseStream(lPort[0]))
    {
        qDebug() << "closeplayer1-PlayM4_CloseStream error" << PlayM4_GetLastError(lPort[0]);
        return;
    }
    if(!PlayM4_FreePort(lPort[0]))
    {
        qDebug() << "closeplayer1-PlayM4_FreePort error" << PlayM4_GetLastError(lPort[0]);
        return;
    }

    //注销用户
    if(!NET_DVR_Logout(lUserID[0]))
    {
        qDebug() << "closeplayer1-NET_DVR_Logout error" << NET_DVR_GetLastError();
        return;
    }

    //释放SDK资源
    if(!NET_DVR_Cleanup())
    {
        qDebug() << "closeplayer1-NET_DVR_Cleanup error" << NET_DVR_GetLastError();
        return;
    }

    //还原
    lRealPlayHandle[0] = -1;
    lUserID[0] = -1;
    lPort[0] = -1;
    player->currentplayername[0] = "";
    this->ui->player1->hide();
    this->ui->player1->show();
}

//关闭窗口1预览
void MainWindow::popClose1(const QPoint&)
{
    if("" != player->currentplayername[0])
    {
        QAction closeWell(QString::fromLocal8Bit("关闭当前预览"), this);//关闭当前预览
        connect(&closeWell, SIGNAL(triggered()), this, SLOT(closeplayer1()));

        QPoint pos;
        QMenu menu(this->ui->player1);
        menu.addAction(&closeWell);
        menu.exec(QCursor::pos());  //在当前鼠标位置显示
    }
}

//关闭窗口2函数
void MainWindow::closeplayer2()
{
    //关闭预览
    if(!NET_DVR_StopRealPlay(lRealPlayHandle[1]))
    {
        qDebug() << "closeplayer2-NET_DVR_StopRealPlay error" << NET_DVR_GetLastError();
        return;
    }

    //释放播放库资源
    if(!PlayM4_Stop(lPort[1]))
    {
        qDebug() << "closeplayer2-PlayM4_Stop error" << PlayM4_GetLastError(lPort[1]);
        return;
    }
    if(!PlayM4_CloseStream(lPort[1]))
    {
        qDebug() << "closeplayer2-PlayM4_CloseStream error" << PlayM4_GetLastError(lPort[1]);
        return;
    }
    if(!PlayM4_FreePort(lPort[1]))
    {
        qDebug() << "closeplayer2-PlayM4_FreePort error" << PlayM4_GetLastError(lPort[1]);
        return;
    }

    //注销用户
    if(!NET_DVR_Logout(lUserID[1]))
    {
        qDebug() << "closeplayer2-NET_DVR_Logout error" << NET_DVR_GetLastError();
        return;
    }

    //释放SDK资源
    if(!NET_DVR_Cleanup())
    {
        qDebug() << "closeplayer2-NET_DVR_Cleanup error" << NET_DVR_GetLastError();
        return;
    }

    //还原
    lRealPlayHandle[1] = -1;
    lUserID[1] = -1;
    lPort[1] = -1;
    player->currentplayername[1] = "";
    this->ui->player2->hide();
    this->ui->player2->show();
}

//关闭窗口2预览
void MainWindow::popClose2(const QPoint&)
{
    if("" != player->currentplayername[1])
    {
        QAction closeWell(QString::fromLocal8Bit("关闭当前预览"), this);//关闭当前预览
        connect(&closeWell, SIGNAL(triggered()), this, SLOT(closeplayer2()));

        QPoint pos;
        QMenu menu(this->ui->player2);
        menu.addAction(&closeWell);
        menu.exec(QCursor::pos());  //在当前鼠标位置显示
    }
}

//关闭窗口3函数
void MainWindow::closeplayer3()
{
    //关闭预览
    if(!NET_DVR_StopRealPlay(lRealPlayHandle[2]))
    {
        qDebug() << "closeplayer3-NET_DVR_StopRealPlay error" << NET_DVR_GetLastError();
        return;
    }

    //释放播放库资源
    if(!PlayM4_Stop(lPort[2]))
    {
        qDebug() << "closeplayer3-PlayM4_Stop error" << PlayM4_GetLastError(lPort[2]);
        return;
    }
    if(!PlayM4_CloseStream(lPort[2]))
    {
        qDebug() << "closeplayer3-PlayM4_CloseStream error" << PlayM4_GetLastError(lPort[2]);
        return;
    }
    if(!PlayM4_FreePort(lPort[2]))
    {
        qDebug() << "closeplayer3-PlayM4_FreePort error" << PlayM4_GetLastError(lPort[2]);
        return;
    }

    //注销用户
    if(!NET_DVR_Logout(lUserID[2]))
    {
        qDebug() << "closeplayer3-NET_DVR_Logout error" << NET_DVR_GetLastError();
        return;
    }

    //释放SDK资源
    if(!NET_DVR_Cleanup())
    {
        qDebug() << "closeplayer3-NET_DVR_Cleanup error" << NET_DVR_GetLastError();
        return;
    }

    //还原
    lRealPlayHandle[2] = -1;
    lUserID[2] = -1;
    lPort[2] = -1;
    player->currentplayername[2] = "";
    this->ui->player3->hide();
    this->ui->player3->show();
}

//关闭窗口3预览
void MainWindow::popClose3(const QPoint&)
{
    if("" != player->currentplayername[2])
    {
        QAction closeWell(QString::fromLocal8Bit("关闭当前预览"), this);//关闭当前预览
        connect(&closeWell, SIGNAL(triggered()), this, SLOT(closeplayer3()));

        QPoint pos;
        QMenu menu(this->ui->player3);
        menu.addAction(&closeWell);
        menu.exec(QCursor::pos());  //在当前鼠标位置显示
    }
}

//关闭窗口4函数
void MainWindow::closeplayer4()
{
    //关闭预览
    if(!NET_DVR_StopRealPlay(lRealPlayHandle[0]))
    {
        qDebug() << "closeplayer4-NET_DVR_StopRealPlay error" << NET_DVR_GetLastError();
        return;
    }

    //释放播放库资源
    if(!PlayM4_Stop(lPort[3]))
    {
        qDebug() << "closeplayer4-PlayM4_Stop error" << PlayM4_GetLastError(lPort[3]);
        return;
    }
    if(!PlayM4_CloseStream(lPort[3]))
    {
        qDebug() << "closeplayer4-PlayM4_CloseStream error" << PlayM4_GetLastError(lPort[3]);
        return;
    }
    if(!PlayM4_FreePort(lPort[3]))
    {
        qDebug() << "closeplayer4-PlayM4_FreePort error" << PlayM4_GetLastError(lPort[3]);
        return;
    }

    //注销用户
    if(!NET_DVR_Logout(lUserID[3]))
    {
        qDebug() << "closeplayer4-NET_DVR_Logout error" << NET_DVR_GetLastError();
        return;
    }

    //释放SDK资源
    if(!NET_DVR_Cleanup())
    {
        qDebug() << "closeplayer4-NET_DVR_Cleanup error" << NET_DVR_GetLastError();
        return;
    }

    //还原
    lRealPlayHandle[3] = -1;
    lUserID[3] = -1;
    lPort[3] = -1;
    player->currentplayername[3] = "";
    this->ui->player4->hide();
    this->ui->player4->show();
}

//关闭窗口4预览
void MainWindow::popClose4(const QPoint&)
{
    if("" != player->currentplayername[3])
    {
        QAction closeWell(QString::fromLocal8Bit("关闭当前预览"), this);//关闭当前预览
        connect(&closeWell, SIGNAL(triggered()), this, SLOT(closeplayer4()));

        QPoint pos;
        QMenu menu(this->ui->player4);
        menu.addAction(&closeWell);
        menu.exec(QCursor::pos());  //在当前鼠标位置显示
    }
}

//大屏显示退出按钮
void MainWindow::on_FullScr_clicked(bool checked)
{
    if(0 == Mode && "" != player->currentplayername[0])
    {
        if(1 == player->foucsplayer)
        {
            HWND m_hwndDisplay = (HWND)this->ui->Display->winId();//获取当前播放窗口的句柄
            this->ui->Display->raise();
            this->ui->player1->hide();
            this->ui->player2->hide();
            this->ui->player3->hide();
            this->ui->player4->hide();
            if(!PlayM4_Play(lPort[0], m_hwndDisplay)) //播放开始
            {
                qDebug() << "on_FullScr_clicked-PlayM4_Play1 error" << PlayM4_GetLastError(lPort[0]);
                return;
            }
            Mode = 1;
            this->ui->FullScr->setText("退出");
        }
        else if(2 == player->foucsplayer && "" != player->currentplayername[1])
        {
            HWND m_hwndDisplay = (HWND)this->ui->Display->winId();//获取当前播放窗口的句柄
            this->ui->Display->raise();
            this->ui->player1->hide();
            this->ui->player2->hide();
            this->ui->player3->hide();
            this->ui->player4->hide();
            if(!PlayM4_Play(lPort[1], m_hwndDisplay)) //播放开始
            {
                qDebug() << "on_FullScr_clicked-PlayM4_Play2 error" << PlayM4_GetLastError(lPort[1]);
                return;
            }
            Mode = 1;
            this->ui->FullScr->setText("退出");
        }
        else if(3 == player->foucsplayer && "" != player->currentplayername[2])
        {
            HWND m_hwndDisplay = (HWND)this->ui->Display->winId();//获取当前播放窗口的句柄
            this->ui->Display->raise();
            this->ui->player1->hide();
            this->ui->player2->hide();
            this->ui->player3->hide();
            this->ui->player4->hide();
            if(!PlayM4_Play(lPort[2], m_hwndDisplay)) //播放开始
            {
                qDebug() << "on_FullScr_clicked-PlayM4_Play3 error" << PlayM4_GetLastError(lPort[2]);
                return;
            }
            Mode = 1;
            this->ui->FullScr->setText("退出");
        }
        else if(4 == player->foucsplayer && "" != player->currentplayername[3])
        {
            HWND m_hwndDisplay = (HWND)this->ui->Display->winId();//获取当前播放窗口的句柄
            this->ui->Display->raise();
            this->ui->player1->hide();
            this->ui->player2->hide();
            this->ui->player3->hide();
            this->ui->player4->hide();
            if(!PlayM4_Play(lPort[3], m_hwndDisplay)) //播放开始
            {
                qDebug() << "on_FullScr_clicked-PlayM4_Play4 error" << PlayM4_GetLastError(lPort[3]);
                return;
            }
            Mode = 1;
            this->ui->FullScr->setText("退出");
        }
    }
    else if(1 == Mode)
    {
        if(1 == player->foucsplayer)
        {
            HWND m_hwndPlayer1 = (HWND)this->ui->player1->winId();//获取当前播放sub窗口的句柄
            if(!PlayM4_Play(lPort[0], m_hwndPlayer1)) //播放开始
            {
                qDebug() << "on_FullScr_clicked-PlayM4_Play1 error" << PlayM4_GetLastError(lPort[0]);
                return;
            }

            this->ui->Display->lower();
            this->ui->player1->show();
            this->ui->player2->show();
            this->ui->player3->show();
            this->ui->player4->show();
            Mode = 0;
            this->ui->FullScr->setText("大屏");
            PlayM4_WndResolutionChange(lPort[0]);
            PlayM4_WndResolutionChange(lPort[1]);
            PlayM4_WndResolutionChange(lPort[2]);
            PlayM4_WndResolutionChange(lPort[3]);
        }
        else if(2 == player->foucsplayer)
        {
            HWND m_hwndPlayer2 = (HWND)this->ui->player2->winId();//获取当前播放sub窗口的句柄
            if(!PlayM4_Play(lPort[1], m_hwndPlayer2)) //播放开始
            {
                qDebug() << "on_FullScr_clicked-PlayM4_Play2 error" << PlayM4_GetLastError(lPort[1]);
                return;
            }

            this->ui->Display->lower();
            this->ui->player1->show();
            this->ui->player2->show();
            this->ui->player3->show();
            this->ui->player4->show();
            Mode = 0;
            this->ui->FullScr->setText("大屏");
            PlayM4_WndResolutionChange(lPort[0]);
            PlayM4_WndResolutionChange(lPort[1]);
            PlayM4_WndResolutionChange(lPort[2]);
            PlayM4_WndResolutionChange(lPort[3]);
        }
        else if(3 == player->foucsplayer)
        {
            HWND m_hwndPlayer3 = (HWND)this->ui->player3->winId();//获取当前播放sub窗口的句柄
            if(!PlayM4_Play(lPort[2], m_hwndPlayer3)) //播放开始
            {
                qDebug() << "on_FullScr_clicked-PlayM4_Play3 error" << PlayM4_GetLastError(lPort[2]);
                return;
            }
            this->ui->Display->lower();
            this->ui->player1->show();
            this->ui->player2->show();
            this->ui->player3->show();
            this->ui->player4->show();
            Mode = 0;
            this->ui->FullScr->setText("大屏");
            PlayM4_WndResolutionChange(lPort[0]);
            PlayM4_WndResolutionChange(lPort[1]);
            PlayM4_WndResolutionChange(lPort[2]);
            PlayM4_WndResolutionChange(lPort[3]);
        }
        else if(4 == player->foucsplayer)
        {
            HWND m_hwndPlayer4 = (HWND)this->ui->player4->winId();//获取当前播放sub窗口的句柄
            if(!PlayM4_Play(lPort[3], m_hwndPlayer4)) //播放开始
            {
                qDebug() << "on_FullScr_clicked-PlayM4_Play4 error" << PlayM4_GetLastError(lPort[3]);
                return;
            }
            this->ui->Display->lower();
            this->ui->player1->show();
            this->ui->player2->show();
            this->ui->player3->show();
            this->ui->player4->show();
            Mode = 0;
            this->ui->FullScr->setText("大屏");
            PlayM4_WndResolutionChange(lPort[0]);
            PlayM4_WndResolutionChange(lPort[1]);
            PlayM4_WndResolutionChange(lPort[2]);
            PlayM4_WndResolutionChange(lPort[3]);
        }
    }
}

//按ESC退出大窗口
void MainWindow::keyPressEvent(QKeyEvent * event)
{
    if(event->key() == Qt::Key_Escape)
    {
        if(1 == player->foucsplayer && 1 == Mode)
        {
            HWND m_hwndPlayer1 = (HWND)this->ui->player1->winId();//获取当前播放sub窗口的句柄
            if(!PlayM4_Play(lPort[0], m_hwndPlayer1)) //播放开始
            {
                qDebug() << "keyPressEvent-PlayM4_Play1 error" << PlayM4_GetLastError(lPort[0]);
                return;
            }

            this->ui->Display->lower();
            this->ui->player1->show();
            this->ui->player2->show();
            this->ui->player3->show();
            this->ui->player4->show();
            Mode = 0;
            this->ui->FullScr->setText("大屏");
            PlayM4_WndResolutionChange(lPort[0]);
            PlayM4_WndResolutionChange(lPort[1]);
            PlayM4_WndResolutionChange(lPort[2]);
            PlayM4_WndResolutionChange(lPort[3]);
        }
        else if(2 == player->foucsplayer && 1 == Mode)
        {
            HWND m_hwndPlayer2 = (HWND)this->ui->player2->winId();//获取当前播放sub窗口的句柄
            if(!PlayM4_Play(lPort[1], m_hwndPlayer2)) //播放开始
            {
                qDebug() << "keyPressEvent-PlayM4_Play2 error" << PlayM4_GetLastError(lPort[1]);
                return;
            }

            this->ui->Display->lower();
            this->ui->player1->show();
            this->ui->player2->show();
            this->ui->player3->show();
            this->ui->player4->show();
            Mode = 0;
            this->ui->FullScr->setText("大屏");
            PlayM4_WndResolutionChange(lPort[0]);
            PlayM4_WndResolutionChange(lPort[1]);
            PlayM4_WndResolutionChange(lPort[2]);
            PlayM4_WndResolutionChange(lPort[3]);
        }
        else if(3 == player->foucsplayer && 1 == Mode)
        {
            HWND m_hwndPlayer3 = (HWND)this->ui->player3->winId();//获取当前播放sub窗口的句柄
            if(!PlayM4_Play(lPort[2], m_hwndPlayer3)) //播放开始
            {
                qDebug() << "keyPressEvent-PlayM4_Play3 error" << PlayM4_GetLastError(lPort[2]);
                return;
            }
            this->ui->Display->lower();
            this->ui->player1->show();
            this->ui->player2->show();
            this->ui->player3->show();
            this->ui->player4->show();
            Mode = 0;
            this->ui->FullScr->setText("大屏");
            PlayM4_WndResolutionChange(lPort[0]);
            PlayM4_WndResolutionChange(lPort[1]);
            PlayM4_WndResolutionChange(lPort[2]);
            PlayM4_WndResolutionChange(lPort[3]);
        }
        else if(4 == player->foucsplayer && 1 == Mode)
        {
            HWND m_hwndPlayer4 = (HWND)this->ui->player4->winId();//获取当前播放sub窗口的句柄
            if(!PlayM4_Play(lPort[3], m_hwndPlayer4)) //播放开始
            {
                qDebug() << "keyPressEvent-PlayM4_Play4 error" << PlayM4_GetLastError(lPort[3]);
                return;
            }
            this->ui->Display->lower();
            this->ui->player1->show();
            this->ui->player2->show();
            this->ui->player3->show();
            this->ui->player4->show();
            Mode = 0;
            this->ui->FullScr->setText("大屏");
            PlayM4_WndResolutionChange(lPort[0]);
            PlayM4_WndResolutionChange(lPort[1]);
            PlayM4_WndResolutionChange(lPort[2]);
            PlayM4_WndResolutionChange(lPort[3]);
        }
    }
}

//手动添加相机或摄像头设备+
void MainWindow::on_AddDeviceBtn_clicked(bool checked)
{
    //展示添加界面
    adddevicewindow = new addnewdevice;
    adddevicewindow->setModal(true);
    mouseDoubleClicklock = 1;
    adddevicewindow->show();
    adddevicewindow->exec();
    mouseDoubleClicklock = 0;

    if(!adddevicewindow->IPaddr.isEmpty() && !adddevicewindow->Devicename.isEmpty() && !adddevicewindow->username.isEmpty() && !adddevicewindow->passward.isEmpty())
    {
        //加载顶层节点
        QTreeWidgetItem * item = new QTreeWidgetItem(QStringList()<<adddevicewindow->Devicename);
        ui->tree->addTopLevelItem(item);

        //加载子节点
        QTreeWidgetItem * itemchild = new QTreeWidgetItem(QStringList()<<adddevicewindow->IPaddr);
        item->addChild(itemchild);

        //存入容器 0自定义设备名 1IP地址 2用户名 3设备密码
        A.push_back(adddevicewindow->Devicenamefinal);
        A.push_back(adddevicewindow->IPaddrfinal);
        A.push_back(adddevicewindow->usernamefinal);
        A.push_back(adddevicewindow->passwardfinal);
        NameAAddr.push_back(A);
        A.clear();
    }

    //保存数据
    QVector<QVector<QString>>::iterator it1;
    QVector<QString>::iterator it2;

    QFile filedelete("DeviceMessage");
    filedelete.remove();

    QFile file("DeviceMessage");
    if(!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        qDebug() << "on_AddDeviceBtn_clicked-file.open error!";
        return;
    }

    for(it1 = NameAAddr.begin(); it1 != NameAAddr.end(); it1 ++)
    {
       it2 = (*it1).begin();

       file.write((it2[0]).toUtf8());
       file.write("\n");
       file.write((it2[1]).toUtf8());
       file.write("\n");
       file.write((it2[2]).toUtf8());
       file.write("\n");
       file.write((it2[3]).toUtf8());
       file.write("\n");
    }
    file.close();
}
