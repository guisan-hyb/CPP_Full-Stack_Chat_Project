#ifndef CHATDIALOG_H
#define CHATDIALOG_H

#include <QDialog>
#include "global.h"
#include "statewidget.h"

namespace Ui {
class ChatDialog;
}

class ChatDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ChatDialog(QWidget *parent = nullptr);
    ~ChatDialog();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void handleGlobalMousePress(QMouseEvent *event) ;

private:
    void ShowSearch(bool b_search = false);
    void AddLBGroup(StateWidget* lb);
    void addChatUserList();
    void ClearLabelState(StateWidget* lb);

private:
    Ui::ChatDialog *ui;
    ChatUIMode _mode;
    ChatUIMode _state;
    bool _b_loading;
    QList<StateWidget*> _lb_list;

private slots:
    void slot_loading_chat_user();
    void slot_side_chat();
    void slot_side_contact();
    void slot_text_changed(const QString & str);
};

#endif // CHATDIALOG_H
