#ifndef MULTIPLAYER_H
#define MULTIPLAYER_H

#include <QWidget>
#include <QKeyEvent>
#include <QFrame>
#include <QPalette>
#include <QMouseEvent>
#include <QDebug>
#include "playermessage.h"
#include "incCn/HCNetSDK.h"
#include "incCn/PlayM4.h"
#include "incCn/plaympeg4.h"

class multiplayer : public QFrame
{
    Q_OBJECT

public:
    explicit multiplayer(QWidget *parent = nullptr);

    //当前窗口正在播放的设备的名称,""则为未播放
    QString currentplayername[4] = {"", "", "", ""};

    //当前子播放器  1,2,3,4
    int currentplayer = -1;

    //焦点播放窗口 1,2,3,4
    int foucsplayer = 1;

private:   
    void keyPressEvent(QKeyEvent * event);

    //调色盘
    QPalette pal;

signals:

};

#endif // MULTIPLAYER_H
