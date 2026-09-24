#ifndef LOGINDIALOG_H
#define LOGINDIALOG_H

#include <QDialog>
#include "global.h"

namespace Ui {
class LoginDialog;
}

class LoginDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LoginDialog(QWidget *parent = nullptr);
    ~LoginDialog();

private:
    void initHttpHandlers();// 注册http报文返回时的回调处理
    void initHead();
    bool checkUserValid();
    bool checkPwdValid();
    void AddTipErr(TipErr te,QString tips);
    void DelTipErr(TipErr te);
    void showTip(QString str, bool b_ok);
    void enableBtn(bool enabled);

private:
    Ui::LoginDialog *ui;
    QMap<TipErr,QString> _tip_errs;
    QMap<ReqId, std::function<void(const QJsonObject&)>> _handlers;

    // 缓存一下
    int _uid;
    QString _token;

signals:
    void sig_switchRegister();//登陆界面切换到注册界面
    void sig_switch_Reset();//点击重置密码切换到重置界面
    void sig_connect_tcp(ServerInfo);//收到http GateServer的服务之后，将信号发给TCP管理者, TCP管理者去发动长连接(与ChatServer)

private slots:
    void slot_forget_pwd();//重置密码
    void on_login_btn_clicked();// 登录
    void slot_login_mod_finish(ReqId id, QString res, ErrorCodes err);// 处理 http登录回包
};

#endif // LOGINDIALOG_H
