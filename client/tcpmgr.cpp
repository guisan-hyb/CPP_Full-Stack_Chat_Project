#include "tcpmgr.h"
#include <QAbstractSocket>
#include "usermgr.h"
#include <QJsonArray>

TcpMgr::~TcpMgr()
{

}

TcpMgr::TcpMgr()
    : _host(""), _port(0), _b_recv_pending(false), _b_connected(false), _message_id(0), _message_len(0)
{
    QObject::connect(&_socket,&QTcpSocket::connected,this,[this](){
        _b_connected = true;
        qDebug()<<"Connected to server!";
        emit sig_conn_success(true);// 连接建立后发送消息
    });

    QObject::connect(&_socket,&QTcpSocket::readyRead,this,[this](){
        _buffer.append(_socket.readAll());

        forever{
            if(!_b_recv_pending){
                if(_buffer.size() < 4) return;

                QDataStream s(&_buffer,QIODevice::ReadOnly);
                s>>_message_id>>_message_len;

                _buffer.remove(0,4);// 移除包头

                qDebug()<<"message_id: "<<_message_id<<" "<<"message_len: "<<_message_len;
            }

            if(_buffer.size() < _message_len){
                _b_recv_pending = true;
                return;
            }

            QByteArray messageBody = _buffer.left(_message_len);
            _buffer.remove(0,_message_len);
            _b_recv_pending = false;

            qDebug()<<"receive body msg is: "<<messageBody;

            // 调用注册的回调函数，处理包体信息
            handleMsg(ReqId(_message_id),_message_len,messageBody);
        }
    });

    // 处理错误
    QObject::connect(&_socket, QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::errorOccurred), [this](QAbstractSocket::SocketError socketError) {
        qDebug() << "socket error:" << socketError << _socket.errorString();
        // 连接出错时通知登录界面，别让按钮一直卡在禁用状态
        _login_timer.stop();
        if (!_b_connected) {
            emit sig_conn_success(false);
        }
    });

    // 聊天登录超时保护：服务端不回包时也要让界面能提示并恢复
    _login_timer.setSingleShot(true);
    _login_timer.setInterval(10000);// 10秒
    QObject::connect(&_login_timer,&QTimer::timeout,this,[this](){
        qDebug()<<"wait ID_CHAT_LOGIN_RSP timeout";
        emit sig_login_timeout();
    });

    // 处理连接断开
    QObject::connect(&_socket,&QTcpSocket::disconnected,[this](){
        qDebug()<<"Disconnected from server";
        _login_timer.stop();
        _buffer.clear();
        _b_recv_pending = false;
        _message_id = 0;
        _message_len = 0;

        const bool was_connected = _b_connected;
        _b_connected = false;
        if (was_connected) {
            emit sig_conn_lost();
        }
    });

    //连接发送信号用来发送数据，期望在任何地方发送信号就会调用写数据的槽函数
    QObject::connect(this,&TcpMgr::sig_send_data,this,&TcpMgr::slot_send_data);

    // 注册回调
    initHandlers();
}

