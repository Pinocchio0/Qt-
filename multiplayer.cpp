#include "multiplayer.h"

//构造
multiplayer::multiplayer(QWidget *parent) : QFrame(parent)
{
    //设置播放窗口背景颜色
    pal.setColor(QPalette::Background, Qt::black);
    this->setAutoFillBackground(true);
    setPalette(pal);
}

//按ESC退出全屏
void multiplayer::keyPressEvent(QKeyEvent * event)
{
    if(event->key() == Qt::Key_Escape)
    {
       this->setWindowFlags(Qt::SubWindow);
       this->showNormal();

//       if(11 == this->currentplayer)
//       {
//           this->currentplayer = -11;
//       }
//       else if(21 == this->currentplayer)
//       {
//           this->currentplayer = -21;
//       }
//       else if(31 == this->currentplayer)
//       {
//           this->currentplayer = -31;
//       }
//       else if(41 == this->currentplayer)
//       {
//           this->currentplayer = -41;
//       }
    }
}
