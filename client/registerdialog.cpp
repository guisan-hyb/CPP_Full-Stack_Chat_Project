#include "registerdialog.h"
#include "ui_registerdialog.h"
#include "global.h"
#include "httpmgr.h"


RegisterDialog::RegisterDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::RegisterDialog)
{
    ui->setupUi(this);
    ui->pass_edit->setEchoMode(QLineEdit::Password);//设置为密码模式
    ui->ack_edit->setEchoMode(QLineEdit::Password);

    ui->err_tip->setProperty("state","normal");
    repolish(ui->err_tip);

    // 连接HttpMgr的 模块完成 信号
    connect(HttpMgr::GetInstance().get(), &HttpMgr::sig_reg_mod_finish,
            this, &RegisterDialog::slot_reg_mod_finish);

    // 注册一些回调函数
    initHttpHandlers();
}

RegisterDialog::~RegisterDialog()
{
    delete ui;
}

// 点击 获取验证码 ，判断邮箱格式是否正确
void RegisterDialog::on_verify_btn_clicked()
{
    auto email = ui->email_edit->text();
    QRegularExpression regex(R"((\w+)(\.|_)?(\w*)@(\w+)(\.(\w+))+)");
    bool match = regex.match(email).hasMatch();

    if(match){
        //showTip(tr("邮箱地址正确"),true);
        //发送http验证码
    }else{
        showTip(tr("邮箱地址不正确"),false);
    }
}

void RegisterDialog::slot_reg_mod_finish(ReqId id, QString res, ErrorCodes err)
{
    if(err != ErrorCodes::SUCCESS){
        showTip(tr("网络请求错误"),false);
        return;
    }

    // 解析JSON 字符串, res 转化为 QByteArray
    QJsonDocument jsonDoc = QJsonDocument::fromJson(res.toUtf8());
    if(jsonDoc.isNull()){
        showTip(tr("json解析失败"),false);
        return;
    }

    //json解析错误
    if(!jsonDoc.isObject()){
        showTip(tr("json解析失败"),false);
        return;
    }

    _handlers[id](jsonDoc.object());// 调用回调函数
    return;
}

void RegisterDialog::showTip(QString str, bool b_ok)
{
    if(b_ok){
        ui->err_tip->setProperty("state","normal");
    }else{
        ui->err_tip->setProperty("state","err");
    }
    ui->err_tip->setText(str);

    repolish(ui->err_tip);
}

void RegisterDialog::initHttpHandlers()
{
    // 注册获取验证码回包的逻辑
    _handlers[ReqId::ID_GET_VERIFY_CODE] = [this](const QJsonObject& jsonObj){
        int error = jsonObj["error"].toInt();
        if(error != ErrorCodes::SUCCESS){
            showTip(tr("参数错误"),false);
            return;
        }

        auto email = jsonObj["email"].toString();
        showTip(tr("验证码已经发送至邮箱，注意查收"),true);
        qDebug()<<"email is: "<<email<<Qt::endl;
    };
}

