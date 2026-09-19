#include "clickedlabel.h"
#include <QMouseEvent>

ClickedLabel::ClickedLabel(QWidget *parent)
    : QLabel(parent),_cur_state(ClickLbState::Normal)
{
    this->setCursor(Qt::PointingHandCursor);
}

ClickedLabel::~ClickedLabel()
{

}

// 处理鼠标点击逻辑
void ClickedLabel::mousePressEvent(QMouseEvent *ev)
{
    if(ev->button() == Qt::LeftButton){
        if(_cur_state == ClickLbState::Normal){
            qDebug()<<"clicked, change to selected hover: "<<_selected_hover;
            _cur_state = ClickLbState::Selected;
            setProperty("state",_selected_hover);// 联动qss
            repolish(this);
            update();
        }else{
            qDebug()<<"clicked,change to normal hover: "<<_normal_hover;
            _cur_state = ClickLbState::Normal;
            setProperty("state",_normal_hover);// 联动qss
            repolish(this);
            update();
        }
        emit clicked();// 发送自定义的点击信号
    }

    // 调用基类方法
    QLabel::mousePressEvent(ev);
}

// 处理鼠标悬停进入事件
void ClickedLabel::enterEvent(QEnterEvent *event)
{
    if(_cur_state == ClickLbState::Normal){
        qDebug()<<"enter, change to normal_hover"<<_normal_hover;
        setProperty("state",_normal_hover);
        repolish(this);
        update();
    }else{
        qDebug()<<"enter, change to selected_hover"<<_selected_hover;
        setProperty("state",_selected_hover);
        repolish(this);
        update();
    }

    QLabel::enterEvent(event);
}

// 处理鼠标悬停离开事件
void ClickedLabel::leaveEvent(QEvent *event)
{
    if(_cur_state == ClickLbState::Normal){
        qDebug()<<"leave, change to normal: "<<_normal;
        setProperty("state",_normal);
        repolish(this);
        update();
    }else{
        qDebug()<<"leave, change to select: "<<_selected;
        setProperty("state",_selected);
        repolish(this);
        update();
    }

    QLabel::leaveEvent(event);
}

void ClickedLabel::SetState(QString normal, QString hover, QString press, QString select, QString select_hover, QString select_press)
{
    _normal = normal;
    _normal_hover = hover;
    _normal_press = press;

    _selected = select;
    _selected_hover = select_hover;
    _selected_press = select_press;

    setProperty("state",_normal);
    repolish(this);
}

ClickLbState ClickedLabel::GetCurState()
{
    return _cur_state;
}
