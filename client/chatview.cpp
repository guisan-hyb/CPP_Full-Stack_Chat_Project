#include "chatview.h"
#include "ui_chatview.h"

#include <QScrollBar>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QEvent>
#include <QStyleOption>
#include <QPainter>

ChatView::ChatView(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::ChatView)
    , isAppended(false)
{
    ui->setupUi(this);

    // 垂直滚动条：连接信号 + 把它放到右上角（不显示在原来并排的位置）
    QScrollBar *pVScrollBar = ui->chat_area->verticalScrollBar();
    connect(pVScrollBar, &QScrollBar::rangeChanged,
            this, &ChatView::onVScrollBarMoved);

    // 把垂直 ScrollBar 放到上边，而不是原来并排的位置
    QHBoxLayout *pHLayout_2 = new QHBoxLayout();
    pHLayout_2->addWidget(pVScrollBar, 0, Qt::AlignRight);
    pHLayout_2->setContentsMargins(0, 0, 0, 0);
    ui->chat_area->setLayout(pHLayout_2);
    pVScrollBar->setHidden(true);

    ui->chat_area->installEventFilter(this);

    initStyleSheet();
}

ChatView::~ChatView()
{
    delete ui;
}

void ChatView::appendChatItem(QWidget *item)
{
    QVBoxLayout *vl = qobject_cast<QVBoxLayout *>(ui->chat_bg->layout());
    qDebug() << "vl->count() is " << vl->count();
    vl->insertWidget(vl->count() - 1, item);   // 始终插在底部 Spacer 之前
    isAppended = true;
}

void ChatView::prependChatItem(QWidget *item)
{
    // TODO
}

void ChatView::insertChatItem(QWidget *before, QWidget *item)
{
    // TODO
}

void ChatView::removeAllItem()
{
    QVBoxLayout *layout = qobject_cast<QVBoxLayout *>(ui->chat_bg->layout());

    int count = layout->count();

    // 保留最后一个 Spacer，其余全部删除
    for (int i = 0; i < count - 1; ++i) {
        QLayoutItem *item = layout->takeAt(0);
        if (item) {
            if (QWidget *widget = item->widget()) {
                delete widget;
            }
            delete item;
        }
    }
}

bool ChatView::eventFilter(QObject *o, QEvent *e)
{
    if (e->type() == QEvent::Enter && o == ui->chat_area)
    {
        QScrollBar *sb = ui->chat_area->verticalScrollBar();
        sb->setHidden(sb->maximum() == 0);
    }
    else if (e->type() == QEvent::Leave && o == ui->chat_area)
    {
        ui->chat_area->verticalScrollBar()->setHidden(true);
    }
    return QWidget::eventFilter(o, e);
}

void ChatView::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QStyleOption opt;
    opt.initFrom(this);
    QPainter p(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
}

void ChatView::onVScrollBarMoved(int min, int max)
{
    Q_UNUSED(min);
    Q_UNUSED(max);
    if (isAppended) // 添加 item 可能调用多次
    {
        QScrollBar *pVScrollBar = ui->chat_area->verticalScrollBar();
        pVScrollBar->setSliderPosition(pVScrollBar->maximum());
        // 500ms 内可能被多次调用
        QTimer::singleShot(500, [this]() {
            isAppended = false;
        });
    }
}

void ChatView::initStyleSheet()
{
    // 与原来保持一致（这里原来也是空实现，样式表可直接在 .ui 里给 chat_area / chat_bg 设）
    // QScrollBar *scrollBar = ui->chat_area->verticalScrollBar();
    // scrollBar->setStyleSheet(...);
}