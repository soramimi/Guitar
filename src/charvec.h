
#ifndef CHARVEC_H_
#define CHARVEC_H_

#include <vector>
#include <string>

void vecprint(std::vector<char> *out, char c);
void vecprint(std::vector<char> *out, char const *begin, char const *end);
void vecprint(std::vector<char> *out, char const *ptr, size_t len);
void vecprint(std::vector<char> *out, char const *s);
void vecprint(std::vector<char> *out, std::string const &s);
void vecprint(std::vector<char> *out, std::vector<char> const *in);
std::string to_stdstr(std::vector<char> const &vec);

#endif
