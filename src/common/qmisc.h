#ifndef QMISC_H
#define QMISC_H

#include <QColor>
// #include <QDateTime>
#include <QPoint>

class QPainter;
class QWidget;
class QDateTime;
class QContextMenuEvent;

namespace misc {

QString getApplicationDir();
void drawFrame(QPainter *pr, int x, int y, int w, int h, QColor color_topleft, QColor color_bottomright = QColor());
QString makeDateTimeString(const QDateTime &dt);
void setFixedSize(QWidget *w);
QPoint contextMenuPos(QWidget *w, QContextMenuEvent *e);

} // namespace misc

#endif // QMISC_H
