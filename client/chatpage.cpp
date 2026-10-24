#include "chatpage.h"
#include "PictureBubble.h"
#include "TextBubble.h"
#include "chatitembase.h"
#include "ui_chatpage.h"
#include <QStyleOption>
#include <QPainter>
#include <QStyle>
#include <QMessageBox>
#include "usermgr.h"
#include "tcpmgr.h"
#include "chatprotocol.h"

ChatPage::ChatPage(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::ChatPage)
{
    ui->setupUi(this);

    // 设置按钮样式
    ui->send_btn->SetState("normal","hover","press");

    // 设置图标样式
    ui->emo_label->SetState("normal","hover","press","normal","hover","press");
    ui->file_label->SetState("normal","hover","press","normal","hover","press");
}

ChatPage::~ChatPage()
{
    delete ui;
}

void ChatPage::paintEvent(QPaintEvent *event)
{
    QStyleOption opt;
    opt.initFrom(this);
    QPainter p(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
}

void ChatPage::SetUserInfo(std::shared_ptr<UserInfo> user_info)
{
    if (!user_info) {
        qWarning() << "ChatPage::SetUserInfo received null UserInfo";
        return;
    }

    _user_info = user_info;
    // 设置Ui界面
    ui->title_label->setText(_user_info->_name);
    ui->chat_data_list->removeAllItem();
    for(auto& msg: user_info->_chat_msgs){
        AppendChatMsg(msg);
    }
}

void ChatPage::AppendChatMsg(std::shared_ptr<TextChatData> msg)
{
    auto self_info = UserMgr::GetInstance()->GetUserInfo();
    if (!msg || !self_info) {
        qWarning() << "ChatPage::AppendChatMsg missing message or current user";
        return;
    }

    ChatRole role;
    //todo... 添加聊天显示
    if (msg->_from_uid == self_info->_uid) {
        role = ChatRole::Self;
        ChatItemBase* pChatItem = new ChatItemBase(role);

        pChatItem->setUserName(self_info->_name);
        pChatItem->setUserIcon(QPixmap(self_info->_icon));
        QWidget* pBubble = nullptr;
        pBubble = new TextBubble(role, msg->_msg_content);
        pChatItem->setWidget(pBubble);
        ui->chat_data_list->appendChatItem(pChatItem);
    }
    else {
        role = ChatRole::Other;
        ChatItemBase* pChatItem = new ChatItemBase(role);
        auto friend_info = UserMgr::GetInstance()->GetFriendById(msg->_from_uid);
        if (friend_info == nullptr) {
            return;
        }
        pChatItem->setUserName(friend_info->_name);
        pChatItem->setUserIcon(QPixmap(friend_info->_icon));
        QWidget* pBubble = nullptr;
        pBubble = new TextBubble(role, msg->_msg_content);
        pChatItem->setWidget(pBubble);
        ui->chat_data_list->appendChatItem(pChatItem);
    }

}

void ChatPage::on_send_btn_clicked()
{
    if(_user_info == nullptr){
        qWarning() << "friend_info is empty";
        return;
    }

    auto user_info = UserMgr::GetInstance()->GetUserInfo();
    if (user_info == nullptr) {
        qWarning() << "current user info is empty";
        return;
    }

    auto pTextEdit = ui->chat_edit;
    ChatRole role = ChatRole::Self;
    QString userName = user_info->_name;
    QString userIcon = user_info->_icon;

    const QVector<MsgInfo>& msgList = pTextEdit->getMsgList();
    QJsonArray textArray;

    auto flushTextArray = [this, &textArray]() {
        if (textArray.isEmpty()) {
            return;
        }

        const QByteArray payload = ChatProtocol::BuildTextRequest(_user_info->_uid, textArray);
        if (payload.size() > ChatProtocol::MaxTextPayloadBytes) {
            qWarning() << "text payload exceeds protocol limit:" << payload.size();
            return;
        }

        emit TcpMgr::GetInstance()->sig_send_data(ReqId::ID_TEXT_CHAT_MSG_REQ, payload);
        textArray = QJsonArray();
    };

    for(int i=0; i<msgList.size(); ++i)
    {
        if (msgList[i].content.isEmpty()) {
            continue;
        }

        QString type = msgList[i].msgFlag;
        ChatItemBase *pChatItem = new ChatItemBase(role);
        pChatItem->setUserName(userName);
        pChatItem->setUserIcon(QPixmap(userIcon));
        QWidget *pBubble = nullptr;

        if(type == "text") {
            //生成唯一id
            QUuid uuid = QUuid::createUuid();
            //转为字符串
            QString uuidString = uuid.toString();

            QJsonObject obj;
            obj["content"] = msgList[i].content;
            obj["unique_id"] = uuidString;

            auto append_result = ChatProtocol::TryAppendText(_user_info->_uid, textArray, obj);
            if (append_result == ChatProtocol::AppendResult::NeedNewBatch) {
                flushTextArray();
                append_result = ChatProtocol::TryAppendText(_user_info->_uid, textArray, obj);
            }

            if (append_result == ChatProtocol::AppendResult::TooLarge) {
                qWarning() << "single chat item exceeds protocol limit"
                           << "unique_id:" << uuidString;
                QMessageBox::warning(this, tr("消息过长"),
                                     tr("单条消息超过 2048 字节协议限制，未发送。"));
                delete pChatItem;
                continue;
            }

            pBubble = new TextBubble(role, msgList[i].content);
            auto txt_msg = std::make_shared<TextChatData>(uuidString, obj["content"].toString(),
                                                          user_info->_uid, _user_info->_uid);
            emit sig_append_send_chat_msg(txt_msg);
        } else if(type == "image") {
            pBubble = new PictureBubble(QPixmap(msgList[i].content) , role);
        }

        //发送消息
        if(pBubble != nullptr)
        {
            pChatItem->setWidget(pBubble);
            ui->chat_data_list->appendChatItem(pChatItem);
        } else {
            delete pChatItem;
        }
    }

    flushTextArray();
}

