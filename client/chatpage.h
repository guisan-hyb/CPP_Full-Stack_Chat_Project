#ifndef CHATPAGE_H
#define CHATPAGE_H

#include "userdata.h"
#include <QWidget>

namespace Ui {
class ChatPage;
}

class ChatPage : public QWidget
{
    Q_OBJECT

public:
    explicit ChatPage(QWidget *parent = nullptr);
    ~ChatPage();
    void SetUserInfo(std::shared_ptr<UserInfo> user_info);
    void AppendChatMsg(std::shared_ptr<TextChatData> msg);

protected:
    // 注: 这里用protected是因为 -> paintEvent是由QWidget::event()内部调用的，所以用protected正好满足了可重写但不可直接调用的需求
    virtual void paintEvent(QPaintEvent *event) override;

private:
    Ui::ChatPage *ui;
    std::shared_ptr<UserInfo> _user_info;

private slots:
    void on_send_btn_clicked();

signals:
    void sig_append_send_chat_msg(std::shared_ptr<TextChatData> msg);
};

#endif // CHATPAGE_H
