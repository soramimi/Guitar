#ifndef EASYCRYPTO_H
#define EASYCRYPTO_H

#include <QByteArray>
#include <vector>

namespace easycrypto {

std::vector<char> encrypt(std::string const &key, std::string_view plain);
std::vector<char> decrypt(std::string const &key, std::string_view encrypted);

}

#endif // EASYCRYPTO_H
