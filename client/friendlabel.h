#ifndef FRIENDLABEL_H
#define FRIENDLABEL_H

#include <QFrame>
#include <QString>

namespace Ui {
class FriendLabel;
}

class FriendLabel : public QFrame
{
    Q_OBJECT
public:
    explicit FriendLabel(QWidget *parent = nullptr);
    ~FriendLabel();

    void SetText(const QString &text);
    int  Width()  const;
    int  Height() const;
    QString Text() const;

private:
    Ui::FriendLabel *ui;
    QString _text;
    int _width  = 0;
    int _height = 0;

private slots:
    void slot_close();

signals:
    void sig_close(QString text);
};

#endif // FRIENDLABEL_H