#include "friendlabel.h"
#include "ui_friendlabel.h"
#include <QDebug>
#include <QFontMetrics>

FriendLabel::FriendLabel(QWidget *parent) :
    QFrame(parent),
    ui(new Ui::FriendLabel)
{
    ui->setupUi(this);

    ui->close_lb->SetState("normal", "hover", "pressed",
                           "selected_normal", "selected_hover", "selected_pressed");

    connect(ui->close_lb, &ClickedLabel::clicked, this, &FriendLabel::slot_close);
}

FriendLabel::~FriendLabel()
{
    delete ui;
}

void FriendLabel::SetText(const QString &text)
{
    _text = text;
    ui->tip_lb->setText(_text);
    ui->tip_lb->adjustSize();

    // Qt6：使用 QFontMetrics::horizontalAdvance() 替代已废弃/移除的 width()
    const QFontMetrics fontMetrics(ui->tip_lb->font());
    const int textWidth  = fontMetrics.horizontalAdvance(ui->tip_lb->text());
    const int textHeight = fontMetrics.height();

    qDebug() << " ui->tip_lb.width() is "  << ui->tip_lb->width();
    qDebug() << " ui->close_lb->width() is " << ui->close_lb->width();
    qDebug() << " textWidth is " << textWidth;

    this->setFixedWidth(ui->tip_lb->width() + ui->close_lb->width() + 5);
    this->setFixedHeight(textHeight + 2);

    qDebug() << "  this->setFixedHeight " << this->height();

    _width  = this->width();
    _height = this->height();
}

int FriendLabel::Width() const
{
    return _width;
}

int FriendLabel::Height() const
{
    return _height;
}

QString FriendLabel::Text() const
{
    return _text;
}

void FriendLabel::slot_close()
{
    emit sig_close(_text);
}

