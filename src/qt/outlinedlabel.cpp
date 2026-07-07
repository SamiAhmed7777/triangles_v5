#include "outlinedlabel.h"

#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QStyleOption>
#include <QTextDocument>
#include <QString>

OutlinedLabel::OutlinedLabel(QWidget* parent)
    : QLabel(parent)
    , m_outlineColor(QColor("#e32105"))
    , m_outlineWidth(3)
{
    // OutlinedLabel is always styled; do not let QSS override our paint.
    setAttribute(Qt::WA_OpaquePaintEvent, false);
}

void OutlinedLabel::setOutlineColor(const QColor& c)
{
    if (m_outlineColor == c) return;
    m_outlineColor = c;
    update();
}

void OutlinedLabel::setOutlineWidth(int w)
{
    if (m_outlineWidth == w) return;
    m_outlineWidth = w;
    update();
}

void OutlinedLabel::paintEvent(QPaintEvent* e)
{
    Q_UNUSED(e);

    // Honor any background styling the parent may have given us, but
    // do our own text rendering below. We deliberately skip QLabel's
    // built-in drawContents/drawText path because it cannot paint a
    // per-character outline.
    QStyleOption opt;
    opt.initFrom(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, nullptr, this);

    if (text().isEmpty()) return;

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    const QFontMetricsF fm(font());
    const QString t = text();
    // Bounding rect for the text, honoring alignment. Add half the
    // outline width on each side so strokes don't clip against the
    // widget edge.
    const qreal pad = m_outlineWidth / 2.0;
    QRectF r = rect().adjusted(pad, pad, -pad, -pad);

    // Center vertically based on font metrics
    const qreal yOffset = (r.height() - fm.height()) / 2.0;
    QPointF baseline(r.left(), r.top() + yOffset + fm.ascent());

    // Align: use only the horizontal part of the alignment flag.
    const int align = int(alignment() & (Qt::AlignLeft | Qt::AlignRight | Qt::AlignHCenter | Qt::AlignJustify));
    const qreal textWidth = fm.horizontalAdvance(t);
    qreal x = r.left();
    if (align & Qt::AlignHCenter) {
        x = r.left() + (r.width() - textWidth) / 2.0;
    } else if (align & Qt::AlignRight) {
        x = r.right() - textWidth;
    }
    baseline.setX(x);

    QPainterPath path;
    path.addText(baseline, font(), t);

    // Stroke (outline) — drawn first, in the brand red so each letter
    // has a clear 3px red border matching the triangle icons.
    QPen outlinePen(m_outlineColor);
    outlinePen.setWidth(m_outlineWidth);
    outlinePen.setJoinStyle(Qt::RoundJoin);
    outlinePen.setCapStyle(Qt::RoundCap);
    painter.setPen(outlinePen);
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(path);

    // Fill the interior with the widget background color so the
    // letters read as hollow red outlines against the dark wallet
    // background, like the triangle icons beside them.
    painter.setPen(Qt::NoPen);
    painter.setBrush(QBrush(palette().color(backgroundRole())));
    painter.drawPath(path);
}
