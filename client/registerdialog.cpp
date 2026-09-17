#include "registerdialog.h"
#include "ui_registerdialog.h"
#include "global.h"
#include "httpmgr.h"


RegisterDialog::RegisterDialog(QWidget *parent)
    : QDialog(parent), _countdown(5)
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

    // 优化错误检查，在之后的输入框输入会检测之间输入框的正确性
    ui->err_tip->clear();
    connect(ui->user_edit,&QLineEdit::editingFinished,this,[this](){
        checkUserValid();
    });
    connect(ui->email_edit,&QLineEdit::editingFinished,this,[this](){
        checkEmailValid();
    });
    connect(ui->pass_edit,&QLineEdit::editingFinished,this,[this](){
        checkPassValid();
    });
    connect(ui->ack_edit,&QLineEdit::editingFinished,this,[this](){
        checkConfirmValid();
    });
    connect(ui->verify_edit,&QLineEdit::editingFinished,this,[this](){
        checkVerifyValid();
    });

    // 隐藏和显示密码
    ui->pass_visible->setCursor(Qt::PointingHandCursor);// 鼠标在该区域变成小手形状
    ui->ack_visible->setCursor(Qt::PointingHandCursor);

    ui->pass_visible->SetState("invisible","invisible_hover","","visible","visible_hover","");
    ui->ack_visible->SetState("invisible","invisible_hover","","visible","visible_hover","");

    connect(ui->pass_visible,&ClickedLabel::clicked,this,[this](){
        auto state = ui->pass_visible->GetCurState();
        if(state == ClickLbState::Normal){
            ui->pass_edit->setEchoMode(QLineEdit::Password);
        }else{
            ui->pass_edit->setEchoMode(QLineEdit::Normal);
        }
        qDebug() << "Label was clicked!";
    });
    connect(ui->ack_visible,&ClickedLabel::clicked,this,[this](){
        auto state = ui->ack_visible->GetCurState();
        if(state == ClickLbState::Normal){
            ui->ack_edit->setEchoMode(QLineEdit::Password);
        }else{
            ui->ack_edit->setEchoMode(QLineEdit::Normal);
        }
        qDebug() << "Label was clicked!";
    });


    // 创建定时器
    _countdown_timer = new QTimer(this);
    connect(_countdown_timer,&QTimer::timeout,[this](){
        if(_countdown==0){
            _countdown_timer->stop();
            emit sigSwitchLogin();
            return;
        }
        _countdown--;
        auto str = QString("注册成功, %1 s后返回登录").arg(_countdown);
        ui->tip_label->setText(str);
    });
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

    qDebug()<<email<<Qt::endl;

    if(match){
        //发送http验证码
        QJsonObject json_obj;
        json_obj["email"] = email;

        HttpMgr::GetInstance()->PostHttpReq(QUrl(gate_url_prefix+"/get_verifycode"),json_obj,
                                                 ReqId::ID_GET_VERIFY_CODE, Modules::MOD_REGISTER);

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

    // 注册 用户注册回包逻辑
    _handlers[ReqId::ID_REG_USER] = [this](const QJsonObject& jsonObj){
        int error = jsonObj["error"].toInt();
        if(error != ErrorCodes::SUCCESS){
            showTip(tr("参数错误"),false);
            return;
        }

        auto email = jsonObj["email"].toString();
        showTip(tr("用户注册成功"),true);
        qDebug()<<"user uuid is: "<<jsonObj["uid"].toString();
        qDebug()<<"email is: "<<email<<Qt::endl;

        // 切到下一页，显示成功待返回
        ChangeTipPage();
    };
}

void RegisterDialog::AddTipErr(TipErr te, QString tips) // 添加错误提示
{
    _tip_errs[te] = tips;
    showTip(tips,false);
}

void RegisterDialog::DelTipErr(TipErr te) // 删除错误提示
{
    _tip_errs.remove(te);
    if(_tip_errs.empty()){
        ui->err_tip->clear();
    }
}

bool RegisterDialog::checkUserValid()
{
    if(ui->user_edit->text() == ""){
        AddTipErr(TipErr::TIP_USER_ERR, tr("用户名不能为空"));
        return false;
    }
    DelTipErr(TipErr::TIP_USER_ERR);
    return true;
}

bool RegisterDialog::checkEmailValid()
{
    auto email = ui->email_edit->text();
    QRegularExpression regex(R"((\w+)(\.|_)?(\w*)@(\w+)(\.(\w+))+)");
    bool match = regex.match(email).hasMatch();

    if(!match){
        AddTipErr(TipErr::TIP_EMAIL_ERR, tr("邮箱地址不正确"));
        return false;
    }

    DelTipErr(TipErr::TIP_EMAIL_ERR);
    return true;
}

bool RegisterDialog::checkPassValid()
{
    auto pass = ui->pass_edit->text();
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

bool RegisterDialog::checkConfirmValid()
{
    auto ackpwd = ui->ack_edit->text();
    auto pwd = ui->pass_edit->text();
    if(ackpwd!=pwd){
        AddTipErr(TipErr::TIP_PWD_CONFIRM,tr("密码与确认密码不匹配"));
        return false;
    }

    DelTipErr(TipErr::TIP_PWD_CONFIRM);
    return true;
}

bool RegisterDialog::checkVerifyValid()
{
    if(ui->verify_edit->text().isEmpty()){
        AddTipErr(TipErr::TIP_VERIFY_ERR, tr("验证码不能为空"));
        return false;
    }

    DelTipErr(TipErr::TIP_VERIFY_ERR);
    return true;
}

void RegisterDialog::ChangeTipPage()
{
    _countdown_timer->stop();
    ui->stackedWidget->setCurrentWidget(ui->page_2);// 注册界面ui切换到第二页

    // 启动定时器
    _countdown_timer->start(1000);
}

// 确认注册
void RegisterDialog::on_confirm_btn_clicked()
{
    bool valid = checkUserValid();
    if(!valid){
        return;
    }

    valid = checkEmailValid();
    if(!valid){
        return;
    }

    valid = checkPassValid();
    if(!valid){
        return;
    }

    valid = checkVerifyValid();
    if(!valid){
        return;
    }

    // 发送http请求注册用户
    QJsonObject json_obj;
    json_obj["user"] = ui->user_edit->text();
    json_obj["email"] = ui->email_edit->text();
    json_obj["passwd"] = xorString(ui->pass_edit->text());
    json_obj["confirm"] = xorString(ui->ack_edit->text());
    json_obj["verifycode"] = ui->verify_edit->text();
    HttpMgr::GetInstance()->PostHttpReq(QUrl(gate_url_prefix+"/user_register"),
                                        json_obj,ReqId::ID_REG_USER,Modules::MOD_REGISTER);
}


void RegisterDialog::on_return_btn_clicked()
{
    _countdown_timer->stop();
    emit sigSwitchLogin();
}

