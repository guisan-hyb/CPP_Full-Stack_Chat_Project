#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    _login_dialog = new LoginDialog(this);
    _login_dialog->setWindowFlags(Qt::CustomizeWindowHint|Qt::FramelessWindowHint);
    setCentralWidget(_login_dialog);
    //_login_dialog->show();

    //创建和注册消息链接
    connect(_login_dialog,&LoginDialog::sig_switchRegister,this,&MainWindow::SlotSwitchReg);

    //连接登录界面忘记密码信号
    connect(_login_dialog,&LoginDialog::sig_switch_Reset,this,&MainWindow::SlotSwitchReset);
}

MainWindow::~MainWindow()
{
    delete ui;

    // if(_login_dialog){
    //     delete _login_dialog;
    //     _login_dialog = nullptr;
    // }

    // if(_reg_dialog){
    //     delete _reg_dialog;
    //     _reg_dialog = nullptr;
    // }
}

void MainWindow::SlotSwitchReg()
{
    _reg_dialog = new RegisterDialog(this);

    //隐藏窗口的默认系统边框和标题栏，创建一个“无边框窗口”
    _reg_dialog->setWindowFlags(Qt::CustomizeWindowHint|Qt::FramelessWindowHint);

    // 连接注册界面返回登录信号
    connect(_reg_dialog,&RegisterDialog::sigSwitchLogin,this,&MainWindow::SlotSwitchLogin);

    setCentralWidget(_reg_dialog);
    _login_dialog->hide();
    _reg_dialog->show();
}

void MainWindow::SlotSwitchLogin()
{
    _login_dialog = new LoginDialog(this);
    _login_dialog->setWindowFlags(Qt::CustomizeWindowHint|Qt::FramelessWindowHint);
    setCentralWidget(_login_dialog);

    _reg_dialog->hide();
    _login_dialog->show();

    //创建和注册消息链接
    connect(_login_dialog,&LoginDialog::sig_switchRegister,this,&MainWindow::SlotSwitchReg);
    //连接登录界面忘记密码信号
    connect(_login_dialog,&LoginDialog::sig_switch_Reset,this,&MainWindow::SlotSwitchReset);
}

void MainWindow::SlotSwitchReset()
{
    _reset_dialog = new ResetDialog(this);
    _reset_dialog->setWindowFlags(Qt::CustomizeWindowHint|Qt::FramelessWindowHint);
    setCentralWidget(_reset_dialog);

    _login_dialog->hide();
    _reset_dialog->show();

    // 连接返回登录信号和槽函数
    connect(_reset_dialog,&ResetDialog::sig_switchLogin,this,&MainWindow::SlotSwitchLoginFromReset);
}

void MainWindow::SlotSwitchLoginFromReset()
{
    _login_dialog = new LoginDialog(this);
    _login_dialog->setWindowFlags(Qt::CustomizeWindowHint|Qt::FramelessWindowHint);
    setCentralWidget(_login_dialog);

    _reset_dialog->hide();
    _login_dialog->show();

    //创建和注册消息链接
    connect(_login_dialog,&LoginDialog::sig_switchRegister,this,&MainWindow::SlotSwitchReg);
    //连接登录界面忘记密码信号
    connect(_login_dialog,&LoginDialog::sig_switch_Reset,this,&MainWindow::SlotSwitchReset);

}