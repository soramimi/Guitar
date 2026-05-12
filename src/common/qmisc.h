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

QStringList splitWords(QString const &text);

QString getApplicationDir();
void drawFrame(QPainter *pr, int x, int y, int w, int h, QColor color_topleft, QColor color_bottomright = QColor());
QString makeDateTimeString(const QDateTime &dt);
void setFixedSize(QWidget *w);
QPoint contextMenuPos(QWidget *w, QContextMenuEvent *e);
QString abbrevBranchName(QString const &name);
QString makeProxyServerURL(QString text);
QString collapseWhitespace(QString const &source);

} // namespace misc

#endif // QMISC_H
