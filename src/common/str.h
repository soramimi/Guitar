#pragma once

#include <QString>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace misc {

class str {
private:
	std::variant<std::string_view, QString> str_;
public:
	str(std::string_view sv) : str_(sv) {}
	str(std::vector<char> const &v) : str_(std::string_view(v.data(), v.size())) {}
	str(QString const &s) : str_(s) {}
	operator std::string() const
	{
		return std::visit([](auto &&arg) -> std::string {
			using T = std::decay_t<decltype(arg)>;
			if constexpr (std::is_same_v<T, std::string_view>) {
				return std::string(arg);
			} else if constexpr (std::is_same_v<T, QString>) {
				return arg.toStdString();
			}
			return {};
		}, str_);
	}
	operator std::vector<char>() const
	{
		return std::visit([](auto &&arg) -> std::vector<char> {
			using T = std::decay_t<decltype(arg)>;
			if constexpr (std::is_same_v<T, std::string_view>) {
				return std::vector<char>(arg.begin(), arg.end());
			} else if constexpr (std::is_same_v<T, QString>) {
				std::string s = arg.toStdString();
				return std::vector<char>(s.begin(), s.end());
			}
			return {};
		}, str_);
	}
	operator QString() const
	{
		return std::visit([](auto &&arg) -> QString {
			using T = std::decay_t<decltype(arg)>;
			if constexpr (std::is_same_v<T, std::string_view>) {
				return QString::fromStdString(std::string(arg));
			} else if constexpr (std::is_same_v<T, QString>) {
				return arg;
			}
			return {};
		}, str_);
	}
};

}

