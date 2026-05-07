#pragma once

#include <QString>
#include <QDebug>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace misc {

class str {
private:
	std::variant<std::string_view, QString> str_;
public:
	explicit str(char const *s) : str_(std::string_view(s)) {}
	explicit str(char const *s, size_t n) : str_(std::string_view(s, n)) {}
	explicit str(std::string_view sv) : str_(sv) {}
	explicit str(std::vector<char> const &v) : str_(std::string_view(v.data(), v.size())) {}
	explicit str(QString const &s) : str_(s) {}
	explicit str(QByteArray const &ba) : str_(std::string_view(ba.data(), ba.size())) {}
	bool empty() const
	{
		return std::visit([](auto &&arg) -> bool {
			using T = std::decay_t<decltype(arg)>;
			if constexpr (std::is_same_v<T, std::string_view>) {
				return arg.empty();
			} else if constexpr (std::is_same_v<T, QString>) {
				return arg.isEmpty();
			}
			return true;
		}, str_);
	}
	bool isEmpty() const
	{
		return empty();
	}
	explicit operator bool () const
	{
		return !empty();
	}
	std::string std_string() const
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
	std::string_view std_string_view() const
	{
		return std::visit([](auto &&arg) -> std::string_view {
			using T = std::decay_t<decltype(arg)>;
			if constexpr (std::is_same_v<T, std::string_view>) {
				return arg;
			} else if constexpr (std::is_same_v<T, QString>) {
#if 0
				std::string s = arg.toStdString();
				return std::string_view(s);
#endif
				// Q. 上のコードの s の寿命は？
				// A. ここでは問題ない。なぜなら、QString::toStdString() は QString の内容をコピーして新しい std::string を作成するため、s はこの関数内で有効であり、返された std::string_view は s の内容を指すことになるから。ただし、この std::string_view を呼び出し元で使用する場合は、s がスコープ外になるため、呼び出し元で s を保持する必要がある。
				// Q. この関数から抜けた時点で、s は破棄されるのでは？
				// A. その通りです。上のコードでは、s はこのラムダ関数内で定義されているため、ラムダ関数が終了すると s は破棄されます。そのため、返された std::string_view は s の内容を指すことになりますが、s が破棄された後は、その std::string_view は不定の動作を引き起こす可能性があります。したがって、このコードは安全ではありません。
				qDebug() << "Warning: Converting QString to std::string_view may lead to dangling reference. Consider using std::string instead.";
				Q_ASSERT(0);
			}
			return {};
		}, str_);
	}
	std::vector<char> std_vector_char() const
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
	QString qstring() const
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
	QByteArray qbytearray() const
	{
		return std::visit([](auto &&arg) -> QByteArray {
			using T = std::decay_t<decltype(arg)>;
			if constexpr (std::is_same_v<T, std::string_view>) {
				return QByteArray(arg.data(), arg.size());
			} else if constexpr (std::is_same_v<T, QString>) {
				return arg.toUtf8();
			}
			return {};
		}, str_);
	}
	operator std::string() const
	{
		return std_string();
	}
	operator std::string_view() const
	{
		return std_string_view();
	}
	operator std::vector<char>() const
	{
		return std_vector_char();
	}
	operator QString() const
	{
		return qstring();
	}
	/**
	 * @warning QByteArray への暗黙の変換演算子があると ambiguous overload for ‘operator=’エラーが出るので、この変換演算子は提供しない。
	 */
#if 0
	operator QByteArray() const
	{
		return qbytearray();
	}
#endif
};

}

