#include "tcpmgr.h"
#include <QAbstractSocket>
#include "usermgr.h"


TcpMgr::~TcpMgr()
{

}

TcpMgr::TcpMgr()
    : _host(""), _port(0), _b_recv_pending(false), _message_id(0), _message_len(0)
{
    QObject::connect(&_socket,&QTcpSocket::connected,this,[this](){
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
    QObject::connect(&_socket, QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::errorOccurred), [&](QAbstractSocket::SocketError socketError) {
        Q_UNUSED(socketError)
        qDebug() << "Error:" << _socket.errorString();
    });

    // 处理连接断开
    QObject::connect(&_socket,&QTcpSocket::disconnected,[this](){
        qDebug()<<"Disconnected from server";
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

        UserMgr::GetInstance()->SetName(jsonObj["name"].toString());
        UserMgr::GetInstance()->SetToken(jsonObj["token"].toString());
        UserMgr::GetInstance()->SetUid(jsonObj["uid"].toInt());

        emit sig_switch_chatDlg();
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
    _socket.connectToHost(_host,_port);
}

void TcpMgr::slot_send_data(ReqId reqId, QString data)
{
    uint16_t id = reqId;
    QByteArray dataBytes = data.toUtf8();
    quint16 len = static_cast<quint16>(dataBytes.size());

    QByteArray block;
    QDataStream out(&block,QIODevice::WriteOnly);
    out.setByteOrder(QDataStream::BigEndian);

    out<<id<<len;
    block.append(dataBytes);

    _socket.write(block);
}

