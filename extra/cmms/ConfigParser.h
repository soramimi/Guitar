#ifndef CONFIGPARSER_H
#define CONFIGPARSER_H

#include <functional>
#include <string>

class ConfigParser {
private:
	typedef std::function<void (std::string const &section, std::string const &key, std::string const &value, void *cookie)> fn_assign_t;
	fn_assign_t fn_assign;
public:
	static bool parse(const char *file, fn_assign_t fn_assign, void *cookie);

	static void example();
};

#endif // CONFIGPARSER_H
