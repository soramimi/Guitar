#ifndef NPOS_H
#define NPOS_H

#include <limits>

struct NPOS_t {
	template <typename T>
	friend constexpr bool operator == (T x, NPOS_t)
	{
		return x == std::numeric_limits<T>::max();
	}

	template <typename T>
	friend constexpr bool operator != (T x, NPOS_t)
	{
		return x != std::numeric_limits<T>::max();
	}
};

inline constexpr NPOS_t NPOS;

class FOUND {
private:
	bool value = false;
public:
	template <typename T> FOUND(T t)
		 : value(t != NPOS)
	{
	}
	explicit operator bool () const
	{
		return value;
	}
};

#endif // NPOS_H
