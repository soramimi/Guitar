#ifndef NPOS_H
#define NPOS_H

#include <string>
#include <limits>
#include <type_traits>

#if 0
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
#endif

class FOUND {
private:
	bool found_ = false;
public:
	explicit FOUND(std::string::size_type t)
	{
		found_ = (t != std::string::npos);
	}
	template <typename T, typename U> FOUND(T t, U const &u)
	{
		found_ = (t != u.end());
	}
	explicit operator bool () const
	{
		return found_;
	}
};

#endif // NPOS_H
