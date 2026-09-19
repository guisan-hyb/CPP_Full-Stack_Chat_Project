#ifndef LOGINDIALOG_H
#define LOGINDIALOG_H

#include <QDialog>

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
    Ui::LoginDialog *ui;

signals:
    void sig_switchRegister();//登陆界面切换到注册界面
    void sig_switch_Reset();//点击重置密码切换到重置界面

private slots:
    void slot_forget_pwd();//重置密码
};

#endif // LOGINDIALOG_H
