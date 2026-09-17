#include "timerbtn.h"
#include <QMouseEvent>

TimerBtn::TimerBtn(QWidget *parent)
    : QPushButton(parent), _counter(10)
{
    _timer = new QTimer(this);
    connect(_timer,&QTimer::timeout,[this](){
        _counter--;
        if(_counter<=0){
            _timer->stop();
            _counter = 10;
            this->setText("获取验证码");
            this->setEnabled(true);
            return;
        }
        this->setText(QString::number(_counter));
    });
}

TimerBtn::~TimerBtn()
{
    _timer->stop();
}

void TimerBtn::mouseReleaseEvent(QMouseEvent *e)
{
    if(e->button() == Qt::LeftButton){
        // 这里处理鼠标左键释放事件
        qDebug()<<"MyButton was released!"<<Qt::endl;
        this->setEnabled(false);
        this->setText(QString::number(_counter));
        _timer->start(1000);
        emit clicked();// 发送原本槽函数的点击信号，防止覆盖
    }

    // 调用基类以确保正常的事件处理
    QPushButton::mouseReleaseEvent(e);
}
