#include "mainwindow.h"

#include <QApplication>
#include <QFile>
#include <QDebug>
#include "global.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // 配置qss
    QFile qss(":/style/style_sheet.qss");
    if(qss.open(QFile::ReadOnly)){
        qDebug("open success");
        QString style = QLatin1String(qss.readAll());
        a.setStyleSheet(style);
        qss.close();
    }else{
        qDebug("Open Failed");
    }

    // 配置config
    QString fileName = "config.ini";
    QString app_path = QCoreApplication::applicationDirPath();// 获取执行路径
    QString config_path = QDir::toNativeSeparators(app_path + QDir::separator() + fileName);
    QSettings settings(config_path,QSettings::IniFormat);
    // 读取config
    QString gate_host = settings.value("GateServer/host").toString();
    QString gate_port = settings.value("GateServer/port").toString();
    gate_url_prefix = "http://"+gate_host+":"+gate_port;
    qDebug()<<gate_url_prefix<<Qt::endl;

    MainWindow w;
    w.show();
    return QCoreApplication::exec();
}
