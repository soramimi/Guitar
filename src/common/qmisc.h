#ifndef QMISC_H
#define QMISC_H

#include <QColor>
#include <QPoint>
#include <QRect>

class QPainter;
class QWidget;
class QDateTime;
class QPalette;
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

void drawTextBadge(QPainter *painter, QPalette const &palette, QRect r, int space, QString const &text, QColor bgcolor, QColor fgcolor, bool bold);

} // namespace misc

#endif // QMISC_H
