#include "TextBubble.h"
#include <QFontMetricsF>
#include <QDebug>
#include <QFont>
#include "global.h"
#include <QTimer>
#include <QTextDocument>
#include <QTextBlock>
#include <QTextLayout>
#include <algorithm>
#include <cmath>

TextBubble::TextBubble(ChatRole role, const QString &text, QWidget *parent)
    : BubbleFrame(role, parent)
{
    m_pTextEdit = new QTextEdit();
    m_pTextEdit->setReadOnly(true);
    m_pTextEdit->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_pTextEdit->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_pTextEdit->installEventFilter(this);

    QFont font("Microsoft YaHei");
    font.setPointSize(12);
    m_pTextEdit->setFont(font);

    setPlainText(text);
    setWidget(m_pTextEdit);
    initStyleSheet();
}

bool TextBubble::eventFilter(QObject *o, QEvent *e)
{
    if (m_pTextEdit == o && e->type() == QEvent::Paint)
    {
        adjustTextHeight(); // PaintEvent 中设置
    }
    return BubbleFrame::eventFilter(o, e);
}

void TextBubble::setPlainText(const QString &text)
{
    m_pTextEdit->setPlainText(text);
    // m_pTextEdit->setHtml(text);

    // 找到段落中最大宽度
    const qreal doc_margin = m_pTextEdit->document()->documentMargin();
    const int margin_left  = this->layout()->contentsMargins().left();
    const int margin_right = this->layout()->contentsMargins().right();

    QFontMetricsF fm(m_pTextEdit->font());
    QTextDocument *doc = m_pTextEdit->document();

    qreal max_width = 0;
    // 遍历每一段找到最宽的那一段
    for (QTextBlock it = doc->begin(); it != doc->end(); it = it.next())
    {
        // Qt6: QFontMetricsF::width() 已被 horizontalAdvance() 取代
        const qreal txtW = fm.horizontalAdvance(it.text());
        max_width = std::max(max_width, txtW);
    }

    // 设置这个气泡的最大宽度，只需要设置一次
    // 注意：horizontalAdvance() 返回的是小数（例如 46.39），
    // 如果直接 static_cast<int> 截断（46），气泡就会比文本真正需要的宽度窄不到 1px，
    // QTextEdit 会把最后一个字符（或者最后一个空格后面的内容）挤到下一行，
    // 高 DPI（125%/150% 缩放）下尤其明显。
    // 所以这里向上取整，并额外留 2px 余量，抵消 Qt 按设备像素取整字形宽度带来的误差。
    const qreal text_width = std::ceil(max_width) + 2;

    setMaximumWidth(static_cast<int>(text_width + doc_margin * 2) + margin_left + margin_right);
}

void TextBubble::adjustTextHeight()
{
    const qreal doc_margin = m_pTextEdit->document()->documentMargin(); // 字体到边框的距离默认为 4
    QTextDocument *doc = m_pTextEdit->document();

    qreal text_height = 0;
    // 把每一段的高度相加 = 文本高
    for (QTextBlock it = doc->begin(); it != doc->end(); it = it.next())
    {
        QTextLayout *pLayout = it.layout();
        if (!pLayout)
            continue;
        const QRectF text_rect = pLayout->boundingRect(); // 这段的 rect
        text_height += text_rect.height();
    }

    const int vMargin = this->layout()->contentsMargins().top();
    // 设置这个气泡需要的高度：文本高 + 文本边距 + TextEdit 边框到气泡边框的距离
    setFixedHeight(static_cast<int>(text_height + doc_margin * 2 + vMargin * 2));
}

void TextBubble::initStyleSheet()
{
    m_pTextEdit->setStyleSheet("QTextEdit{background:transparent;border:none}");
}