void TcpMgr::initHandlers()
{
    //auto self = shared_from_this();
    // 细节：为什么这里不用这句话，让lambda捕获self呢？
    // 因为要使用shared_from_this的前提是这个类要构造完全，
    // 由于我们在构造函数里调用initHandlers,所以这个类没构造完

    _handlers[ReqId::ID_CHAT_LOGIN_RSP] = [this](ReqId id, int len, QByteArray data){
        _login_timer.stop();// 收到登录回包，取消超时等待
        qDebug()<<"handle id is: "<<id<<", data is: "<<data;
        QJsonDocument jsonDoc = QJsonDocument::fromJson(data);

        // 是否转换成功
        if(jsonDoc.isNull()){
            qDebug()<<"Failed to create QJsonDocument";
            return;
        }

        QJsonObject jsonObj = jsonDoc.object();
        if(!jsonObj.contains("error")){
            int err = ErrorCodes::ERR_JSON;
            qDebug() << "Login Failed, err is Json Parse Err" << err ;
            emit sig_login_failed(err);
            return;
        }

        int err = jsonObj["error"].toInt();
        if(err != ErrorCodes::SUCCESS){
            qDebug()<<"Login failed, err is: "<<err;
            emit sig_login_failed(err);
            return;
        }

        auto uid = jsonObj["uid"].toInt();
        auto name = jsonObj["name"].toString();
        auto nick = jsonObj["nick"].toString();
        auto icon = jsonObj["icon"].toString();
        auto sex = jsonObj["sex"].toInt();
        auto user_info = std::make_shared<UserInfo>(uid, name, nick, icon, sex);

        UserMgr::GetInstance()->SetUserInfo(user_info);
        UserMgr::GetInstance()->SetToken(jsonObj["token"].toString());

        if(jsonObj.contains("apply_list")){
            UserMgr::GetInstance()->AppendApplyList(jsonObj["apply_list"].toArray());
        }

        if(jsonObj.contains("friend_list")){
            UserMgr::GetInstance()->AppendFriendList(jsonObj["friend_list"].toArray());
        }

        emit sig_switch_chatDlg();
    };

    // 查找用户的回包：
    // 之前这里注册的 id 写成了 ID_ADD_FRIEND_RSP(1010)，而服务端回的是 ID_SEARCH_USER_RSP(1008)，
    // 客户端查不到对应的处理函数，就丢掉了回包(日志里的 "not found id [ 1008 ] to handle")，
    // 于是界面一直停在加载动画上。
    _handlers[ReqId::ID_SEARCH_USER_RSP] = [this](ReqId id, int len,QByteArray data){
        Q_UNUSED(len);
        qDebug()<<"handle id is: "<<id<<", data is: "<<data;
        QJsonDocument jsonDoc = QJsonDocument::fromJson(data);

        // 是否转换成功
        if(jsonDoc.isNull()){
            qDebug()<<"Failed to create QJsonDocument";
            emit sig_user_search(nullptr);
            return;
        }

        QJsonObject jsonObj = jsonDoc.object();
        // 服务端没查到用户(或请求有问题)时会带上非 0 的 error，这时按“没找到”处理
        int err = jsonObj.contains("error") ? jsonObj["error"].toInt() : ErrorCodes::ERR_JSON;
        if(err != ErrorCodes::SUCCESS){
            qDebug()<<"Search user failed, err is: "<<err;
            emit sig_user_search(nullptr);
            return;
        }

        auto search_info = std::make_shared<SearchInfo>(jsonObj["uid"].toInt(),jsonObj["name"].toString(),
                                                        jsonObj["nick"].toString(),jsonObj["desc"].toString(),
                                                        jsonObj["sex"].toInt(),jsonObj["icon"].toString());
        // 服务端理论上不会回没有 uid 的“成功”包，这里再兜一层，避免弹出空白的结果框
        if(search_info->_uid <= 0){
            qDebug()<<"Search user rsp has no valid uid";
            emit sig_user_search(nullptr);
            return;
        }

        emit sig_user_search(search_info);
    };

    _handlers[ReqId::ID_ADD_FRIEND_RSP] = [this](ReqId id,int len,QByteArray data){
        Q_UNUSED(len);
        qDebug()<<"handle id is: "<<id<<" data is: "<<data;
        // 将QByteArray转换为QJsonDocument
        QJsonDocument jsonDoc = QJsonDocument::fromJson(data);

        // 检查是否转换成功
        if(jsonDoc.isNull()){
            qDebug()<<"Failed to creat QJsonDocument";
            return;
        }

        QJsonObject jsonObj = jsonDoc.object();

        if(!jsonObj.contains("error")){
            int err = ErrorCodes::ERR_JSON;
            qDebug()<<"Add Friend Failed, err is Json Parse Err: "<<err;
            return;
        }

        int err = jsonObj["error"].toInt();
        if(err!=ErrorCodes::SUCCESS){
            qDebug()<<"Add Friend Failed, err is: "<<err;
            return;
        }

        qDebug()<<"Add Friend Success";
    };

    _handlers[ReqId::ID_NOTIFY_ADD_FRIEND_REQ] = [this](ReqId id,int len,QByteArray data){
        Q_UNUSED(len);
        qDebug()<<"handle id is: "<<id<<" data is: "<<data;
        QJsonDocument jsonDoc = QJsonDocument::fromJson(data);

        if(jsonDoc.isNull()){
            qDebug()<<"Failed to create QJsonDocument";
            return;
        }

        QJsonObject jsonObj = jsonDoc.object();

        if(!jsonObj.contains("error")){
            int err = ErrorCodes::ERR_JSON;
            qDebug()<<"notify add friend req failed"<<err;
            emit sig_user_search(nullptr);
            return;
        }

        int err = jsonObj["error"].toInt();
        if(err!=ErrorCodes::SUCCESS){
            qDebug()<<"notify add friend req failed"<<err;
            emit sig_user_search(nullptr);
            return;
        }

        int from_uid = jsonObj["applyuid"].toInt();
        QString name = jsonObj["name"].toString();
        QString desc = jsonObj["desc"].toString();
        QString icon = jsonObj["icon"].toString();
        QString nick = jsonObj["nick"].toString();
        int sex = jsonObj["sex"].toInt();

        auto apply_info = std::make_shared<AddFriendApply>(from_uid,name,desc,icon,nick,sex);

        emit sig_friend_apply(apply_info);
        qDebug()<<"notify add friend req success";
    };

    _handlers[ReqId::ID_NOTIFY_AUTH_FRIEND_REQ] = [this](ReqId id,int len, QByteArray data){
        Q_UNUSED(len);
        qDebug()<<"handle id is: "<<id<<" data is: "<<data;
        QJsonDocument jsonDoc = QJsonDocument::fromJson(data);

        if(jsonDoc.isNull()){
            qDebug()<<"Failed to create QJsonDocument";
            return;
        }

        QJsonObject jsonObj = jsonDoc.object();
        if(!jsonObj.contains("error")){
            int err = ErrorCodes::ERR_JSON;
            qDebug()<<"notify authen friend req failed "<<err;
            return;
        }

        int err = jsonObj["error"].toInt();
        if(err!=ErrorCodes::SUCCESS){
            qDebug()<<"Auth Friend Failed, err is: "<<err;
            return;
        }

        int from_uid = jsonObj["from_uid"].toInt();
        QString name = jsonObj["name"].toString();
        QString nick = jsonObj["nick"].toString();
        QString icon = jsonObj["icon"].toString();
        int sex = jsonObj["sex"].toInt();

        auto auth_info = std::make_shared<AuthInfo>(from_uid,name,nick,icon,sex);

        emit sig_add_auth_friend(auth_info);
    };

    _handlers[ReqId::ID_AUTH_FRIEND_RSP] = [this](ReqId id,int len, QByteArray data){
        Q_UNUSED(len);
        qDebug()<<"handle id is: "<<id<<" data is: "<<data;
        QJsonDocument jsonDoc = QJsonDocument::fromJson(data);

        if(jsonDoc.isNull()){
            qDebug()<<"Failed to create QJsonDocument";
            return;
        }

        QJsonObject jsonObj = jsonDoc.object();
        if(!jsonObj.contains("error")){
            int err = ErrorCodes::ERR_JSON;
            qDebug()<<"auth friend rsp failed "<<err;
            return;
        }

        int err = jsonObj["error"].toInt();
        if(err!=ErrorCodes::SUCCESS){
            qDebug()<<"Auth Friend Failed, err is: "<<err;
            return;
        }

        auto name = jsonObj["name"].toString();
        auto nick = jsonObj["nick"].toString();
        auto icon = jsonObj["icon"].toString();
        auto sex = jsonObj["sex"].toInt();
        auto uid = jsonObj["uid"].toInt();
        auto rsp = std::make_shared<AuthRsp>(uid,name,nick,icon,sex);

        emit sig_auth_rsp(rsp);

        qDebug()<<"Auth Friend Success";
    };

    _handlers[ID_TEXT_CHAT_MSG_RSP] = [this](ReqId id,int len,QByteArray data){
        Q_UNUSED(len);
        qDebug()<<"handle id is: "<<id<<" data is: "<<data<<Qt::endl;
        QJsonDocument jsonDoc = QJsonDocument::fromJson(data);

        if(jsonDoc.isNull()){
            qDebug()<<"Failed to create QJsonDocument";
            return;
        }

        QJsonObject jsonObj = jsonDoc.object();

        if(!jsonObj.contains("error")){
            int err = ErrorCodes::ERR_JSON;
            qDebug()<<"Chat Msg Rsp Failed, err is Json Parse Err: "<<err;
            return;
        }

        int err = jsonObj["error"].toInt();
        if(err!=ErrorCodes::SUCCESS){
            qDebug()<<"Chat Msg Rsp Failed, err is: "<<err;
            return;
        }

        qDebug()<<"Receive Text Chat Rsp Success";
        // ui设置送达等标记 todo...
    };

    _handlers[ID_NOTIFY_TEXT_CHAT_MSG_REQ] = [this](ReqId id,int len,QByteArray data){
        Q_UNUSED(len);
        qDebug() << "handle id is " << id << " data is " << data;
        // 将QByteArray转换为QJsonDocument
        QJsonDocument jsonDoc = QJsonDocument::fromJson(data);

        // 检查转换是否成功
        if (jsonDoc.isNull()) {
            qDebug() << "Failed to create QJsonDocument.";
            return;
        }

        QJsonObject jsonObj = jsonDoc.object();

        if (!jsonObj.contains("error")) {
            int err = ErrorCodes::ERR_JSON;
            qDebug() << "Notify Chat Msg Failed, err is Json Parse Err" << err;
            return;
        }

        int err = jsonObj["error"].toInt();
        if (err != ErrorCodes::SUCCESS) {
            qDebug() << "Notify Chat Msg Failed, err is " << err;
            return;
        }

        qDebug() << "Receive Text Chat Notify Success " ;
        auto msg_ptr = std::make_shared<TextChatMsg>(jsonObj["from_uid"].toInt(),
                                                     jsonObj["to_uid"].toInt(),jsonObj["text_array"].toArray());
        emit sig_text_chat_msg(msg_ptr);
    };
}

