#ifndef LOADINGDIALOG_H
#define LOADINGDIALOG_H

#include <QDialog>
#include <QMovie>

namespace Ui {
class LoadingDialog;
}

class LoadingDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LoadingDialog(QWidget *parent = nullptr);
    ~LoadingDialog();

protected:
    void showEvent(QShowEvent *event) override;// 显示时把自己摆到父窗口正中

private:
    Ui::LoadingDialog *ui;
    QMovie *_movie;
};

#endif // LOADINGDIALOG_H
