#ifndef TCPMGR_H
#define TCPMGR_H

#include <QTcpSocket>
#include "singleton.h"
#include "global.h"
#include <functional>
#include <QObject>
#include <QTimer>
#include "userdata.h"

class TcpMgr: public QObject, public Singleton<TcpMgr>, public std::enable_shared_from_this<TcpMgr>
{
    Q_OBJECT
    friend class Singleton<TcpMgr>;
public:
    ~TcpMgr();

private:
    TcpMgr();
    void initHandlers();
    void handleMsg(ReqId id, int len, QByteArray data); // 根据ReqId进行回调

private:
    QTcpSocket _socket;
    QTimer _login_timer; // 等待聊天登录回包的超时定时器
    QString _host;
    uint16_t _port;
    QByteArray _buffer;
    bool _b_recv_pending;
    quint16 _message_id;
    quint16 _message_len;
    QMap<ReqId,std::function<void(ReqId id, int len, QByteArray data)>> _handlers;

public slots:
    void slot_tcp_connect(ServerInfo); // 与login_dialog中的信号相连
    void slot_send_data(ReqId reqId, QString data);

signals:
    void sig_conn_success(bool b_success);
    void sig_send_data(ReqId reqId, QString data);
    void sig_switch_chatDlg();
    void sig_login_failed(int);
    void sig_login_timeout(); // 聊天登录长时间收不到回包
    void sig_user_search(std::shared_ptr<SearchInfo>);
    void sig_friend_apply(std::shared_ptr<AddFriendApply>);// 我 申请 对方 为好友的信号
    void sig_add_auth_friend(std::shared_ptr<AuthInfo>);// 对方 同意了，发送的信号
    void sig_auth_rsp(std::shared_ptr<AuthRsp>);// 对方申请我为好友，我同意了，会发送该信号
};

#endif // TCPMGR_H
