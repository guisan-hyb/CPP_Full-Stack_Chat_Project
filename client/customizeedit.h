#ifndef CUSTOMIZEEDIT_H
#define CUSTOMIZEEDIT_H

#include <QLineEdit>
#include <QSignalBlocker>
#include "textutils.h"

class CustomizeEdit: public QLineEdit
{
    Q_OBJECT
public:
    CustomizeEdit(QWidget *parent = nullptr);
    void SetMaxLength(int maxLen);
protected:
    void focusOutEvent(QFocusEvent *event) override
    {
        // 执行失去焦点时的处理逻辑
        //qDebug() << "CustomizeEdit focusout";
        // 调用基类的focusOutEvent()方法，保证基类的行为得到执行
        QLineEdit::focusOutEvent(event);
        //发送失去焦点得信号
        emit sig_foucus_out();
    }
private:
    void limitTextLength(QString text) {
        if(_max_len <= 0){
            return;
        }

        if (text.size() > _max_len) {
            const QSignalBlocker blocker(this);
            const QString clamped = ClampTextToMaxCharacters(text, _max_len);
            this->setText(clamped);
            this->setCursorPosition(clamped.size());
        }
    }

    int _max_len;
signals:
    void sig_foucus_out();
};

#endif // CUSTOMIZEEDIT_H
