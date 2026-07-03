#ifndef TRIANGLES_QT_OUTLINEDLABEL_H
#define TRIANGLES_QT_OUTLINEDLABEL_H

#include <QLabel>

/**
 * QLabel that renders its text with an outline (stroke) in the
 * outline color, and a fill in the fill color. Used for the
 * "HD" badge in the status bar of the Triangles Qt wallet so
 * that each letter is outlined in the same red (#f26522) as
 * the triangle icons.
 *
 * Outline is drawn first (wide red pen), then the fill is drawn
 * on top (narrower pen, slightly inset). Both pens use the
 * same font/alignment as the parent label.
 */
class OutlinedLabel : public QLabel
{
    Q_OBJECT
    Q_PROPERTY(QColor outlineColor READ outlineColor WRITE setOutlineColor)
    Q_PROPERTY(int outlineWidth READ outlineWidth WRITE setOutlineWidth)

public:
    explicit OutlinedLabel(QWidget* parent = nullptr);

    QColor outlineColor() const { return m_outlineColor; }
    void setOutlineColor(const QColor& c);

    int outlineWidth() const { return m_outlineWidth; }
    void setOutlineWidth(int w);

protected:
    void paintEvent(QPaintEvent* e) override;

private:
    QColor m_outlineColor;
    int m_outlineWidth;
};

#endif // TRIANGLES_QT_OUTLINEDLABEL_H
