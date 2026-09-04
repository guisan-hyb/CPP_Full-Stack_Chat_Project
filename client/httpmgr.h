#ifndef HTTPMGR_H
#define HTTPMGR_H

#include "singleton.h"
#include <QUrl>
#include <QString>
#include <QObject>
#include <QNetworkAccessManager>

#include <QJsonObject>
#include <QJsonDocument>

class HttpMgr : public QObject, public Singleton<HttpMgr>, public std::enable_shared_from_this<HttpMgr>
{
    Q_OBJECT // 使用信号与槽
    friend class Singleton<HttpMgr>;
public:
    ~HttpMgr();

private:
    HttpMgr();
    void PostHttpReq(QUrl url,QJsonObject json,ReqId req_id,Modules mod);// url, 内容，某个模块具体功能的id, 哪个模块

private:
    QNetworkAccessManager _manager;

private slots:
    void slot_http_finish(ReqId id, QString res, ErrorCodes err, Modules mod);// 槽函数参数数量要≤信号数量，且参数顺序要一致

signals:
    void sig_http_finish(ReqId id, QString res, ErrorCodes err, Modules mod);// 当一个http发送完毕后，会发送信号通知其他模块
    void sig_reg_mod_finish(ReqId id,QString res,ErrorCodes err);// 注册模块http响应结束
};

#endif // HTTPMGR_H
