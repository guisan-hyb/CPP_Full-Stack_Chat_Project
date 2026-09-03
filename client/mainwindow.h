#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "logindialog.h"
#include "registerdialog.h"

/******************************************************************************
 *
 * @file       mainwindow.h
 * @brief      主窗口
 *
 * @author     guisan
 * @date       2026/09/03
 * @history
 *****************************************************************************/

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private:
    Ui::MainWindow *ui;
    LoginDialog* _login_dialog;
    RegisterDialog* _reg_dialog;

public slots:
    void SlotSwitchReg();
};
#endif // MAINWINDOW_H
