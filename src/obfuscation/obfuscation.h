#ifndef OBFUSCATION_H
#define OBFUSCATION_H

#include <QByteArray>
#include <vector>

namespace obfuscation {

QByteArray encode(QByteArray const &source, std::string_view magic_4bytes);
QByteArray decode(QByteArray const &encoded, std::string_view magic_4bytes);

}

#endif // OBFUSCATION_H
