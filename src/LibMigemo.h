#ifndef LIBMIGEMO_H
#define LIBMIGEMO_H

#include <string>
#include <optional>

class LibMigemo {
private:
	struct Private;
	Private *m;
public:
	LibMigemo();
	~LibMigemo();
	void init();
	bool open();
	void close();
	std::optional<std::string> queryMigemo(const char *word);

	static bool migemoEnabled();
	static std::string migemoDictDir();
	static std::string migemoDictPath();
	static bool setupMigemoDict();
	static void deleteMigemoDict();

	static LibMigemo *instance();

};

#endif // LIBMIGEMO_H
