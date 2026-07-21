
#ifndef GITUSER_H
#define GITUSER_H

#include <string>

struct GitUser {
	std::string name;
	std::string email;
	explicit operator bool () const;
};

#endif // GITUSER_H
