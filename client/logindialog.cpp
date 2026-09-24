#include "logindialog.h"
#include "ui_logindialog.h"
#include <QPainter>
#include <QPainterPath>
#include "httpmgr.h"

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

    initHead();// 调用initHead()
    initHttpHandlers();// 注册回调函数

    // 连接 登录回包 信号
    connect(HttpMgr::GetInstance().get(),&HttpMgr::sig_login_mod_finish,this,&LoginDialog::slot_login_mod_finish);
}

LoginDialog::~LoginDialog()
{
    delete ui;
}

void LoginDialog::initHttpHandlers()
{
    // 注册获取登录回包信息
    _handlers[ReqId::ID_LOGIN_USER] = [this](QJsonObject jsonObj){
        int error = jsonObj["error"].toInt();
        if(error!=ErrorCodes::SUCCESS){
            showTip(tr("参数错误"),false);
            enableBtn(true); // 设置按钮可用
            return;
        }
        auto email = jsonObj["email"].toString();

        // 发送信号通知TcpMgr发送长连接
        ServerInfo si;
        si.Uid = jsonObj["uid"].toInt();
        si.Host = jsonObj["host"].toString();
        si.Port = jsonObj["port"].toString();
        si.Token = jsonObj["token"].toString();

        _uid = si.Uid;
        _token = si.Token;
        qDebug()<<"email is: "<<email<<" uid is: "<<si.Uid<<" host is: "<<si.Host<<" Port is: "<<si.Port<<" Token is: "<<si.Token;

        emit sig_connect_tcp(si);
    };
}

void LoginDialog::initHead()
{
    // 加载图片
    QPixmap originalPixmap(":/res/jianchi.jpg");
    // 设置图片自动缩放
    qDebug()<<originalPixmap.size()<<ui->head_label->size()<<Qt::endl;
    originalPixmap = originalPixmap.scaled(ui->head_label->size(),Qt::KeepAspectRatio,Qt::SmoothTransformation);

    // 创建一个和原始图片相同大小的QPixmap, 用于绘制圆角图片
    QPixmap roundedPixmap(originalPixmap.size());
    roundedPixmap.fill(Qt::transparent);// 用透明填充

    QPainter painter(&roundedPixmap);
    painter.setRenderHint(QPainter::Antialiasing);// 设置抗锯齿，使圆角更平滑
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    // 使用QPainterPath设置圆角
    QPainterPath path;
    path.addRoundedRect(0,0,originalPixmap.width(),originalPixmap.height(),10,10);
    painter.setClipPath(path);

    // 将原始图片绘制到roundedPixmap上
    painter.drawPixmap(0,0,originalPixmap);

    // 设置绘制好的圆角图片到QLabel上
    ui->head_label->setPixmap(roundedPixmap);
}

bool LoginDialog::checkUserValid()
{
    auto email = ui->email_lineEdit->text();
    if(email.isEmpty()){
        qDebug()<<"email empty";
        AddTipErr(TipErr::TIP_EMAIL_ERR,tr("邮箱不能为空"));
        return false;
    }
    DelTipErr(TipErr::TIP_EMAIL_ERR);
    return true;
}

bool LoginDialog::checkPwdValid()
{
    auto pass = ui->pass_lineEdit->text();
    if(pass.length() <6 || pass.length() > 15){
        // 提示长度不正确
        AddTipErr(TipErr::TIP_PWD_ERR, tr("密码长度应为6~15"));
        return false;
    }

    // 创建一个正则表达式对象，按照上述密码要求
    // 这个正则表达式解释：
    // ^[a-zA-Z0-9!@#$%^&*]{6,15}$ 密码长度至少6，可以是字母、数字和特定的特殊字符
    QRegularExpression regExp("^[a-zA-Z0-9!@#$%^&*]{6,15}$");
    bool match = regExp.match(pass).hasMatch();

    if(!match){
        AddTipErr(TipErr::TIP_PWD_ERR, tr("不能包含非法字符"));
        return false;
    }

    DelTipErr(TipErr::TIP_PWD_ERR);
    return true;
}

void LoginDialog::AddTipErr(TipErr te, QString tips)
{
    _tip_errs[te] = tips;
    showTip(tips,false);
}

void LoginDialog::DelTipErr(TipErr te)
{
    _tip_errs.remove(te);
    if(_tip_errs.empty()){
        ui->err_tip->clear();
    }
}

void LoginDialog::showTip(QString str, bool b_ok)
{
    if(b_ok){
        ui->err_tip->setProperty("state","normal");
    }else{
        ui->err_tip->setProperty("state","err");
    }
    ui->err_tip->setText(str);

    repolish(ui->err_tip);
}

void LoginDialog::enableBtn(bool enabled)
{
    ui->login_btn->setEnabled(enabled);
    ui->reg_btn->setEnabled(enabled);
}

void LoginDialog::slot_forget_pwd()
{
    qDebug()<<"slot forget pwd"<<Qt::endl;
    emit sig_switch_Reset();
}

void LoginDialog::on_login_btn_clicked()
{
    qDebug()<<"login btn clicked";
    if(checkUserValid() == false){
        return;
    }

    if(checkPwdValid() == false){
        return;
    }

    enableBtn(false);// 设置按钮失效
    auto email = ui->email_lineEdit->text();
    auto pwd = ui->pass_lineEdit->text();

    // 发送http请求登录
    QJsonObject json_obj;
    json_obj["email"] = email;
    json_obj["passwd"] = xorString(pwd);
    HttpMgr::GetInstance()->PostHttpReq(QUrl(gate_url_prefix+"/user_login"),
                                        json_obj,ReqId::ID_LOGIN_USER,Modules::MOD_LOGIN);
}

void LoginDialog::slot_login_mod_finish(ReqId id, QString res, ErrorCodes err)
{
    if(err != ErrorCodes::SUCCESS){
        showTip(tr("网络请求错误"),false);
        return;
    }

    // 解析 JSON 字符串 , res转化为QByteArray
    QJsonDocument jsonDoc = QJsonDocument::fromJson(res.toUtf8());

    // JSON解析错误
    if(jsonDoc.isNull()){
        showTip(tr("json解析错误"),false);
        return;
    }

    // 调用回调函数
    _handlers[id](jsonDoc.object());
}


