#include "loadingdialog.h"
#include "ui_loadingdialog.h"
#include <QMovie>

LoadingDialog::LoadingDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::LoadingDialog)
    , _movie(nullptr)
{
    ui->setupUi(this);

    setWindowFlags(Qt::Dialog|Qt::FramelessWindowHint|Qt::WindowSystemMenuHint|Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_TranslucentBackground);// 设置透明背景

    // 只留一个刚好装下动画的小方框。
    // 原来是 setFixedSize(parent->size())，对话框会被撑成父窗口那么大，
    // 而搜索列表又高又窄，于是加载动画背后多了一个竖直的大矩形。
    setFixedSize(120, 120);
    // 动画label正好100x100(动画本身就是100x100的gif)并居中显示
    ui->verticalLayout->setContentsMargins(10, 10, 10, 10);
    ui->loading_lb->setAlignment(Qt::AlignCenter);

    _movie = new QMovie(":/res/loading.gif", QByteArray(), ui->loading_lb);// 加载动画的资源文件
    ui->loading_lb->setMovie(_movie);
    _movie->start();
}

LoadingDialog::~LoadingDialog()
{
    delete ui;
}

void LoadingDialog::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);

    // 居中显示在父窗口上，而不是跑到屏幕角落
    QWidget* parent = parentWidget();
    if(!parent){
        return;
    }

    QPoint center = parent->mapToGlobal(parent->rect().center());
    move(center.x() - width() / 2, center.y() - height() / 2);
}
