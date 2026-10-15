#include "findfaildlg.h"
#include "ui_findfaildlg.h"

FindFailDlg::FindFailDlg(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::FindFailDlg)
{
    ui->setupUi(this);
    setWindowTitle("添加");
    setWindowFlags(windowFlags() | Qt::FramelessWindowHint);
    setObjectName("FindFailDlg");
    ui->fail_sure_btn->SetState("normal","hover","press");
    ui->horizontalLayout->setAlignment(Qt::AlignHCenter);// 按钮居中
    this->setModal(true);
}

FindFailDlg::~FindFailDlg()
{
    qDebug()<<"Find FailDlg destruct";
    delete ui;
}

void FindFailDlg::on_fail_sure_btn_clicked()
{
    this->hide();
}

