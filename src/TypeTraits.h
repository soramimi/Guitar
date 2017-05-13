#ifndef TYPETRAITS_H
#define TYPETRAITS_H

#include <type_traits>

namespace type_traits {
	template <typename ...Dummy>
	using void_t = void;

	enum class return_any {};

	template <typename, typename R = void, typename = void> struct is_callable : std::false_type {};

	template <typename Fn, typename... ArgTypes, typename R>
	struct is_callable<Fn(ArgTypes...), R, type_traits::void_t<decltype(std::declval<Fn>()(std::declval<ArgTypes>()...))>>
		: std::is_convertible<decltype(std::declval<Fn>()(std::declval<ArgTypes>()...)), R> {};

	template <typename Fn, typename... ArgTypes>
	struct is_callable<Fn(ArgTypes...), return_any, type_traits::void_t<decltype(std::declval<Fn>()(std::declval<ArgTypes>()...))>>
		: std::true_type {};
}

#endif
