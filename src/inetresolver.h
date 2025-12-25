#ifndef INETRESOLVER_H
#define INETRESOLVER_H

#include <vector>
#include <string>

class InetResolver {
public:
	enum Type {
		UNDEFINED,
		IN4,
		IN6,
	};
	typedef void _in_addr;
	typedef void _in6_addr;
	struct Addr {
		Type type = UNDEFINED;
		std::vector<std::vector<char>> addr;
		size_t size() const
		{
			return addr.size();
		}
		bool empty() const
		{
			return size() == 0;
		}
		operator bool () const
		{
			return !empty();
		}
		_in_addr const *to_in4(size_t i) const
		{
			return reinterpret_cast<_in_addr const *>(addr[i].data());
		}
		_in6_addr const *to_in6(size_t i) const
		{
			return reinterpret_cast<_in6_addr const *>(addr[i].data());
		}
		std::string to_string(size_t i = 0) const;
	};
	bool resolve(char const *name, Type type, Addr *out);
};


#endif // INETRESOLVER_H
