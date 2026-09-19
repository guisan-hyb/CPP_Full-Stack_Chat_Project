#include "logindialog.h"
#include "ui_logindialog.h"

LoginDialog::LoginDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::LoginDialog)
{
    ui->setupUi(this);
    connect(ui->reg_btn,&QPushButton::clicked,this,&LoginDialog::sig_switchRegister);

    // 忘记密码的一些功能
    ui->forget_label->SetState("normal","hover","","selected","selected_hover","");
    // ui->forget_label->setCursor(Qt::PointingHandCursor);
    connect(ui->forget_label,&ClickedLabel::clicked,this,&LoginDialog::slot_forget_pwd);
}

LoginDialog::~LoginDialog()
{
    delete ui;
}

void LoginDialog::slot_forget_pwd()
{
    qDebug()<<"slot forget pwd"<<Qt::endl;
    emit sig_switch_Reset();
}
