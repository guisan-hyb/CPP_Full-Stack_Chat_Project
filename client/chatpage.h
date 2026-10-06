#ifndef CHATPAGE_H
#define CHATPAGE_H

#include <QWidget>

namespace Ui {
class ChatPage;
}

class ChatPage : public QWidget
{
    Q_OBJECT

public:
    explicit ChatPage(QWidget *parent = nullptr);
    ~ChatPage();

protected:
    // 注: 这里用protected是因为 -> paintEvent是由QWidget::event()内部调用的，所以用protected正好满足了可重写但不可直接调用的需求
    virtual void paintEvent(QPaintEvent *event) override;

private:
    Ui::ChatPage *ui;
};

#endif // CHATPAGE_H
