#ifndef EASYCRYPTO_H
#define EASYCRYPTO_H

#include <QByteArray>
#include <vector>

namespace easycrypto {

#if 0
QByteArray encode(QByteArray const &source, std::string_view magic_4bytes);
QByteArray decode(QByteArray const &encoded, std::string_view magic_4bytes);
#else
std::vector<char> encrypt(std::string const &key, std::string_view plain);
std::vector<char> decrypt(std::string const &key, std::string_view encrypted);
#endif

}

#endif // EASYCRYPTO_H
