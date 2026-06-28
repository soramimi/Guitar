#ifndef INCREMENTALSEARCHHELPER_H
#define INCREMENTALSEARCHHELPER_H

#include <string>
#include <optional>
#include <QString>
#include <memory>
#include <MyMecab.h>
#include <regex>

class QRect;
class QPainter;
class QStyleOptionViewItem;

class IncrementalSearchFilter;

// namespace incrementalsearch {
// class AbstractFilter;
// }


namespace incrementalsearch {

QString normalizeText(QString s);
void drawText(QPainter *painter, const QStyleOptionViewItem &opt, QRect r, const QString &text);
void drawText_filtered(QPainter *painter, QStyleOptionViewItem const &opt, QRect const &rect, const IncrementalSearchFilter &filter);
void fillFilteredBG(QPainter *painter, const QRect &rect);

static constexpr int ASCII_BACKSPACE = 0x08;
static constexpr int ASCII_DELETE = 0xff;
QString appendCharToFilterText(QString filter, QString const &add);

} // namespace incrementalsearch


#endif // INCREMENTALSEARCHHELPER_H