void TcpMgr::handleMsg(ReqId id, int len, QByteArray data)
{
    auto it = _handlers.find(id);
    if(it == _handlers.end()){
        qDebug()<< "not found id ["<< id << "] to handle";
        return ;
    }

    it.value()(id,len,data);
}

void TcpMgr::slot_tcp_connect(ServerInfo si)
{
    qDebug()<<"receive tcp connect signal";
    qDebug()<<"Connecting to server...";
    _host = si.Host;
    _port = static_cast<uint16_t>(si.Port.toUInt());
    _buffer.clear();
    _b_recv_pending = false;
    _b_connected = false;
    _socket.connectToHost(_host,_port);
}

void TcpMgr::slot_send_data(ReqId reqId, QByteArray dataBytes)
{
    if (_socket.state() != QAbstractSocket::ConnectedState) {
        qWarning() << "cannot send data while TCP socket is disconnected";
        return;
    }

    if (dataBytes.size() > 2048) {
        qWarning() << "TCP payload exceeds 2048 bytes:" << dataBytes.size();
        return;
    }

    uint16_t id = reqId;
    quint16 len = static_cast<quint16>(dataBytes.size());

    QByteArray block;
    QDataStream out(&block,QIODevice::WriteOnly);
    out.setByteOrder(QDataStream::BigEndian);

    out<<id<<len;
    block.append(dataBytes);

    _socket.write(block);

    // 发的是聊天登录请求，开始等回包
    if(reqId == ReqId::ID_CHAT_LOGIN){
        _login_timer.start();
    }
}

