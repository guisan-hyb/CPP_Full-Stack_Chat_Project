#ifndef REGISTERDIALOG_H
#define REGISTERDIALOG_H

#include <QDialog>
#include "global.h"

namespace Ui {
class RegisterDialog;
}

class RegisterDialog : public QDialog
{
    Q_OBJECT

public:
    explicit RegisterDialog(QWidget *parent = nullptr);
    ~RegisterDialog();

private slots:
    void on_verify_btn_clicked();// ui界面 转到槽 自动生成，可以考虑改槽函数名称
    void slot_reg_mod_finish(ReqId id, QString res, ErrorCodes err);// 接收 HttpMgr 注册模块http信号响应结束 的新华

private:
    void showTip(QString str, bool b_ok);// 展示提示
    void initHttpHandlers();// 注册一些回调函数

private:
    Ui::RegisterDialog *ui;
    QMap<ReqId, std::function<void(const QJsonObject&)>> _handlers;
};

#endif // REGISTERDIALOG_H
