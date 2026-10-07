#ifndef CHATVIEW_H
#define CHATVIEW_H

#include <QWidget>
#include <QTimer>

QT_BEGIN_NAMESPACE
namespace Ui { class ChatView; }
QT_END_NAMESPACE

class ChatView : public QWidget
{
    Q_OBJECT
public:
    explicit ChatView(QWidget* parent = nullptr);
    ~ChatView() override;

    void appendChatItem(QWidget *item);                 // 尾插
    void prependChatItem(QWidget *item);                // 头插
    void insertChatItem(QWidget *before, QWidget *item);// 中间插
    void removeAllItem();

protected:
    bool eventFilter(QObject *o, QEvent *e) override;
    void paintEvent(QPaintEvent *event) override;

private slots:
    void onVScrollBarMoved(int min, int max);

private:
    void initStyleSheet();

private:
    Ui::ChatView *ui;
    bool isAppended;
};

#endif // CHATVIEW_H